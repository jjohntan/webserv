#include "http_cgi.hpp"

static void stripCgiStatusHeader(std::string& headersAndBody)
{
	// only look for the first occurrence of \r\n\r\n, avoid duplicate
	size_t hdr_end = headersAndBody.find("\r\n\r\n");
	if (hdr_end == std::string::npos) return;
	//split header and body
	std::string header = headersAndBody.substr(0, hdr_end);
	std::string body   = headersAndBody.substr(hdr_end + 4);

	std::istringstream hs(header);
	std::ostringstream newHdr;
	std::string line;

	//clean up line endings 
	while (std::getline(hs, line)) {
		if (!line.empty() && line[line.size()-1]=='\r') line.erase(line.size()-1,1);
		std::string lower = line;
		for (size_t i=0;i<lower.size();++i) lower[i] = std::tolower((unsigned char)lower[i]);
		if (lower.rfind("status:", 0) == 0) {
			// skip if status header found
			continue;
		}
		newHdr << line << "\r\n"; // newHdr = "Content-Type: text/html\r\n" for proper ending
	}
	//Combines cleaned headers with body
	headersAndBody = newHdr.str() + "\r\n" + body;
	// headersAndBody = "Content-Type: text/html\r\n\r\n<html>About</html>"
}


static void sendError(int code, const std::string& message, int socketFD, const ServerConfig* sc, const HTTPRequest* req, Server& srv)
{
	if (!sc) {
		// Fallback for no server config
		std::ostringstream oss;
		oss << "<html><body><h1>" << code << "</h1></body></html>";
		std::string body = oss.str();
		std::string full = "Content-Type: text/html\r\nConnection: close\r\n\r\n" + body;
		HTTPResponse resp(message, code, full, socketFD);
		srv.queueResponse(socketFD, resp.getRawResponse());
		return;
	}
	
	// Use existing ErrorResponse class
	std::string extraHeaders;
	if (req) {
		extraHeaders = req->connectionHeader(req->isConnectionAlive());
	}
	
	ErrorResponse errorResp(code, message, *sc, extraHeaders, socketFD);
	srv.queueResponse(socketFD, errorResp.getRawResponse());
}
/* --------------------------------------------------------------------------------------------------------------------------------*/

// CGI call function
CGIResult runCGI(const HTTPRequest& request, const std::string& script_path, const std::map<std::string, std::string>& cgi_extensions, const std::string& working_directory, const std::string& server_name, int server_port)
{
	CGIHandler cgi_handler;
	return cgi_handler.executeCGI(request, script_path, cgi_extensions, working_directory, server_name, server_port);
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


const ServerConfig* findServerConfig(const HTTPRequest& request, const std::vector<ServerConfig>& servers) {
	const std::map<std::string, std::string>& headers = request.getHeaderMap();
	std::map<std::string, std::string>::const_iterator host_it = headers.find("host");
	
	if (host_it == headers.end()) {
		// No Host header, return first server as fallback
		return servers.empty() ? NULL : &servers[0];
	}
	
	std::string host = host_it->second;  // "localhost:8081" or "example.com:8080"
	std::string hostname;
	std::string port_str;
	int port;
	
	// Parse hostname and port from Host header
	size_t colon_pos = host.find(':');
	if (colon_pos != std::string::npos) {
		hostname = host.substr(0, colon_pos);
		port_str = host.substr(colon_pos + 1);
		port = atoi(port_str.c_str());
	} else {
		// No port specified, assume default port 80
		hostname = host;
		port = 80;
	}
	
	// First pass: Find exact match by server_name and port
	for (size_t i = 0; i < servers.size(); ++i) {
		if (servers[i].port == port) {
			// Check if this server has the requested hostname
			for (size_t j = 0; j < servers[i].server_names.size(); ++j) {
				if (servers[i].server_names[j] == hostname) {
					return &servers[i];
				}
			}
		}
	}
	
	// Second pass: Find first server on the same port (default server for this port)
	for (size_t i = 0; i < servers.size(); ++i) {
		if (servers[i].port == port) {
			return &servers[i];
		}
	}
	
	return NULL; // No matching server found
}


// Check whether CGI is enabled for a specific path by finding the most specific location match (from config file)
bool isCGIEnabled(const std::string& path, const ServerConfig* server_config) {
	const Location* best_match = getMatchingLocation(path, server_config);
	
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
	
	const Location* best_match = getMatchingLocation(path, server_config);
	
	if (best_match) {
		cgi_extensions = best_match->cgi_extensions;
	}
	
	return cgi_extensions;
}



std::string parseMultipartData(const std::string& body, const std::string& boundary, std::string& filename) {
	if (boundary.empty()) {
		return "";
	}
	
	// Find the boundary in the body
	std::string full_boundary = "--" + boundary;
	size_t boundary_pos = body.find(full_boundary);
	if (boundary_pos == std::string::npos) {
		return "";
	}
	
	// Finds where headers end and file content begins
	size_t header_end = body.find("\r\n\r\n", boundary_pos);
	if (header_end == std::string::npos) {
		return "";
	}
	
	// Extract header between boundary and \r\n\r\n
	std::string headers = body.substr(boundary_pos, header_end - boundary_pos);
	// extract filename from headers
	size_t filename_pos = headers.find("filename=\"");
	if (filename_pos != std::string::npos) {
		filename_pos += 10; // Skip "filename=\""
		size_t filename_end = headers.find("\"", filename_pos);
		if (filename_end != std::string::npos) {
			filename = headers.substr(filename_pos, filename_end - filename_pos);
		}
	}
	
	// Extract file content (after headers, before next boundary)
	size_t content_start = header_end + 4; // Skip \r\n\r\n
	size_t next_boundary = body.find(full_boundary, content_start);
	if (next_boundary == std::string::npos) {
		// No next boundary, take until end
		return body.substr(content_start);
	}
	
	// Clean up and Removes trailing \r\n from file content
	std::string content = body.substr(content_start, next_boundary - content_start);
	if (content.length() >= 2 && content.substr(content.length() - 2) == "\r\n") {
		content = content.substr(0, content.length() - 2);
	}
	
	return content;
}
 
bool handleFileUpload(const HTTPRequest& request, const std::string& upload_path, int socketFD, const ServerConfig* server_config, Server& srv) {
	// Gets the HTTP request body (contains the file data)
	std::string body = request.getRawBody();
	if (body.empty()) {
		sendError(400, "Bad Request", socketFD, server_config, &request, srv);
		return false;
	}
	
	// Create upload directory if it doesn't exist with permission
	if (access(upload_path.c_str(), F_OK) != 0) {
		if (mkdir(upload_path.c_str(), 0755) != 0) {
			sendError(500, "Internal Server Error", socketFD, server_config, &request, srv);
			return false;
		}
	}
	
	// Check if it's multipart/form-data
	std::string content_type = "";
	std::map<std::string, std::string> headers = request.getHeaderMap();
	std::map<std::string, std::string>::const_iterator ct_it = headers.find("content-type");
	if (ct_it != headers.end()) {
		content_type = ct_it->second;
	}
	
	std::string filename = "";
	std::string file_content = "";
	
	if (content_type.find("multipart/form-data") != std::string::npos) {
		// Parse multipart data
		size_t boundary_pos = content_type.find("boundary=");
		if (boundary_pos != std::string::npos) {
			std::string boundary = content_type.substr(boundary_pos + 9);
			file_content = parseMultipartData(body, boundary, filename);
		}
	} else {
		// direct upload without html
		/*
		Request: POST /upload/myfile.jpg
		request.getPath() = "/upload/myfile.jpg"
		filename = "myfile.jpg" (after last /)
		*/
		file_content = body;
		// Extract filename from path
		filename = request.getPath();
		size_t last_slash = filename.find_last_of('/');
		if (last_slash != std::string::npos) {
			filename = filename.substr(last_slash + 1);
		}
	}
	
	if (filename.empty()) {
		// Generate a unique filename
		std::ostringstream oss;
		oss << "upload_" << time(NULL) << ".bin";
		filename = oss.str();
	}
	
	if (file_content.empty()) {
		sendError(400, "Bad Request", socketFD, server_config, &request, srv);
		return false;
	}
	
	std::string file_path = upload_path + "/" + filename; // file_path = "./pages/upload/test.txt"
	
	/*
	Open file for binary writing
	Check if opened successfully (disk space, permissions, etc.)
	Write raw bytes from file_content
	Close file
	*/
	std::ofstream file(file_path.c_str(), std::ios::binary);
	if (!file.is_open()) {
		sendError(500, "Internal Server Error", socketFD, server_config, &request, srv);
		return false;
	}
	
	file.write(file_content.c_str(), file_content.size());
	file.close();
	
	// Send success response
	std::string response_content = "Content-Type: text/plain\r\n"
								 + request.connectionHeader(request.isConnectionAlive()) + "\r\n"
								 + "File uploaded successfully: " + filename;
	HTTPResponse response("Created", 201, response_content, socketFD);
	srv.queueResponse(socketFD, response.getRawResponse());
	return true;
}

// Helper function to handle file deletion 
bool handleFileDeletion(const HTTPRequest& request, const std::string& server_root, int socketFD, const ServerConfig* server_config, Server& srv) {
	std::string path = request.getPath();
	
	// load upload directory, use the upload path instead of server root
	std::string file_path;
	if (path.find("/upload/") == 0) {
		file_path = "./pages/upload" + path.substr(7); // Remove "/upload" prefix
	} else {
		file_path = server_root + path;
	}
	
	// Check if file exists
	if (access(file_path.c_str(), F_OK) != 0) {
		sendError(404, "Not Found", socketFD, server_config, &request, srv);
		return false;
	}
	
	// Delete the file
	if (unlink(file_path.c_str()) != 0) {
		sendError(500, "Internal Server Error", socketFD, server_config, &request, srv);
		return false;
	}
	
	// Send success response (204 No Content)
	std::string response_content = request.connectionHeader(request.isConnectionAlive()) + "\r\n";
	HTTPResponse response("No Content", 204, response_content, socketFD);
	srv.queueResponse(socketFD, response.getRawResponse());
	return true;
}

// Main function to processes incoming HTTP requests and decides whether to serve static files, execute CGI scripts
void handleRequestProcessing(const HTTPRequest& request, int socketFD, const std::vector<ServerConfig>& servers, Server& srv) 
{
	std::string path = request.getPath();

	// Server selection by Host:port
	const ServerConfig* server_config = findServerConfig(request, servers);

	// Pick the most specific location once
	const Location* matching_location = getMatchingLocation(path, server_config);

	// Decide CGI vs Static
	bool cgi_enabled = isCGIEnabled(path, server_config);
	if (cgi_enabled) {
		std::map<std::string, std::string> cgi_extensions = getCGIExtensions(path, server_config);
		CGIHandler cgi_handler;
		if (cgi_handler.needsCGI(path, cgi_extensions)) {
			std::string script_path = path;
			std::string working_directory = "./";
			if (path.find("/cgi_bin/") == 0) {
				script_path = "./cgi_bin" + path.substr(8);
				working_directory = "./";
			}

			// Execute CGI
			std::cout << "Executing CGI Script: " << script_path << std::endl;
			
			// Extract server name from Host header
			std::string server_name = "localhost";
			const std::map<std::string, std::string>& headers = request.getHeaderMap();
			std::map<std::string, std::string>::const_iterator host_it = headers.find("host");
			if (host_it != headers.end()) {
				std::string host = host_it->second;
				size_t colon_pos = host.find(':');
				if (colon_pos != std::string::npos) {
					server_name = host.substr(0, colon_pos);
				} else {
					server_name = host;
				}
			}
			
			CGIResult cgi_result = runCGI(request, script_path, cgi_extensions, working_directory, server_name, server_config->port);
			std::string cgiPayload = cgi_result.content;
			stripCgiStatusHeader(cgiPayload);
			HTTPResponse response(cgi_result.status_message, cgi_result.status_code, cgiPayload, socketFD);
			std::cout << "Queue CGI Response\n";
			// Add to server queue
			srv.queueResponse(socketFD, response.getRawResponse()); 
			return;
		}
		
	}

	// Static file handle
	std::string server_root = server_config ? server_config->root : "pages/www";  
	if (matching_location && !matching_location->root.empty())
		server_root = matching_location->root;

	// Check if location has custom index → use it
	// Otherwise → default to "index.html"
	std::string filePath;                                                         
	if (path == "/" || path.empty()) {
		std::string indexName = (matching_location && !matching_location->index.empty())
								? matching_location->index : "index.html";
		filePath = server_root + "/" + indexName;
	} else {
		filePath = server_root + path; // filePath = "./pages/www/about.html"
	}

	// Check if the method is allowed for this location
	if (!methodAllowed(request, matching_location)) {
		sendError(405, "Method Not Allowed", socketFD, server_config, &request, srv);
		return;
	}

	/*
	POST request → Check if location supports uploads
	Has upload_path → Handle file upload
	No upload_path → 400 Bad Request error
	*/
	if (request.getMethod() == "POST") {
		if (matching_location && !matching_location->upload_path.empty()) {
			handleFileUpload(request, matching_location->upload_path, socketFD, server_config, srv);
			return;
		} else {
			sendError(400, "Bad Request", socketFD, server_config, &request, srv);
			return;
		}
	}

	// Handle DELETE requests (file deletion)
	if (request.getMethod() == "DELETE") {
		handleFileDeletion(request, server_root, socketFD, server_config, srv);
		return;
	}

	// Auto index directory listing
	// filePath ends with / (indicates directory)
	if ((request.getMethod() == "GET" ) && !filePath.empty() && filePath[filePath.length() - 1] == '/') {
		bool autoindex_enabled = (matching_location ? matching_location->autoindex : false);
		if (autoindex_enabled) {
			std::string dirListing = generateDirectoryListing(filePath);
			std::string responseContent = "Content-Type: text/html\r\n"
										+ request.connectionHeader(request.isConnectionAlive()) + "\r\n"
										+ dirListing;
			HTTPResponse response("OK", 200, responseContent, socketFD);
			srv.queueResponse(socketFD, response.getRawResponse()); 
		}
		else
			sendError(403, "Forbidden", socketFD, server_config, &request, srv);     
		return;
	}

	// Read file (serveFile()) → Get file content
	std::string content = serveFile(filePath);
	if (content.empty()) {
		sendError(404, "Not Found", socketFD, server_config, &request, srv);         
		return;
	}

	// Determine static file type → Based on file extension
	std::string contentType = "text/html";
	if (filePath.find(".css") != std::string::npos) contentType = "text/css";
	else if (filePath.find(".js")  != std::string::npos) contentType = "application/javascript";
	else if (filePath.find(".jpg") != std::string::npos || filePath.find(".jpeg") != std::string::npos) contentType = "image/jpeg";
	else if (filePath.find(".png") != std::string::npos) contentType = "image/png";
	// Create HTTP response → Headers + content
	std::string responseContent = "Content-Type: " + contentType + "\r\n"
								+ request.connectionHeader(request.isConnectionAlive()) + "\r\n"
								+ content;
	HTTPResponse response("OK", 200, responseContent, socketFD);
	srv.queueResponse(socketFD, response.getRawResponse()); 
}
