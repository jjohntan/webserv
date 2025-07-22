#include "server.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <sstream>

Server::Server(int p, const std::string& root) : server_fd(-1), port(p), document_root(root) {
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
    
    std::string request(buffer);
    std::cout << "Request received:\n" << request.substr(0, request.find("\r\n")) << std::endl;
    
    // Parse the request line
    std::istringstream iss(request);
    std::string method, path, version;
    iss >> method >> path >> version;
    
    if (method != "GET") {
        sendErrorResponse(client_fd, 400);
        return;
    }
    
    // Handle root path
    if (path == "/") {
        path = "/index.html";
    }
    
    // Build full file path
    std::string file_path = document_root + path;
    
    // Try to serve the file
    std::ifstream file(file_path.c_str(), std::ios::binary);
    if (!file.is_open()) {
        std::cout << "File not found: " << file_path << std::endl;
        sendErrorResponse(client_fd, 404);
        return;
    }
    
    // Read file content
    std::ostringstream oss;
    oss << file.rdbuf();
    std::string content = oss.str();
    
    // Determine content type
    std::string content_type = getContentType(path);
    
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
    
    send(client_fd, response.c_str(), response.length(), 0);
    std::cout << "Served: " << file_path << " (" << content.length() << " bytes)" << std::endl;
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