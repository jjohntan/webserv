#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <map>
#include <vector>
#include "cgi_handler/cgi.hpp"
#include "http/HTTPRequest.hpp"
#include "config_files/config.hpp"

class Server {
private:
    int server_fd;
    int port;
    std::string document_root;
    std::map<int, std::string> error_pages;
    std::vector<ServerConfig> server_configs;
    CGIHandler cgi_handler;
    
    void handleRequest(int client_fd);
    void sendErrorResponse(int client_fd, int error_code);
    bool endsWith(const std::string& str, const std::string& suffix);
    std::string getContentType(const std::string& path);
    std::string handleCGIRequest(const HTTPRequest& request, const Location& location, const std::string& file_path);
    std::string processHTTPRequest(const HTTPRequest& request);
    const ServerConfig* findMatchingServer(const HTTPRequest& request);
    const Location* findMatchingLocation(const std::string& path, const std::vector<Location>& locations);
    std::string buildFilePath(const std::string& request_path, const Location& location);
    std::string serveStaticFile(const std::string& file_path);
    std::string intToString(int value);
    
public:
    Server(int p, const std::string& root);
    Server(int p, const std::string& root, const std::vector<ServerConfig>& configs);
    ~Server();
    
    bool start();
    void run();
    void setServerConfigs(const std::vector<ServerConfig>& configs);
};

#endif