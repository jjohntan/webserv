#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <set>

struct Location {
    std::string path;
    std::string root;
    std::string index;
    std::vector<std::string> allowed_methods;
    std::string upload_path;
    bool autoindex;
    std::map<std::string, std::string> cgi_extensions; 
    std::string redirect_url;
    int redirect_code;
    
    Location() : autoindex(false), redirect_code(0) {}
};

struct ServerConfig {
    std::string listen_ip;
    int port;
    std::vector<std::string> server_names;
    std::string root;
    size_t client_max_body_size;
    std::map<int, std::string> error_pages;
    std::vector<Location> locations;
    
    ServerConfig();
};

class ConfigParser {
private:
    std::string trim(const std::string& str);
    void parseServerDirective(const std::string& line, ServerConfig& server);
    void parseLocationDirective(const std::string& line, Location& location);
    
    // Validation methods
    bool validatePort(int port);
    bool validateIP(const std::string& ip);
    bool validateMethod(const std::string& method);
    bool validatePath(const std::string& path);
    bool validateRedirectCode(int code);
    bool validateErrorCode(int code);
    bool validateServerConfig(const ServerConfig& server);
    bool validateLocationConfig(const Location& location);
    void checkDuplicatePorts(const std::vector<ServerConfig>& servers);
    
public:
    std::vector<ServerConfig> parseConfig(const std::string& filename);
    bool validateConfig(const std::vector<ServerConfig>& servers);
};

#endif