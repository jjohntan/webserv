#include "http_cgi.hpp"
#include "HTTPResponse/HTTPResponse.hpp"
#include <fstream>
#include <dirent.h>
#include <sstream>
#include <iostream>

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
void handleRequestProcessing(const HTTPRequest& request, int socketFD, const std::vector<ServerConfig>& servers)
{
	(void)socketFD; 
	std::string path = request.getPath();
	
	// Find the server configuration for this request
	const ServerConfig* server_config = findServerConfig(request, servers);
	
	// Check if CGI is enabled for this path in the server configuration
	bool cgi_enabled = isCGIEnabled(path, server_config);
	
	if (cgi_enabled) {
		// Get CGI extensions from server configuration
		std::map<std::string, std::string> cgi_extensions = getCGIExtensions(path, server_config);
		
		CGIHandler cgi_handler;
		if (cgi_handler.needsCGI(path, cgi_extensions)) {
			// Resolve CGI script path based on location configuration
			std::string script_path = path;
			std::string working_directory = "./";
			
			// Handle /cgi_bin/ location - map to ./cgi_bin/ directory
			if (path.find("/cgi_bin/") == 0) {
				script_path = "./cgi_bin" + path.substr(8); // Remove "/cgi_bin" and add "./cgi_bin"
				working_directory = "./";
			}
			
			// Execute CGI script
			std::cout << "Executing CGI Script: " << script_path << std::endl;
			CGIResult cgi_result = runCGI(request, script_path, cgi_extensions, working_directory);
			HTTPResponse response(cgi_result.status_message, cgi_result.status_code, cgi_result.content, socketFD);
			// Sending response back to corresponding socket
			std::cout << "Sending CGI Response Back to Socket\n";
			response.sendResponse();
		} else {
			// CGI script not found, serve as static file
			std::string filePath;
			std::string server_root = server_config ? server_config->root : "pages/www";
			
			if (path == "/" || path.empty()) {
				// Serve index.html for root path
				filePath = server_root + "/index.html";
			} else {
				filePath = server_root + path;
			}
			
			std::cout << "CGI Script Not Found - Serving as Static File: " << filePath << std::endl;
			
			std::string content = serveFile(filePath);
			if (!content.empty()) {
				// Add proper content type header for HTML files
				std::string contentType = "text/html";
				if (filePath.find(".css") != std::string::npos) {
					contentType = "text/css";
				} else if (filePath.find(".js") != std::string::npos) {
					contentType = "application/javascript";
				} else if (filePath.find(".jpg") != std::string::npos || filePath.find(".jpeg") != std::string::npos) {
					contentType = "image/jpeg";
				} else if (filePath.find(".png") != std::string::npos) {
					contentType = "image/png";
				}
				
				std::ostringstream contentLengthStream;
				contentLengthStream << content.length();
				std::string responseContent = "Content-Type: " + contentType + "\r\nContent-Length: " + contentLengthStream.str() + "\r\n\r\n" + content;
				
				HTTPResponse response("OK", 200, responseContent, socketFD);
				std::cout << "Sending Response Back to Socket\n";
				response.sendResponse();
			} else {
				// File not found, send 404
				std::string errorHtml = "<html><body><h1>404 Not Found</h1><p>The requested file was not found.</p></body></html>";
				std::ostringstream errorLengthStream;
				errorLengthStream << errorHtml.length();
				std::string errorContent = "Content-Type: text/html\r\nContent-Length: " + errorLengthStream.str() + "\r\n\r\n" + errorHtml;
				HTTPResponse response("Not Found", 404, errorContent, socketFD);
				std::cout << "Sending 404 Response Back to Socket\n";
				response.sendResponse();
			}
		}
	} else {
		// Serve static files using server configuration
		std::string filePath;
		std::string server_root = server_config ? server_config->root : "pages/www";
		
		// Check if this path matches a specific location with its own root
		const Location* matching_location = NULL;
		size_t best_match_length = 0;
		
		if (server_config) {
			for (size_t i = 0; i < server_config->locations.size(); ++i) {
				const Location& location = server_config->locations[i];
				if (path.find(location.path) == 0 && location.path.length() > best_match_length) {
					matching_location = &location;
					best_match_length = location.path.length();
				}
			}
		}
		
		// Use location-specific root if available
		if (matching_location && !matching_location->root.empty()) {
			server_root = matching_location->root;
		}
		
		if (path == "/" || path.empty()) {
			// Serve index.html for root 
			filePath = server_root + "/index.html";
		} else {
			filePath = server_root + path;
		}
		
		// Check if it's a directory request - Fixed the .back() issue
		if (!filePath.empty() && filePath[filePath.length() - 1] == '/') {
			// Directory request - check if autoindex is enabled
			bool autoindex_enabled = false;
			if (matching_location) {
				autoindex_enabled = matching_location->autoindex;
			}
			
			if (autoindex_enabled) {
				// Generate directory listing
				std::string dirListing = generateDirectoryListing(filePath);
				std::ostringstream contentLengthStream;
				contentLengthStream << dirListing.length();
				std::string responseContent = "Content-Type: text/html\r\nContent-Length: " + contentLengthStream.str() + "\r\n\r\n" + dirListing;
				
				HTTPResponse response("OK", 200, responseContent, socketFD);
				std::cout << "Sending Directory Listing Response Back to Socket\n";
				response.sendResponse();
			} else {
				// Autoindex disabled, send 403 Forbidden
				std::string errorHtml = "<html><body><h1>403 Forbidden</h1><p>Directory listing is not allowed.</p></body></html>";
				std::ostringstream errorLengthStream;
				errorLengthStream << errorHtml.length();
				std::string errorContent = "Content-Type: text/html\r\nContent-Length: " + errorLengthStream.str() + "\r\n\r\n" + errorHtml;
				HTTPResponse response("Forbidden", 403, errorContent, socketFD);
				std::cout << "Sending 403 Response Back to Socket\n";
				response.sendResponse();
			}
		} else {
			std::string content = serveFile(filePath);
			if (content.empty()) {
				// File not found, send 404
				std::string errorHtml = "<html><body><h1>404 Not Found</h1><p>The requested file was not found.</p></body></html>";
				std::ostringstream errorLengthStream;
				errorLengthStream << errorHtml.length();
				std::string errorContent = "Content-Type: text/html\r\nContent-Length: " + errorLengthStream.str() + "\r\n\r\n" + errorHtml;
				HTTPResponse response("Not Found", 404, errorContent, socketFD);
				std::cout << "Sending 404 Response Back to Socket\n";
				response.sendResponse();
			} else {
				// Add proper content type header for HTML files
				std::string contentType = "text/html";
				if (filePath.find(".css") != std::string::npos) {
					contentType = "text/css";
				} else if (filePath.find(".js") != std::string::npos) {
					contentType = "application/javascript";
				} else if (filePath.find(".jpg") != std::string::npos || filePath.find(".jpeg") != std::string::npos) {
					contentType = "image/jpeg";
				} else if (filePath.find(".png") != std::string::npos) {
					contentType = "image/png";
				}
				
				std::ostringstream contentLengthStream;
				contentLengthStream << content.length();
				std::string responseContent = "Content-Type: " + contentType + "\r\nContent-Length: " + contentLengthStream.str() + "\r\n\r\n" + content;
				
				HTTPResponse response("OK", 200, responseContent, socketFD);
				std::cout << "Sending Response Back to Socket\n";
				response.sendResponse();
			}
		}
	}
}
