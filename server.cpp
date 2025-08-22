#include "server.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

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
    
    // Process the request (with CGI support)
    std::string response = processHTTPRequest(request);
    
    // Send response
    send(client_fd, response.c_str(), response.length(), 0);
    std::cout << "Response sent (" << response.length() << " bytes)" << std::endl;
}

std::string Server::processHTTPRequest(const HTTPRequest& request) {
    // Find matching server config based on request
    const ServerConfig* server_config = findMatchingServer(request);
    if (!server_config) {
        return "HTTP/1.1 404 Not Found\r\n\r\nNo matching server";
    }
    
    // Find matching location
    const Location* location = findMatchingLocation(request.getPath(), server_config->locations);
    if (!location) {
        return "HTTP/1.1 404 Not Found\r\n\r\nNo matching location";
    }
    
    // Check if method is allowed
    if (std::find(location->allowed_methods.begin(), location->allowed_methods.end(), 
                  request.getMethod()) == location->allowed_methods.end()) {
        return "HTTP/1.1 405 Method Not Allowed\r\n\r\nMethod not allowed";
    }
    
    // Build full file path
    std::string file_path = buildFilePath(request.getPath(), *location);
    
    // Check if this is a CGI request
    if (cgi_handler.needsCGI(file_path, location->cgi_extensions)) {
        return handleCGIRequest(request, *location, file_path);
    }
    
    // Handle as static file
    return serveStaticFile(file_path);
}

std::string Server::handleCGIRequest(const HTTPRequest& request, const Location& location, const std::string& file_path) {
    // Set working directory (usually the directory containing the script)
    std::string working_directory = "./cgi_bin"; // or extract from file_path
    
    std::cout << "[DEBUG] handleCGIRequest - file_path passed to CGI: " << file_path << std::endl;
    std::cout << "[DEBUG] handleCGIRequest - working_directory: " << working_directory << std::endl;
    
    // Execute CGI
    CGIResult result = cgi_handler.executeCGI(request, file_path, location.cgi_extensions, working_directory);
    
/*
    result will contain:

    success → whether the script ran successfully
    status_code → HTTP status code like 200, 404, 500
    headers → any special headers the CGI script output (e.g., Content-Type)
    body → the actual content generated by the script
*/

    if (!result.success) {
        // Handle CGI execution error
        std::string error_response = "HTTP/1.1 " + intToString(result.status_code);
        if (result.status_code == 500) {
            error_response += " Internal Server Error\r\n\r\nCGI execution failed";
        } else if (result.status_code == 404) {
            error_response += " Not Found\r\n\r\nCGI script not found";
        }
        return error_response;
    }
    
    // when everything success then print http status line ok
    std::string http_response = "HTTP/1.1 200 OK\r\n";
    
    // Add CGI headers if provided
    if (!result.headers.empty()) {
        http_response += result.headers + "\r\n";
    } else {
        http_response += "Content-Type: text/html\r\n";
    }
    // \r\n marks the end of the headers section
    http_response += "\r\n" + result.body;
    
    return http_response;
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
    
    // Handle root path
    if (clean_path == "/") {
        clean_path = "/" + location.index;
    }
    
    // For regular locations, use location root if available, otherwise document_root
    std::string root = location.root.empty() ? document_root : location.root;
    return root + clean_path;
}

std::string Server::serveStaticFile(const std::string& file_path) {
    // Try to serve the file
    std::ifstream file(file_path.c_str(), std::ios::binary);
    if (!file.is_open()) {
        std::cout << "File not found: " << file_path << std::endl;
        return "HTTP/1.1 404 Not Found\r\n\r\nFile not found";
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
    
    // Send successful response
    std::string response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: " + content_type + "\r\n";
    response += "Content-Length: " + content_length_ss.str() + "\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    response += content;
    
    std::cout << "Served: " << file_path << " (" << content.length() << " bytes)" << std::endl;
    return response;
}

std::string Server::intToString(int value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

void Server::sendErrorResponse(int client_fd, int error_code) {
    std::string content;
    std::string status_text;
    
    // Try to load custom error page
    if (error_pages.find(error_code) != error_pages.end()) {
        std::ifstream error_file(error_pages[error_code].c_str());
        if (error_file.is_open()) {
            std::ostringstream oss;
            oss << error_file.rdbuf();
            content = oss.str();
        }
    }
    
    // Set status text and fallback content
    switch (error_code) {
        case 400:
            status_text = "Bad Request";
            if (content.empty()) content = "<h1>400 Bad Request</h1>";
            break;
        case 404:
            status_text = "Not Found";
            if (content.empty()) content = "<h1>404 Not Found</h1>";
            break;
        case 408:
            status_text = "Request Timeout";
            if (content.empty()) content = "<h1>408 Request Timeout</h1>";
            break;
        case 500:
            status_text = "Internal Server Error";
            if (content.empty()) content = "<h1>500 Internal Server Error</h1>";
            break;
        default:
            status_text = "Error";
            if (content.empty()) content = "<h1>Error</h1>";
    }
    
    // Convert error code and content length to strings (C++98 compatible)
    std::ostringstream error_code_ss, content_length_ss;
    error_code_ss << error_code;
    content_length_ss << content.length();
    
    std::string response = "HTTP/1.1 " + error_code_ss.str() + " " + status_text + "\r\n";
    response += "Content-Type: text/html\r\n";
    response += "Content-Length: " + content_length_ss.str() + "\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    response += content;
    
    send(client_fd, response.c_str(), response.length(), 0);
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
    return "text/plain";
}