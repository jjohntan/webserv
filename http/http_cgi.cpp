#include "http_cgi.hpp"
#include "HTTPResponse/HTTPResponse.hpp"
#include <fstream>
#include <dirent.h>
#include <sstream>
#include <iostream>
#include "../Server.hpp" // [ADD] need full type for srv.queueResponse

// [ADD] Build headers for HEAD (no body) with an explicit Content-Length.
static std::string buildHeadOnlyHeaders(const std::string& contentType,
										size_t contentLen,
										const HTTPRequest& req)
{
	std::ostringstream out;
	out << "Content-Type: " << contentType << "\r\n"
		<< "Content-Length: " << contentLen << "\r\n"
		<< req.connectionHeader(req.isConnectionAlive())
		<< "\r\n"; // end of headers; no body
	return out.str();
}

// [ADD] Remove any leading "Status:" header from CGI output (we supply status line).
static void stripCgiStatusHeader(std::string& headersAndBody)
{
	// Only look in the header section before the first CRLFCRLF
	size_t hdr_end = headersAndBody.find("\r\n\r\n");
	if (hdr_end == std::string::npos) return;
	std::string header = headersAndBody.substr(0, hdr_end);
	std::string body   = headersAndBody.substr(hdr_end + 4);

	std::istringstream hs(header);
	std::ostringstream newHdr;
	std::string line;
	while (std::getline(hs, line)) {
		if (!line.empty() && line[line.size()-1]=='\r') line.erase(line.size()-1,1);
		std::string lower = line;
		for (size_t i=0;i<lower.size();++i) lower[i] = std::tolower((unsigned char)lower[i]);
		if (lower.rfind("status:", 0) == 0) {
			// drop this line
			continue;
		}
		newHdr << line << "\r\n";
	}
	headersAndBody = newHdr.str() + "\r\n" + body;
}

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

// case-insensitive header fetch
std::string getHeaderCI(const std::map<std::string,std::string>& h, const std::string& key) {
	for (std::map<std::string,std::string>::const_iterator it=h.begin(); it!=h.end(); ++it) {
		if (it->first.size()==key.size()) {
			bool eq=true;
			for (size_t i=0;i<key.size();++i) {
				if (std::tolower((unsigned char)it->first[i]) != std::tolower((unsigned char)key[i])) { eq=false; break; }
			}
			if (eq) return it->second;
		}
	}
	return "";
}

std::string joinPath(const std::string& dir, const std::string& base) {
	if (dir.empty()) return base;
	if (!dir.empty() && (dir[dir.size()-1]=='/' || dir[dir.size()-1]=='\\'))
		return dir + base;
	return dir + "/" + base;
}

// crude filename sanitization: drop any path parts
static std::string basenameOnly(const std::string& name) {
	size_t p = name.find_last_of("/\\");
	if (p != std::string::npos) return name.substr(p+1);
	return name;
}

static bool ensureDirExists(const std::string& pathDir) {
	struct stat st;
	if (stat(pathDir.c_str(), &st) == 0) return S_ISDIR(st.st_mode);
	// try to create (posix)
	return mkdir(pathDir.c_str(), 0755) == 0;
}

static bool parseMultipartAndExtract(const std::string& body,
									const std::string& boundary,
									std::string& filename,
									std::string& filedata)
{
	// Boundary comes like: "----abcdef"; the payload uses "--boundary"
	std::string b = "--" + boundary;
	// find first part
	size_t pos = body.find(b);
	if (pos == std::string::npos) return false;
	pos += b.size();
	if (pos+2 > body.size()) return false;
	if (body.compare(pos, 2, "\r\n")==0) pos += 2;

	// headers of this part until CRLFCRLF
	size_t hdr_end = body.find("\r\n\r\n", pos);
	if (hdr_end == std::string::npos) return false;
	std::string part_headers = body.substr(pos, hdr_end - pos);

	// Content-Disposition: form-data; name="file"; filename="xxx"
	{
		std::string key = "content-disposition:";
		std::istringstream hs(part_headers);
		std::string line;
		while (std::getline(hs, line)) {
			if (!line.empty() && line[line.size()-1]=='\r') line.erase(line.size()-1,1);
			std::string lower=line;
			for (size_t i=0;i<lower.size();++i) lower[i]=std::tolower((unsigned char)lower[i]);
			if (lower.find(key)==0) {
				// naive filename="
				size_t fnp = line.find("filename=");
				if (fnp != std::string::npos) {
					size_t q1 = line.find('"', fnp);
					size_t q2 = (q1==std::string::npos) ? std::string::npos : line.find('"', q1+1);
					if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1+1) {
						filename = line.substr(q1+1, q2-q1-1);
					}
				}
				break;
			}
		}
	}
	if (filename.empty()) return false;

	size_t data_start = hdr_end + 4; // skip CRLFCRLF
	// data ends at \r\n--boundary
	size_t term = body.find("\r\n" + b, data_start);
	if (term == std::string::npos) return false;

	filedata.assign(body.data()+data_start, term - data_start);
	return true;
}

bool saveUploadedBody(const HTTPRequest& req,
					const Location* loc,
					const ServerConfig* sc,
					std::string& saved_path,
					std::string& error)
{
	// choose target dir
	std::string dest_dir;
	if (loc && !loc->upload_path.empty()) dest_dir = loc->upload_path;
	else if (sc && !sc->root.empty())     dest_dir = joinPath(sc->root, "upload");
	else                                  dest_dir = "./upload";

	if (!ensureDirExists(dest_dir)) {
		error = "upload destination directory does not exist and could not be created";
		return false;
	}

	const std::map<std::string,std::string>& H = req.getHeaderMap();
	std::string ctype = getHeaderCI(H, "content-type");
	std::string raw   = req.getRawBody();            // full body
	const std::vector<char>& bodyv = req.getBodyVector();

	std::string filename, data;

	if (ctype.find("multipart/form-data") != std::string::npos) {
		// extract boundary=...
		size_t bp = ctype.find("boundary=");
		if (bp == std::string::npos) { error="multipart with no boundary"; return false; }
		std::string boundary = ctype.substr(bp+9);
		// trim quotes if any
		if (!boundary.empty() && boundary[0]=='"') {
			size_t q2 = boundary.find('"',1);
			boundary = (q2==std::string::npos) ? boundary.substr(1) : boundary.substr(1, q2-1);
		}
		if (!parseMultipartAndExtract(raw, boundary, filename, data)) {
			error="failed to parse multipart/form-data";
			return false;
		}
	} else {
		// octet-stream (or anything else): use whole body and invent a name
		if (!bodyv.empty()) data.assign(&bodyv[0], bodyv.size());
		else data = raw;

		// filename hint (optional) via header X-Filename
		filename = getHeaderCI(H, "x-filename");
		if (filename.empty()) {
			std::ostringstream oss;
			oss << "upload_" << std::time(NULL) << ".bin";
			filename = oss.str();
		}
	}

	filename = basenameOnly(filename);
	if (filename.empty()) { error="empty filename"; return false; }

	saved_path = joinPath(dest_dir, filename);

	std::ofstream out(saved_path.c_str(), std::ios::binary);
	if (!out) { error="failed to open destination file for write"; return false; }
	out.write(data.data(), static_cast<std::streamsize>(data.size()));
	out.close();
	if (!out) { error="failed while writing file"; return false; }

	return true;
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
			// [ADD] Remove any "Status:" header from CGI payload to avoid duplicate status signaling
			std::string cgiPayload = cgi_result.content;
			stripCgiStatusHeader(cgiPayload);
			HTTPResponse response(cgi_result.status_message, cgi_result.status_code, cgiPayload, socketFD);
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

	// --- Handle POST upload if this location has upload_path configured ---
	if (request.getMethod() == "POST" && matching_location) {
		if (!matching_location->upload_path.empty()) {
			std::string saved, err;
			if (saveUploadedBody(request, matching_location, server_config, saved, err))
			{
				if (request.getMethod() == "HEAD") { // [ADD]
					std::ostringstream okh;
					okh << "Content-Type: text/plain\r\n"
						<< "Content-Length: 0\r\n"
						<< request.connectionHeader(request.isConnectionAlive())
						<< "\r\n";
					HTTPResponse resp("OK", 200, okh.str(), socketFD);
					srv.queueResponse(socketFD, resp.getRawResponse());
					return;
				}
				std::ostringstream ok;
				ok << "Content-Type: text/plain\r\n"
					<< request.connectionHeader(request.isConnectionAlive())
					<< "\r\n"
					<< "Uploaded to: " << saved << "\n";
				HTTPResponse resp("OK", 200, ok.str(), socketFD); 
				srv.queueResponse(socketFD, resp.getRawResponse());
			} else {
				// Config has upload_path but we failed to store → 500
				std::string body = "<html><body><h1>500</h1><p>" + err + "</p></body></html>";
				std::ostringstream out;
				out << "Content-Type: text/html\r\n"
					<< request.connectionHeader(request.isConnectionAlive()) << "\r\n"
					<< body;
				HTTPResponse resp("Internal Server Error", 500, out.str(), socketFD);
				srv.queueResponse(socketFD, resp.getRawResponse());
			}
			return; // handled POST
		}
		// If no upload_path here, fall through to static/CGI rules
	}

	// Directory request → list on GET, and support HEAD (no body)
	if ((request.getMethod() == "GET" || request.getMethod() == "HEAD") && !filePath.empty() && filePath[filePath.length() - 1] == '/') {
		bool autoindex_enabled = (matching_location ? matching_location->autoindex : false);
		if (autoindex_enabled) {
			std::string dirListing = generateDirectoryListing(filePath);
			if (request.getMethod() == "HEAD")
			{
				std::string h = buildHeadOnlyHeaders("text/html",
														dirListing.size(),
														request);
				HTTPResponse response("OK", 200, h, socketFD);
				srv.queueResponse(socketFD, response.getRawResponse());
				return;
			}
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

	if (request.getMethod() == "HEAD")
	{ 
		// [ADD] no body, correct length
		std::string h = buildHeadOnlyHeaders(contentType, content.size(), request);
		HTTPResponse response("OK", 200, h, socketFD);
		srv.queueResponse(socketFD, response.getRawResponse());
		return;
	}
	std::string responseContent = "Content-Type: " + contentType + "\r\n"
								+ request.connectionHeader(request.isConnectionAlive()) + "\r\n"
								+ content;
	HTTPResponse response("OK", 200, responseContent, socketFD);
	srv.queueResponse(socketFD, response.getRawResponse()); // [CHANGE]
}
