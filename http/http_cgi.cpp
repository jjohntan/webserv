#include "http_cgi.hpp"
#include "HTTPResponse/HTTPResponse.hpp"
#include <fstream>
#include <dirent.h>
#include <sstream>
#include <iostream>
#include "../Server.hpp" // [ADD] need full type for srv.queueResponse

// // Enforce allowed_methods, redirects, error pages, 413 limit, and static DELETE
// // add near top of file (helpers)
// static const Location* getMatchingLocation(const std::string& path, const ServerConfig* sc)
// {
// 	if (!sc) return NULL;
// 	const Location* best = NULL;
// 	size_t best_len = 0;
// 	for (size_t i = 0; i < sc->locations.size(); ++i) {
// 		const Location& L = sc->locations[i];
// 		if (path.find(L.path) == 0 && L.path.length() > best_len) {
// 			best = &L;
// 			best_len = L.path.length();
// 		}
// 	}
// 	return best;
// }

// static bool methodAllowed(const HTTPRequest& req, const Location* L)
// {
// 	if (!L || L->allowed_methods.empty()) return true; // default allow if not configured
// 	for (size_t i = 0; i < L->allowed_methods.size(); ++i) {
// 		if (req.getMethod() == L->allowed_methods[i]) return true;
// 	}
// 	return false;
// }

// Try to load configured error page; if not found, fall back to simple body
static std::string loadErrorPageBody(int code, const ServerConfig* sc)
{
	// Try config-mapped page
	if (sc) {
		std::map<int, std::string>::const_iterator it = sc->error_pages.find(code);
		if (it != sc->error_pages.end()) {
			std::string mapped = it->second;
			// If it's a URI like "/error/404.html", map to server root
			if (!mapped.empty() && mapped[0] == '/' && !sc->root.empty()) {  // [CHANGE]
				std::string fs = sc->root + mapped;                          // [CHANGE]
				std::ifstream f(fs.c_str(), std::ios::in | std::ios::binary);
				if (f) {
					std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
					return content;
				}
			} else {
				// Treat as filesystem path as-is
				std::ifstream f(mapped.c_str(), std::ios::in | std::ios::binary);
				if (f) {
					std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
					return content;
				}
			}
		}
	}
	// Fallback page
	std::ostringstream os;
	os << "<html><body><h1>" << code << "</h1></body></html>";
	return os.str();
}

static void sendError( int code, const std::string& message, int socketFD, const ServerConfig* sc, const HTTPRequest* req, Server& srv) // [CHANGE]
{
	std::string body = loadErrorPageBody(code, sc);
	std::ostringstream len; len << body.size();
	std::string full = "Content-Type: text/html\r\n";
	if (req)
		full += req->connectionHeader(req->isConnectionAlive());
	else
		full += "Connection: close\r\n";
	full += "\r\n" + body;
	HTTPResponse resp(message, code, full, socketFD);
	srv.queueResponse(socketFD, resp.getRawResponse()); // [CHANGE]
}
/* --------------------------------------------------------------------------------------------------------------------------------*/

// CGI call function
CGIResult runCGI(const HTTPRequest& request, const std::string& script_path, const std::map<std::string, std::string>& cgi_extensions, const std::string& working_directory)
{
	CGIHandler cgi_handler;
	return cgi_handler.executeCGI(request, script_path, cgi_extensions, working_directory);
}

// read static files structure
std::string serveFile(const std::string& filePath)
{
	std::ifstream file(filePath.c_str());
	if (!file.is_open()) {
		return ""; // File not found
	}
	
	std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	file.close();
	return content;
}

// Directory listing autoindex
std::string generateDirectoryListing(const std::string& dirPath)
{
	std::string html = "<!DOCTYPE html>\n<html>\n<head>\n<title>Directory Listing</title>\n";
	html += "<style>body{font-family:Arial,sans-serif;margin:20px;}h1{color:#333;}ul{list-style:none;padding:0;}";
	html += "li{margin:5px 0;}a{text-decoration:none;color:#0066cc;}a:hover{text-decoration:underline;}</style>\n";
	html += "</head>\n<body>\n<h1>Directory Listing</h1>\n<ul>\n";
	
	// Add parent directory link
	html += "<li><a href=\"../\">../</a></li>\n";
	
	// List directory contents
	DIR* dir = opendir(dirPath.c_str());
	if (dir != NULL) {
		struct dirent* entry;
		while ((entry = readdir(dir)) != NULL) {
			std::string name = entry->d_name;
			if (name != "." && name != "..") {
				html += "<li><a href=\"" + name;
				if (entry->d_type == DT_DIR) {
					html += "/";
				}
				html += "\">" + name;
				if (entry->d_type == DT_DIR) {
					html += "/";
				}
				html += "</a></li>\n";
			}
		}
		closedir(dir);
	}
	
	html += "</ul>\n</body>\n</html>";
	return html;
}

// To check whether http request to the correct server configuration between different port
/*
example : Function Execution:
// 1. host = "localhost:8081"
// 2. port_str = "8081" 
// 3. port = 8081
// 4. Loop finds servers[1] with port 8081
// 5. Returns: &servers[1] (pointer to the 8081 server config)

*/
const ServerConfig* findServerConfig(const HTTPRequest& request, const std::vector<ServerConfig>& servers) {
	
    const std::map<std::string, std::string>& headers = request.getHeaderMap();
	std::map<std::string, std::string>::const_iterator host_it = headers.find("host");
	std::string host = (host_it != headers.end()) ? host_it->second : "";
	
	// Extract port from host header (e.g., "localhost:8081" -> "8081")
	size_t colon_pos = host.find(':');
	std::string port_str = (colon_pos != std::string::npos) ? host.substr(colon_pos + 1) : "80";
	
	// Convert port string to int
	int port = atoi(port_str.c_str());
	
	// Find matching server configuration
	for (size_t i = 0; i < servers.size(); ++i) {
		if (servers[i].port == port) {
			return &servers[i];
		}
	}
	
	return NULL; // No matching server found
}

// Check whether CGI is enabled for a specific path by finding the most specific location match (from config file)
bool isCGIEnabled(const std::string& path, const ServerConfig* server_config) {
	if (!server_config) {
		return false;
	}
	
	// Find the most specific location match (longest matching path)
	const Location* best_match = NULL;
	size_t best_match_length = 0;
	
	// Check each location in the server configuration
	for (size_t i = 0; i < server_config->locations.size(); ++i) {
		const Location& location = server_config->locations[i];
		
		// Check if path matches this location
		if (path.find(location.path) == 0) {
			// Check if this is a more specific match (longer path)
			if (location.path.length() > best_match_length) {
				best_match = &location;
				best_match_length = location.path.length();
			}
		}
	}
	
	if (best_match) {
		bool has_cgi = !best_match->cgi_extensions.empty();
		std::cout << "CGI Status - Path: " << path << " | CGI Enabled: " << (has_cgi ? "YES" : "NO") << std::endl;
		return has_cgi;
	}
	
	return false;
}

// Helper function to get CGI extensions for a path
std::map<std::string, std::string> getCGIExtensions(const std::string& path, const ServerConfig* server_config) {
	std::map<std::string, std::string> cgi_extensions;
	
	if (!server_config) return cgi_extensions;
	
	// Find the most specific location match (longest matching path)
	const Location* best_match = NULL;
	size_t best_match_length = 0;
	
	// Find the matching location
	for (size_t i = 0; i < server_config->locations.size(); ++i) {
		const Location& location = server_config->locations[i];
		
		// Check if path matches this location
		if (path.find(location.path) == 0) {
			// Check if this is a more specific match (longer path)
			if (location.path.length() > best_match_length) {
				best_match = &location;
				best_match_length = location.path.length();
			}
		}
	}
	
	if (best_match) {
		cgi_extensions = best_match->cgi_extensions;
	}
	
	return cgi_extensions;
}

// Main function to handle CGI checking and file serving
void handleRequestProcessing(const HTTPRequest& request, int socketFD, const std::vector<ServerConfig>& servers, Server& srv) // [CHANGE]
{
	std::string path = request.getPath();

	// Server selection by Host:port
	const ServerConfig* server_config = findServerConfig(request, servers);

	// Pick the most specific location once
	const Location* matching_location = getMatchingLocation(path, server_config); // [CHANGE]

	// Decide CGI vs Static
	bool cgi_enabled = isCGIEnabled(path, server_config);
	if (cgi_enabled) {
		std::map<std::string, std::string> cgi_extensions = getCGIExtensions(path, server_config);
		CGIHandler cgi_handler;
		if (cgi_handler.needsCGI(path, cgi_extensions)) {
			// Map /cgi_bin/* → ./cgi_bin/*
			std::string script_path = path;
			std::string working_directory = "./";
			if (path.find("/cgi_bin/") == 0) {
				script_path = "./cgi_bin" + path.substr(8);
				working_directory = "./";
			}

			// Execute CGI
			std::cout << "Executing CGI Script: " << script_path << std::endl;
			CGIResult cgi_result = runCGI(request, script_path, cgi_extensions, working_directory);
			HTTPResponse response(cgi_result.status_message, cgi_result.status_code, cgi_result.content, socketFD);
			std::cout << "Queue CGI Response\n";
			srv.queueResponse(socketFD, response.getRawResponse()); // [CHANGE]
			return;
		}
		// fall-through to static if needsCGI()==false
	}

	// -------- Static serving (also handles DELETE) --------

	// Effective root (location root overrides server root)
	std::string server_root = server_config ? server_config->root : "pages/www";  // [CHANGE]
	if (matching_location && !matching_location->root.empty())
		server_root = matching_location->root;

	// Per-location index for "/" paths
	std::string filePath;                                                         // [CHANGE]
	if (path == "/" || path.empty()) {
		std::string indexName = (matching_location && !matching_location->index.empty())
								? matching_location->index : "index.html";
		filePath = server_root + "/" + indexName;
	} else {
		filePath = server_root + path;
	}

	// Static DELETE (only files, not directories)
	if (request.getMethod() == "DELETE") {
		struct stat st;
		if (stat(filePath.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
			if (std::remove(filePath.c_str()) == 0) {
				std::string content = request.connectionHeader(request.isConnectionAlive()) + "\r\n";
				HTTPResponse resp("No Content", 204, content, socketFD);
				srv.queueResponse(socketFD, resp.getRawResponse()); // [CHANGE]
			}
			else
				sendError(403, "Forbidden", socketFD, server_config, &request, srv); // [CHANGE]
		}
		else
			sendError(404, "Not Found", socketFD, server_config, &request, srv);     // [CHANGE]
		return ;
	}

	// Directory request → only list on GET (others fall through)
	if (request.getMethod() == "GET" && !filePath.empty() && filePath[filePath.length() - 1] == '/') {
		bool autoindex_enabled = (matching_location ? matching_location->autoindex : false);
		if (autoindex_enabled) {
			std::string dirListing = generateDirectoryListing(filePath);
			std::ostringstream contentLengthStream; contentLengthStream << dirListing.length();
			std::string responseContent = "Content-Type: text/html\r\n"
										+ request.connectionHeader(request.isConnectionAlive()) + "\r\n"
										+ dirListing;
			HTTPResponse response("OK", 200, responseContent, socketFD);
			srv.queueResponse(socketFD, response.getRawResponse()); // [CHANGE]
		}
		else
			sendError(403, "Forbidden", socketFD, server_config, &request, srv);     // [CHANGE]
		return;
	}

	// Regular file
	std::string content = serveFile(filePath);
	if (content.empty()) {
		sendError(404, "Not Found", socketFD, server_config, &request, srv);         // [CHANGE]
		return;
	}

	// Static Files
	std::string contentType = "text/html";
	if (filePath.find(".css") != std::string::npos) contentType = "text/css";
	else if (filePath.find(".js")  != std::string::npos) contentType = "application/javascript";
	else if (filePath.find(".jpg") != std::string::npos || filePath.find(".jpeg") != std::string::npos) contentType = "image/jpeg";
	else if (filePath.find(".png") != std::string::npos) contentType = "image/png";

	std::ostringstream contentLengthStream; contentLengthStream << content.length();
	std::string responseContent = "Content-Type: " + contentType + "\r\n"
								+ request.connectionHeader(request.isConnectionAlive()) + "\r\n"
								+ content;
	HTTPResponse response("OK", 200, responseContent, socketFD);
	srv.queueResponse(socketFD, response.getRawResponse()); // [CHANGE]
}
