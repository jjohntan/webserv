#include "server.hpp"
#include "http/HTTPResponse/HTTPResponse.hpp" // Add this include
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <sys/stat.h>
#include <dirent.h>
#include <ctime>
#include <cstdlib>

Server::Server(int p, const std::string& root) : document_root(root) {
    ports.push_back(p);
    server_fds.push_back(-1);
    // Set up error pages
    error_pages[400] = "./pages/error/400.html";
    error_pages[404] = "./pages/error/404.html";
    error_pages[408] = "./pages/error/408.html";
    error_pages[500] = "./pages/error/500.html";
}

Server::Server(int p, const std::string& root, const std::vector<ServerConfig>& configs) 
    : document_root(root), server_configs(configs) {
    ports.push_back(p);
    server_fds.push_back(-1);
    // Set up error pages
    error_pages[400] = "./pages/error/400.html";
    error_pages[404] = "./pages/error/404.html";
    error_pages[408] = "./pages/error/408.html";
    error_pages[500] = "./pages/error/500.html";
}

Server::Server(const std::vector<ServerConfig>& configs) : server_configs(configs) {
    if (!configs.empty()) {
        document_root = configs[0].root;
    }
    
    // Extract unique ports from configs
    for (size_t i = 0; i < configs.size(); ++i) {
        bool port_exists = false;
        for (size_t j = 0; j < ports.size(); ++j) {
            if (ports[j] == configs[i].port) {
                port_exists = true;
                break;
            }
        }
        if (!port_exists) {
            ports.push_back(configs[i].port);
            server_fds.push_back(-1);
        }
    }
    
    // Set up error pages from configuration (use first server config's error pages)
    if (!configs.empty()) {
        error_pages = configs[0].error_pages;
    }
    
    // Add default error pages if not specified in config
    if (error_pages.find(400) == error_pages.end()) {
        error_pages[400] = "./pages/error/400.html";
    }
    if (error_pages.find(404) == error_pages.end()) {
        error_pages[404] = "./pages/error/404.html";
    }
    if (error_pages.find(405) == error_pages.end()) {
        error_pages[405] = "./pages/error/405.html";
    }
    if (error_pages.find(408) == error_pages.end()) {
        error_pages[408] = "./pages/error/408.html";
    }
    if (error_pages.find(500) == error_pages.end()) {
        error_pages[500] = "./pages/error/500.html";
    }
}

Server::~Server() {
    for (size_t i = 0; i < server_fds.size(); ++i) {
        if (server_fds[i] != -1) {
            close(server_fds[i]);
        }
    }
}

void Server::setServerConfigs(const std::vector<ServerConfig>& configs) {
    server_configs = configs;
}

bool Server::start() {
    // Create sockets for all ports
    for (size_t i = 0; i < ports.size(); ++i) {
        int port = ports[i];
        
        // Create socket
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd == -1) {
            std::cerr << "Failed to create socket for port " << port << std::endl;
            return false;
        }
        
        // Allow socket reuse
        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        // Set up address
        sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);
        
        // Bind socket
        if (bind(server_fd, (sockaddr*)&address, sizeof(address)) < 0) {
            std::cerr << "Failed to bind to port " << port << std::endl;
            close(server_fd);
            return false;
        }
        
        // Listen for connections
        if (listen(server_fd, 10) < 0) {
            std::cerr << "Failed to listen on socket for port " << port << std::endl;
            close(server_fd);
            return false;
        }
        
        server_fds[i] = server_fd;
        std::cout << "Listening on port " << port << ".." << std::endl;
    }
    
    return true;
}

void Server::run() {
    fd_set read_fds;
    int max_fd = 0;
    
    // Find the maximum file descriptor
    for (size_t i = 0; i < server_fds.size(); ++i) {
        if (server_fds[i] > max_fd) {
            max_fd = server_fds[i];
        }
    }
    
    while (true) {
        // Clear the descriptor set
        FD_ZERO(&read_fds);
        
        // Add all server sockets to the set
        for (size_t i = 0; i < server_fds.size(); ++i) {
            FD_SET(server_fds[i], &read_fds);
        }
        
        // Wait for activity on any socket
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        
        if (activity < 0) {
            std::cerr << "Select error" << std::endl;
            continue;
        }
        
        // Check which socket has activity
        for (size_t i = 0; i < server_fds.size(); ++i) {
            if (FD_ISSET(server_fds[i], &read_fds)) {
                sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                
                int client_fd = accept(server_fds[i], (sockaddr*)&client_addr, &client_len);
                if (client_fd == -1) {
                    std::cerr << "Failed to accept connection on port " << ports[i] << std::endl;
                    continue;
                }
                
                std::cout << "Listening on port " << ports[i] << ".." << std::endl;
                handleRequest(client_fd, ports[i]);
                close(client_fd);
            }
        }
    }
}

void Server::handleRequest(int client_fd, int port) {
    char buffer[4096];
    std::string complete_request;
    ssize_t bytes_read;
    bool header_complete = false;
    size_t content_length = 0;
    size_t header_end_pos = 0;
    
    // Read the request in chunks
    while (true) {
        for (int i = 0; i < 4096; ++i) {
            buffer[i] = 0;
        }
        
        bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_read <= 0) {
            if (complete_request.empty()) {
                sendErrorResponse(client_fd, 400);
                return;
            }
            break; // No more data
        }
        
        complete_request.append(buffer, bytes_read);
        
        // Check if we have complete headers
        if (!header_complete) {
            header_end_pos = complete_request.find("\r\n\r\n");
            if (header_end_pos != std::string::npos) {
                header_complete = true;
                
                // Extract content-length from headers
                std::string headers = complete_request.substr(0, header_end_pos);
                size_t cl_pos = headers.find("Content-Length:");
                if (cl_pos == std::string::npos) {
                    cl_pos = headers.find("content-length:");
                }
                
                if (cl_pos != std::string::npos) {
                    size_t cl_start = headers.find(":", cl_pos) + 1;
                    size_t cl_end = headers.find("\r\n", cl_start);
                    if (cl_end != std::string::npos) {
                        std::string cl_str = headers.substr(cl_start, cl_end - cl_start);
                        // Trim spaces
                        size_t start = cl_str.find_first_not_of(" \t");
                        size_t end = cl_str.find_last_not_of(" \t");
                        if (start != std::string::npos && end != std::string::npos) {
                            cl_str = cl_str.substr(start, end - start + 1);
                            content_length = atoi(cl_str.c_str());
                        }
                    }
                }
            }
        }
        
        // If we have headers and know content length, check if we have complete body
        if (header_complete && content_length > 0) {
            size_t body_start = header_end_pos + 4;
            size_t current_body_size = complete_request.length() - body_start;
            
            if (current_body_size >= content_length) {
                break; // We have complete request
            }
        } else if (header_complete && content_length == 0) {
            break; // No body expected (GET request)
        }
        
        // Prevent infinite reading - safety check
        if (complete_request.length() > 10 * 1024 * 1024) { // 10MB limit
            sendErrorResponse(client_fd, 413); // Request Entity Too Large
            return;
        }
    }
    
    // Parse HTTP request using HTTPRequest class
    HTTPRequest request;
    request.feed(complete_request);
    
    if (!request.isHeaderComplete()) {
        sendErrorResponse(client_fd, 400);
        return;
    }
    
    // For POST requests, ensure body is complete
    if (request.getMethod() == "POST" && content_length > 0 && !request.isBodyComplete()) {
        sendErrorResponse(client_fd, 400);
        return;
    }
    
    // Process the request and get HTTPResponse object
    HTTPResponse response = processHTTPRequest(request, client_fd, port);
    
    // Send response using HTTPResponse class
    response.sendResponse();
}

HTTPResponse Server::processHTTPRequest(const HTTPRequest& request, int client_fd, int port) {
    std::cout << "[DEBUG] processHTTPRequest - Method: " << request.getMethod() << ", Path: " << request.getPath() << std::endl;
    
    // Find matching server config based on request
    const ServerConfig* server_config = findMatchingServer(request, port);
    if (!server_config) {
        return createErrorResponse(404, client_fd);
    }
    
    // Find matching location
    const Location* location = findMatchingLocation(request.getPath(), server_config->locations);
    if (!location) {
        std::cout << "[DEBUG] No matching location found for path: " << request.getPath() << std::endl;
        return createErrorResponse(404, client_fd, server_config);
    }
    
    std::cout << "[DEBUG] Matched location: " << location->path << std::endl;
    
    // Check for redirects first
    if (location->redirect_code > 0) {
        std::cout << "[DEBUG] Redirect found: " << location->redirect_code << " -> " << location->redirect_url << std::endl;
        
        // Create redirect headers
        std::string redirect_content = "Location: " + location->redirect_url + "\r\n";
        redirect_content += "Content-Length: 0\r\n";
        redirect_content += "Connection: close\r\n";
        redirect_content += "\r\n";
        
        return HTTPResponse("Found", location->redirect_code, redirect_content, client_fd);
    }
    
    // Check if method is allowed
    if (std::find(location->allowed_methods.begin(), location->allowed_methods.end(), 
                  request.getMethod()) == location->allowed_methods.end()) {
        return createErrorResponse(405, client_fd, server_config);
    }
    
    // Handle DELETE method for file deletion
    if (request.getMethod() == "DELETE") {
        return handleDeleteRequest(request, *location, client_fd, server_config);
    }
    
    // Handle POST method for file uploads
    if (request.getMethod() == "POST" && location->path == "/upload/") {
        return handleUploadRequest(request, *location, client_fd);
    }
    
    // Build full file path
    std::string file_path = buildFilePath(request.getPath(), *location);
    
    // std::cout << "[DEBUG] processHTTPRequest - Checking if CGI needed for: " << file_path << std::endl;
    
    // Check if this is a CGI request
    bool needs_cgi = cgi_handler.needsCGI(file_path, location->cgi_extensions);
    // std::cout << "[DEBUG] processHTTPRequest - needsCGI returned: " << (needs_cgi ? "true" : "false") << std::endl;
    
    if (needs_cgi) {
        // std::cout << "[DEBUG] processHTTPRequest - Executing CGI request" << std::endl;
        return handleCGIRequest(request, *location, file_path, client_fd, server_config);
    }
    
    // std::cout << "[DEBUG] processHTTPRequest - Serving as static file" << std::endl;
    // Handle as static file
    return serveStaticFile(file_path, client_fd, server_config);
}

HTTPResponse Server::handleDeleteRequest(const HTTPRequest& request, const Location& location, int client_fd, const ServerConfig* server_config) {
    std::string file_path = buildFilePath(request.getPath(), location);
    
    // Check if file exists
    std::ifstream file_check(file_path.c_str());
    if (!file_check.good()) {
        return createErrorResponse(404, client_fd, server_config);
    }
    file_check.close();
    
    // Try to delete the file
    if (remove(file_path.c_str()) == 0) {
        std::string headers = "Content-Type: text/plain\r\n\r\n";
        std::string response_body = headers + "File deleted successfully";
        return HTTPResponse("OK", 200, response_body, client_fd);
    } else {
        return HTTPResponse("Internal Server Error", 500, "Failed to delete file", client_fd);
    }
}

HTTPResponse Server::handleUploadRequest(const HTTPRequest& request, const Location& location, int client_fd) {
    // Get the request body
    const std::vector<char>& body = request.getBodyVector();
    
    std::cout << "[DEBUG] Upload request - Body size: " << body.size() << std::endl;
    
    if (body.empty()) {
        std::cout << "[DEBUG] Upload error - Body is empty" << std::endl;
        
        std::string html = "<!DOCTYPE html><html><head><title>Upload Error</title></head><body>";
        html += "<h1>Upload Error</h1><p>No file data received. Body is empty.</p>";
        html += "<p><strong>Debug info:</strong> Body size = " + intToString(body.size()) + "</p>";
        html += "<a href=\"/\">Back to Home</a></body></html>";
        
        std::string headers = "Content-Type: text/html\r\n";
        headers += "Content-Length: " + intToString(html.length()) + "\r\n\r\n";
        return HTTPResponse("Bad Request", 400, headers + html, client_fd);
    }
    
    // Convert vector to string for easier processing
    std::string body_str(body.begin(), body.end());
    std::cout << "[DEBUG] Upload request - Body string length: " << body_str.length() << std::endl;
    
    // Debug: Check content type
    const std::map<std::string, std::string>& headers_map = request.getHeaderMap();
    std::map<std::string, std::string>::const_iterator content_type_it = headers_map.find("content-type");
    
    if (content_type_it != headers_map.end()) {
        std::cout << "[DEBUG] Content-Type: " << content_type_it->second << std::endl;
    } else {
        std::cout << "[DEBUG] No Content-Type header found" << std::endl;
    }
    
    if (content_type_it == headers_map.end() || 
        content_type_it->second.find("multipart/form-data") == std::string::npos) {
        std::string html = "<!DOCTYPE html><html><head><title>Upload Error</title></head><body>";
        html += "<h1>Upload Error</h1><p>Invalid content type. Expected multipart/form-data.</p>";
        if (content_type_it != headers_map.end()) {
            html += "<p>Got: " + content_type_it->second + "</p>";
        } else {
            html += "<p>No Content-Type header found</p>";
        }
        html += "<a href=\"/\">Back to Home</a></body></html>";
        
        std::string response_headers = "Content-Type: text/html\r\n";
        response_headers += "Content-Length: " + intToString(html.length()) + "\r\n\r\n";
        return HTTPResponse("Bad Request", 400, response_headers + html, client_fd);
    }
    
    // Simple multipart parsing
    std::string filename = "";
    std::string file_content = "";
    
    // Look for filename in Content-Disposition header within the body
    size_t filename_pos = body_str.find("filename=\"");
    if (filename_pos != std::string::npos) {
        filename_pos += 10; // Skip 'filename="'
        size_t filename_end = body_str.find("\"", filename_pos);
        if (filename_end != std::string::npos) {
            filename = body_str.substr(filename_pos, filename_end - filename_pos);
        }
    }
    
    std::cout << "[DEBUG] Filename found: " << filename << std::endl;
    
    if (filename.empty()) {
        // Try alternative parsing - look for filename=
        size_t alt_pos = body_str.find("filename=");
        if (alt_pos != std::string::npos) {
            alt_pos += 9; // Skip 'filename='
            size_t alt_end = body_str.find_first_of("\r\n;", alt_pos);
            if (alt_end != std::string::npos) {
                filename = body_str.substr(alt_pos, alt_end - alt_pos);
                // Remove quotes if present
                if (!filename.empty() && filename[0] == '"') {
                    filename = filename.substr(1);
                }
                if (!filename.empty() && filename[filename.length()-1] == '"') {
                    filename = filename.substr(0, filename.length()-1);
                }
            }
        }
    }
    
    if (filename.empty()) {
        filename = "uploaded_file_" + intToString(time(NULL)); // Generate default filename
    }
    
    std::cout << "[DEBUG] Final filename: " << filename << std::endl;
    
    // Find the actual file content (after the double CRLF)
    size_t content_start = body_str.find("\r\n\r\n");
    if (content_start != std::string::npos) {
        content_start += 4; // Skip "\r\n\r\n"
        
        // Find the end boundary (usually starts with \r\n--)
        size_t boundary_start = body_str.find("\r\n--", content_start);
        if (boundary_start != std::string::npos) {
            file_content = body_str.substr(content_start, boundary_start - content_start);
        } else {
            // Alternative: take everything after headers until near the end
            size_t content_end = body_str.length();
            if (content_end > content_start + 100) { // Leave some margin for boundary
                content_end = body_str.rfind("--", content_end - 50);
                if (content_end != std::string::npos && content_end > content_start) {
                    file_content = body_str.substr(content_start, content_end - content_start);
                }
            }
        }
    }
    
    std::cout << "[DEBUG] File content length: " << file_content.length() << std::endl;
    
    // Clean filename to prevent directory traversal
    size_t slash_pos = filename.find_last_of("/\\");
    if (slash_pos != std::string::npos) {
        filename = filename.substr(slash_pos + 1);
    }
    
    // Save file to upload directory
    std::string upload_path = location.upload_path.empty() ? "pages/upload" : location.upload_path;
    std::string full_path = upload_path + "/" + filename;
    
    std::cout << "[DEBUG] Saving to: " << full_path << std::endl;
    
    std::ofstream file(full_path.c_str(), std::ios::binary);
    if (!file.is_open()) {
        std::string html = "<!DOCTYPE html><html><head><title>Upload Error</title></head><body>";
        html += "<h1>Upload Error</h1><p>Failed to create file: " + full_path + "</p>";
        html += "<a href=\"/\">Back to Home</a></body></html>";
        
        std::string response_headers = "Content-Type: text/html\r\n";
        response_headers += "Content-Length: " + intToString(html.length()) + "\r\n\r\n";
        return HTTPResponse("Internal Server Error", 500, response_headers + html, client_fd);
    }
    
    file.write(file_content.c_str(), file_content.length());
    file.close();
    
    std::cout << "[DEBUG] File saved successfully" << std::endl;
    
    // Create success response
    std::string html = "<!DOCTYPE html>\n<html>\n<head>\n";
    html += "<title>Upload Success</title>\n";
    html += "<style>body { font-family: Arial, sans-serif; margin: 40px; text-align: center; }";
    html += ".success { color: #27ae60; font-size: 18px; }";
    html += ".info { color: #7f8c8d; margin: 10px 0; }";
    html += ".button { display: inline-block; margin: 20px 10px; padding: 10px 20px; ";
    html += "background-color: #3498db; color: white; text-decoration: none; border-radius: 5px; }</style>\n";
    html += "</head>\n<body>\n";
    html += "<h1>File Upload Successful!</h1>\n";
    html += "<p class=\"success\">File '" + filename + "' uploaded successfully.</p>\n";
    html += "<div class=\"info\">";
    html += "<p>File size: " + intToString(file_content.length()) + " bytes</p>\n";
    html += "<p>Saved to: " + upload_path + "</p>\n";
    html += "</div>";
    html += "<a href=\"/upload/\" class=\"button\">📁 View Uploaded Files</a>\n";
    html += "<a href=\"/\" class=\"button\">🏠 Back to Home</a>\n";
    html += "</body>\n</html>";
    
    std::string response_headers = "Content-Type: text/html\r\n";
    response_headers += "Content-Length: " + intToString(html.length()) + "\r\n";
    response_headers += "Connection: close\r\n\r\n";
    
    std::string response_body = response_headers + html;
    
    return HTTPResponse("OK", 200, response_body, client_fd);
}

HTTPResponse Server::handleCGIRequest(const HTTPRequest& request, const Location& location, const std::string& file_path, int client_fd, const ServerConfig* server_config) {
    // Set working directory (usually the directory containing the script)
    std::string working_directory = "./cgi_bin"; // or extract from file_path
    
    // std::cout << "[DEBUG] handleCGIRequest - file_path passed to CGI: " << file_path << std::endl;
    // std::cout << "[DEBUG] handleCGIRequest - working_directory: " << working_directory << std::endl;
    
    // Execute CGI
    CGIResult result = cgi_handler.executeCGI(request, file_path, location.cgi_extensions, working_directory);
    
    if (!result.success) {
        // Handle CGI execution error
        if (result.status_code == 500) {
            return HTTPResponse("Internal Server Error", 500, "CGI execution failed", client_fd);
        } else if (result.status_code == 404) {
            return createErrorResponse(404, client_fd, server_config);
        } else {
            return HTTPResponse("Internal Server Error", result.status_code, "CGI error", client_fd);
        }
    }
    
    // Build headers
    std::string headers = "";
    
    // Add CGI headers if provided
    if (!result.headers.empty()) {
        // std::cout << "[DEBUG] handleCGIRequest - Adding CGI headers: " << result.headers << std::endl;
        headers += result.headers + "\r\n";
    } else {
        // std::cout << "[DEBUG] handleCGIRequest - No CGI headers, using default" << std::endl;
        headers += "Content-Type: text/html\r\n";
    }
    
    // Add end of headers
    headers += "\r\n";
    
    // Combine headers and body
    std::string response_body = headers + result.body;
    
    // std::cout << "[DEBUG] handleCGIRequest - Final response body length: " << result.body.length() << std::endl;
    
    // Return HTTPResponse with status message and code
    return HTTPResponse("OK", 200, response_body, client_fd);
}

const ServerConfig* Server::findMatchingServer(const HTTPRequest& /* request */, int port) {
    // Find server config that matches the port
    for (std::vector<ServerConfig>::const_iterator it = server_configs.begin(); it != server_configs.end(); ++it) {
        if (it->port == port) {
            return &(*it);
        }
    }
    return NULL;
}

const Location* Server::findMatchingLocation(const std::string& path, const std::vector<Location>& locations) {
    // Find the most specific matching location
    const Location* best_match = NULL;
    size_t best_match_length = 0;
    
    for (std::vector<Location>::const_iterator it = locations.begin(); it != locations.end(); ++it) {
        if (path.find(it->path) == 0 && it->path.length() > best_match_length) {
            best_match = &(*it);
            best_match_length = it->path.length();
        }
    }
    
    return best_match;
}

std::string Server::buildFilePath(const std::string& request_path, const Location& location) {
    std::string clean_path = request_path;
    
   
    size_t query_pos = clean_path.find('?');
    if (query_pos != std::string::npos) {
        clean_path = clean_path.substr(0, query_pos);
    }
    
    // std::cout << "[DEBUG] buildFilePath - Clean path (after removing query): " << clean_path << std::endl;
    
    // For CGI-bin location, use location-specific root
    if (location.path == "/cgi-bin/") {
        // Remove /cgi-bin/ prefix and use location root
        std::string script_name = clean_path.substr(9); // Remove "/cgi-bin/"
        std::string location_root = location.root.empty() ? "cgi_bin" : location.root;
        std::string final_path = location_root + "/" + script_name;
        
        // std::cout << "[DEBUG] buildFilePath - Script name: " << script_name << std::endl;
        // std::cout << "[DEBUG] buildFilePath - Location root used: " << location_root << std::endl;
        // std::cout << "[DEBUG] buildFilePath - Final CGI path: " << final_path << std::endl;
        
        return final_path;
    }
    
    // For upload location, handle specially
    if (location.path == "/upload/") {
        // For upload location with root ./pages, we want:
        // /upload/ -> ./pages/upload/
        // /upload/file.txt -> ./pages/upload/file.txt
        
        std::string file_name = clean_path.substr(8); // Remove "/upload/"
        std::string location_root = location.root.empty() ? "pages" : location.root;
        
        // Build the full path: root + /upload + filename
        std::string final_path = location_root + "/upload";
        if (!file_name.empty()) {
            final_path += "/" + file_name;
        }
        
        std::cout << "[DEBUG] buildFilePath - Upload file name: '" << file_name << "'" << std::endl;
        std::cout << "[DEBUG] buildFilePath - Upload location root: '" << location_root << "'" << std::endl;
        std::cout << "[DEBUG] buildFilePath - Final upload path: '" << final_path << "'" << std::endl;
        
        return final_path;
    }
    
    // Handle root path
    if (clean_path == "/") {
        clean_path = "/" + location.index;
    }
    
    // For regular locations, use location root if available, otherwise document_root
    std::string root = location.root.empty() ? document_root : location.root;
    return root + clean_path;
}

HTTPResponse Server::serveStaticFile(const std::string& file_path, int client_fd, const ServerConfig* server_config) {
    std::cout << "[DEBUG] serveStaticFile called with path: '" << file_path << "'" << std::endl;
    
    // First check if it's a directory
    struct stat path_stat;
    if (stat(file_path.c_str(), &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
        std::cout << "[DEBUG] Path is a directory, calling generateDirectoryListing" << std::endl;
        return generateDirectoryListing(file_path, client_fd);
    }
    
    // Check if file exists
    std::ifstream file(file_path.c_str(), std::ios::binary);
    if (!file.is_open()) {
        std::cout << "[DEBUG] File not found and not a directory: " << file_path << std::endl;
        return createErrorResponse(404, client_fd, server_config);
    }
    
    // Read file content
    std::ostringstream oss;
    oss << file.rdbuf();
    std::string content = oss.str();
    
    // Determine content type
    std::string content_type = getContentType(file_path);
    
    // Convert content length to string (C++98 compatible)
    std::ostringstream content_length_ss;
    content_length_ss << content.length();
    
    // Build headers
    std::string headers = "Content-Type: " + content_type + "\r\n";
    headers += "Content-Length: " + content_length_ss.str() + "\r\n";
    headers += "Connection: close\r\n\r\n";
    
    // Combine headers and content
    std::string response_body = headers + content;
    
    // Removed verbose serving output
    return HTTPResponse("OK", 200, response_body, client_fd);
}

HTTPResponse Server::generateDirectoryListing(const std::string& dir_path, int client_fd) {
    std::cout << "[DEBUG] generateDirectoryListing called with path: '" << dir_path << "'" << std::endl;
    
    DIR* dir = opendir(dir_path.c_str());
    if (!dir) {
        std::cout << "[DEBUG] Failed to open directory: " << dir_path << std::endl;
        return HTTPResponse("Forbidden", 403, "Directory access forbidden", client_fd);
    }
    
    std::ostringstream html;
    html << "<!DOCTYPE html>\n<html>\n<head>\n";
    html << "<title>Directory Listing</title>\n";
    html << "<style>\n";
    html << "body { font-family: Arial, sans-serif; margin: 20px; }\n";
    html << "h1 { color: #333; }\n";
    html << "table { width: 100%; border-collapse: collapse; margin-top: 20px; }\n";
    html << "th, td { padding: 10px; text-align: left; border-bottom: 1px solid #ddd; }\n";
    html << "th { background-color: #f2f2f2; font-weight: bold; }\n";
    html << "a { color: #0066cc; text-decoration: none; }\n";
    html << "a:hover { text-decoration: underline; }\n";
    html << "</style>\n</head>\n<body>\n";
    html << "<h1>Directory Listing: " << dir_path << "</h1>\n";
    html << "<table>\n";
    html << "<tr><th>Name</th><th>Size</th><th>Type</th></tr>\n";
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        
        std::string full_path = dir_path + "/" + name;
        struct stat file_stat;
        std::string size_str = "-";
        std::string type_str = "File";
        
        if (stat(full_path.c_str(), &file_stat) == 0) {
            if (S_ISDIR(file_stat.st_mode)) {
                type_str = "Directory";
                size_str = "-";
            } else {
                std::ostringstream size_ss;
                size_ss << file_stat.st_size;
                size_str = size_ss.str() + " bytes";
            }
        }
        
        html << "<tr><td><a href=\"" << name << "\">" << name << "</a></td>";
        html << "<td>" << size_str << "</td>";
        html << "<td>" << type_str << "</td></tr>\n";
    }
    
    closedir(dir);
    html << "</table>\n</body>\n</html>";
    
    std::string content = html.str();
    std::ostringstream content_length_ss;
    content_length_ss << content.length();
    
    std::string headers = "Content-Type: text/html\r\n";
    headers += "Content-Length: " + content_length_ss.str() + "\r\n";
    headers += "Connection: close\r\n\r\n";
    
    std::string response_body = headers + content;
    
    return HTTPResponse("OK", 200, response_body, client_fd);
}

std::string Server::intToString(int value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

void Server::sendErrorResponse(int client_fd, int error_code) {
    HTTPResponse error_response = createErrorResponse(error_code, client_fd);
    error_response.sendResponse();
}

HTTPResponse Server::createErrorResponse(int error_code, int client_fd, const ServerConfig* server_config) {
    std::string error_message;
    
    // Set appropriate error message based on code
    switch (error_code) {
        case 400: error_message = "Bad Request"; break;
        case 404: error_message = "Not Found"; break;
        case 405: error_message = "Method Not Allowed"; break;
        case 408: error_message = "Request Timeout"; break;
        case 413: error_message = "Request Entity Too Large"; break;
        case 500: error_message = "Internal Server Error"; break;
        default: error_message = "Error"; break;
    }
    
    // Use error pages from server config if provided, otherwise use default
    std::map<int, std::string> current_error_pages = error_pages;
    std::string current_root = document_root;
    
    if (server_config != NULL) {
        current_error_pages = server_config->error_pages;
        current_root = server_config->root;
        std::cout << "[DEBUG] Using server config root: " << current_root << std::endl;
    }
    
    // Check if custom error page exists
    std::map<int, std::string>::iterator it = current_error_pages.find(error_code);
    if (it != current_error_pages.end()) {
        std::string error_page_path = it->second;
        
        // If the path starts with '/', it's relative to document root
        if (!error_page_path.empty() && error_page_path[0] == '/') {
            error_page_path = current_root + error_page_path;
        }
        
        std::cout << "[DEBUG] Looking for error page at: " << error_page_path << std::endl;
        
        // Try to read the custom error page
        std::ifstream file(error_page_path.c_str());
        if (file.is_open()) {
            std::string content;
            std::string line;
            while (std::getline(file, line)) {
                content += line + "\n";
            }
            file.close();
            
            std::cout << "[DEBUG] Successfully loaded custom error page for " << error_code << std::endl;
            
            // Create response with custom error page
            std::ostringstream content_length_ss;
            content_length_ss << content.length();
            
            std::string headers = "Content-Type: text/html\r\n";
            headers += "Content-Length: " + content_length_ss.str() + "\r\n";
            headers += "Connection: close\r\n\r\n";
            
            std::string response_body = headers + content;
            
            return HTTPResponse(error_message, error_code, response_body, client_fd);
        } else {
            std::cout << "[DEBUG] Failed to open custom error page: " << error_page_path << std::endl;
        }
    }
    
    // Fallback to default error page
    std::cout << "[DEBUG] Using default error page for " << error_code << std::endl;
    std::string body = "<html><body><h1>Error " + intToString(error_code) + " - " + error_message + "</h1></body></html>";
    return HTTPResponse(error_message, error_code, body, client_fd);
}

bool Server::endsWith(const std::string& str, const std::string& suffix) {
    if (suffix.length() > str.length()) return false;
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

std::string Server::getContentType(const std::string& path) {
    if (endsWith(path, ".html") || endsWith(path, ".htm")) {
        return "text/html";
    }
    else if (endsWith(path, ".css")) {
        return "text/css";
    }
    else if (endsWith(path, ".js")) {
        return "application/javascript";
    }
    else if (endsWith(path, ".png")) {
        return "image/png";
    }
    else if (endsWith(path, ".jpg") || endsWith(path, ".jpeg")) {
        return "image/jpeg";
    }
    else if (endsWith(path, ".gif")) {
        return "image/gif";
    }
    else if (endsWith(path, ".txt")) {
        return "text/plain";
    }
    else if (endsWith(path, ".pdf")) {
        return "application/pdf";
    }
    else if (endsWith(path, ".py")) {
        return "text/plain";  // This should not be reached if CGI detection works
    }
    return "text/plain";
}