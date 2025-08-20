#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>

struct Location {
    std::string path;
    std::string index;
    std::vector<std::string> allowed_methods;
    std::string upload_path;
    bool autoindex;
    std::map<std::string, std::string> cgi_extensions; 
    
    Location() : autoindex(false) {}
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
    
public:
    std::vector<ServerConfig> parseConfig(const std::string& filename);
    void printConfig(const std::vector<ServerConfig>& servers);
};

#endif