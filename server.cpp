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

Server::Server(int p, const std::string& root) : server_fd(-1), port(p), document_root(root) {
    // Set up error pages
    error_pages[400] = "./pages/error/400.html";
    error_pages[404] = "./pages/error/404.html";
    error_pages[408] = "./pages/error/408.html";
    error_pages[500] = "./pages/error/500.html";
}

Server::Server(int p, const std::string& root, const std::vector<ServerConfig>& configs) 
    : server_fd(-1), port(p), document_root(root), server_configs(configs) {
    // Set up error pages
    error_pages[400] = "./pages/error/400.html";
    error_pages[404] = "./pages/error/404.html";
    error_pages[408] = "./pages/error/408.html";
    error_pages[500] = "./pages/error/500.html";
}

Server::~Server() {
    if (server_fd != -1) {
        close(server_fd);
    }
}

void Server::setServerConfigs(const std::vector<ServerConfig>& configs) {
    server_configs = configs;
}

bool Server::start() {
    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        std::cerr << "Failed to create socket" << std::endl;
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
        return false;
    }
    
    // Listen for connections
    if (listen(server_fd, 10) < 0) {
        std::cerr << "Failed to listen on socket" << std::endl;
        return false;
    }
    
    std::cout << "Server listening on http://localhost:" << port << std::endl;
    return true;
}

void Server::run() {
    while (true) {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
        if (client_fd == -1) {
            std::cerr << "Failed to accept connection" << std::endl;
            continue;
        }
        
        handleRequest(client_fd);
        close(client_fd);
    }
}

void Server::handleRequest(int client_fd) {
    char buffer[4096];
    for (int i = 0; i < 4096; ++i) {
        buffer[i] = 0;
    }
    
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_read <= 0) {
        sendErrorResponse(client_fd, 400);
        return;
    }
    
    std::string request_str(buffer);
    std::cout << "Request received:\n" << request_str.substr(0, request_str.find("\r\n")) << std::endl;
    
    // Parse HTTP request using HTTPRequest class
    HTTPRequest request;
    request.feed(request_str);
    
    if (!request.isHeaderComplete()) {
        sendErrorResponse(client_fd, 400);
        return;
    }
    
    // Process the request and get HTTPResponse object
    HTTPResponse response = processHTTPRequest(request, client_fd);
    
    // Send response using HTTPResponse class
    response.sendResponse();
    std::cout << "Response sent (" << response.getRawResponse().length() << " bytes)" << std::endl;
}

HTTPResponse Server::processHTTPRequest(const HTTPRequest& request, int client_fd) {
    // Find matching server config based on request
    const ServerConfig* server_config = findMatchingServer(request);
    if (!server_config) {
        return HTTPResponse("Not Found", 404, "No matching server", client_fd);
    }
    
    // Find matching location
    const Location* location = findMatchingLocation(request.getPath(), server_config->locations);
    if (!location) {
        return HTTPResponse("Not Found", 404, "No matching location", client_fd);
    }
    
    // Check if method is allowed
    if (std::find(location->allowed_methods.begin(), location->allowed_methods.end(), 
                  request.getMethod()) == location->allowed_methods.end()) {
        return HTTPResponse("Method Not Allowed", 405, "Method not allowed", client_fd);
    }
    
    // Handle DELETE method for file deletion
    if (request.getMethod() == "DELETE") {
        return handleDeleteRequest(request, *location, client_fd);
    }
    
    // Build full file path
    std::string file_path = buildFilePath(request.getPath(), *location);
    
    std::cout << "[DEBUG] processHTTPRequest - Checking if CGI needed for: " << file_path << std::endl;
    
    // Check if this is a CGI request
    bool needs_cgi = cgi_handler.needsCGI(file_path, location->cgi_extensions);
    std::cout << "[DEBUG] processHTTPRequest - needsCGI returned: " << (needs_cgi ? "true" : "false") << std::endl;
    
    if (needs_cgi) {
        std::cout << "[DEBUG] processHTTPRequest - Executing CGI request" << std::endl;
        return handleCGIRequest(request, *location, file_path, client_fd);
    }
    
    std::cout << "[DEBUG] processHTTPRequest - Serving as static file" << std::endl;
    // Handle as static file
    return serveStaticFile(file_path, client_fd);
}

HTTPResponse Server::handleDeleteRequest(const HTTPRequest& request, const Location& location, int client_fd) {
    std::string file_path = buildFilePath(request.getPath(), location);
    
    // Check if file exists
    std::ifstream file_check(file_path.c_str());
    if (!file_check.good()) {
        return HTTPResponse("Not Found", 404, "File not found", client_fd);
    }
    file_check.close();
    
    // Try to delete the file
    if (remove(file_path.c_str()) == 0) {
        return HTTPResponse("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n", "File deleted successfully", client_fd);
    } else {
        return HTTPResponse("Internal Server Error", 500, "Failed to delete file", client_fd);
    }
}

HTTPResponse Server::handleCGIRequest(const HTTPRequest& request, const Location& location, const std::string& file_path, int client_fd) {
    // Set working directory (usually the directory containing the script)
    std::string working_directory = "./cgi_bin"; // or extract from file_path
    
    std::cout << "[DEBUG] handleCGIRequest - file_path passed to CGI: " << file_path << std::endl;
    std::cout << "[DEBUG] handleCGIRequest - working_directory: " << working_directory << std::endl;
    
    // Execute CGI
    CGIResult result = cgi_handler.executeCGI(request, file_path, location.cgi_extensions, working_directory);
    
    if (!result.success) {
        // Handle CGI execution error
        if (result.status_code == 500) {
            return HTTPResponse("Internal Server Error", 500, "CGI execution failed", client_fd);
        } else if (result.status_code == 404) {
            return HTTPResponse("Not Found", 404, "CGI script not found", client_fd);
        } else {
            return HTTPResponse("Internal Server Error", result.status_code, "CGI error", client_fd);
        }
    }
    
    // Build status line with headers
    std::string status_line = "HTTP/1.1 200 OK\r\n";
    
    // Add CGI headers if provided
    if (!result.headers.empty()) {
        std::cout << "[DEBUG] handleCGIRequest - Adding CGI headers: " << result.headers << std::endl;
        status_line += result.headers + "\r\n";
    } else {
        std::cout << "[DEBUG] handleCGIRequest - No CGI headers, using default" << std::endl;
        status_line += "Content-Type: text/html\r\n";
    }
    
    std::cout << "[DEBUG] handleCGIRequest - Final response body length: " << result.body.length() << std::endl;
    
    // Return HTTPResponse with status line and body
    return HTTPResponse(status_line, result.body, client_fd);
}

const ServerConfig* Server::findMatchingServer(const HTTPRequest& /* request */) {
    // For simplicity, return the first server config if available
    // In a full implementation, you'd match based on Host header and port
    if (!server_configs.empty()) {
        return &server_configs[0];
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
    
    std::cout << "[DEBUG] buildFilePath - Original request_path: " << request_path << std::endl;
    std::cout << "[DEBUG] buildFilePath - Location path: " << location.path << std::endl;
    std::cout << "[DEBUG] buildFilePath - Location root: " << location.root << std::endl;
    
    // Remove query string if present
    size_t query_pos = clean_path.find('?');
    if (query_pos != std::string::npos) {
        clean_path = clean_path.substr(0, query_pos);
    }
    
    std::cout << "[DEBUG] buildFilePath - Clean path (after removing query): " << clean_path << std::endl;
    
    // For CGI-bin location, use location-specific root
    if (location.path == "/cgi-bin/") {
        // Remove /cgi-bin/ prefix and use location root
        std::string script_name = clean_path.substr(9); // Remove "/cgi-bin/"
        std::string location_root = location.root.empty() ? "cgi_bin" : location.root;
        std::string final_path = location_root + "/" + script_name;
        
        std::cout << "[DEBUG] buildFilePath - Script name: " << script_name << std::endl;
        std::cout << "[DEBUG] buildFilePath - Location root used: " << location_root << std::endl;
        std::cout << "[DEBUG] buildFilePath - Final CGI path: " << final_path << std::endl;
        
        return final_path;
    }
    
    // For upload location, handle specially
    if (location.path == "/upload/") {
        // Remove /upload/ prefix and use location root
        std::string file_name = clean_path.substr(8); // Remove "/upload/"
        std::string location_root = location.root.empty() ? "pages/upload" : location.root;
        std::string final_path = location_root + "/" + file_name;
        
        std::cout << "[DEBUG] buildFilePath - Upload file name: " << file_name << std::endl;
        std::cout << "[DEBUG] buildFilePath - Upload location root: " << location_root << std::endl;
        std::cout << "[DEBUG] buildFilePath - Final upload path: " << final_path << std::endl;
        
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

HTTPResponse Server::serveStaticFile(const std::string& file_path, int client_fd) {
    // Check if file exists
    std::ifstream file(file_path.c_str(), std::ios::binary);
    if (!file.is_open()) {
        std::cout << "File not found: " << file_path << std::endl;
        
        // Check if it's a directory and if autoindex is enabled
        struct stat path_stat;
        if (stat(file_path.c_str(), &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
            return generateDirectoryListing(file_path, client_fd);
        }
        
        return HTTPResponse("Not Found", 404, "File not found", client_fd);
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
    
    // Build status line with headers
    std::string status_line = "HTTP/1.1 200 OK\r\n";
    status_line += "Content-Type: " + content_type + "\r\n";
    status_line += "Content-Length: " + content_length_ss.str() + "\r\n";
    status_line += "Connection: close\r\n";
    
    std::cout << "Served: " << file_path << " (" << content.length() << " bytes)" << std::endl;
    return HTTPResponse(status_line, content, client_fd);
}

HTTPResponse Server::generateDirectoryListing(const std::string& dir_path, int client_fd) {
    DIR* dir = opendir(dir_path.c_str());
    if (!dir) {
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
    
    std::string status_line = "HTTP/1.1 200 OK\r\n";
    status_line += "Content-Type: text/html\r\n";
    status_line += "Content-Length: " + content_length_ss.str() + "\r\n";
    status_line += "Connection: close\r\n";
    
    return HTTPResponse(status_line, content, client_fd);
}

std::string Server::intToString(int value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

void Server::sendErrorResponse(int client_fd, int error_code) {
    std::string status_line = "HTTP/1.1 " + intToString(error_code) + " Error\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n";
    std::string body = "<html><body><h1>Error " + intToString(error_code) + "</h1></body></html>";
    HTTPResponse error_response(status_line, body, client_fd);
    error_response.sendResponse();
    std::cout << "Sent error response: " << error_code << std::endl;
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