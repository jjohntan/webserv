#include "config.hpp"

ServerConfig::ServerConfig() : port(0), client_max_body_size(0) {
}

std::vector<ServerConfig> ConfigParser::parseConfig(const std::string& filename) {
    std::ifstream file(filename.c_str());
    if (!file.is_open()) {
        std::cout << "Error: Cannot open config file: " << filename << std::endl;
        return std::vector<ServerConfig>();
    }
    
    std::vector<ServerConfig> servers;
    std::string line;
    ServerConfig current_server;
    Location current_location;
    bool in_server = false;
    bool in_location = false;
    
    while (std::getline(file, line)) {
        line = trim(line);
        
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') continue;
        
        if (line.find("server {") != std::string::npos) {
            current_server = ServerConfig();
            in_server = true;
            std::cout << "Found server block" << std::endl;
        }
        else if (line == "}" && in_location) {
            current_server.locations.push_back(current_location);
            in_location = false;
            std::cout << "End location block" << std::endl;
        }
        else if (line == "}" && in_server) {
            servers.push_back(current_server);
            in_server = false;
            std::cout << "End server block" << std::endl;
        }
        else if (in_server) {
            if (line.find("location") != std::string::npos) {
                current_location = Location();
                // Extract path from "location /path {"
                size_t start = line.find(' ') + 1;
                size_t end = line.find(' ', start);
                current_location.path = line.substr(start, end - start);
                in_location = true;
                std::cout << "Found location: " << current_location.path << std::endl;
            }
            else if (in_location) {
                parseLocationDirective(line, current_location);
            }
            else {
                parseServerDirective(line, current_server);
            }
        }
    }
    
    return servers;
}

void ConfigParser::printConfig(const std::vector<ServerConfig>& servers) {
    std::cout << "\n=== Configuration Summary ===" << std::endl;
    
    for (size_t i = 0; i < servers.size(); i++) {
        const ServerConfig& server = servers[i];
        std::cout << "\nServer " << (i + 1) << ":" << std::endl;
        std::cout << "  Listen: " << server.listen_ip << ":" << server.port << std::endl;
        
        std::cout << "  Server Names: ";
        for (size_t j = 0; j < server.server_names.size(); ++j) {
            std::cout << server.server_names[j] << " ";
        }
        std::cout << std::endl;
        
        std::cout << "  Root: " << server.root << std::endl;
        std::cout << "  Max Body Size: " << server.client_max_body_size << " bytes" << std::endl;
        
        std::cout << "  Error Pages:" << std::endl;
        for (std::map<int, std::string>::const_iterator it = server.error_pages.begin();
             it != server.error_pages.end(); ++it) {
            std::cout << "    " << it->first << " -> " << it->second << std::endl;
        }
        
        std::cout << "  Locations:" << std::endl;
        for (size_t j = 0; j < server.locations.size(); ++j) {
            const Location& location = server.locations[j];
            std::cout << "    Path: " << location.path << std::endl;
            std::cout << "    Index: " << location.index << std::endl;
            std::cout << "    Upload Path: " << location.upload_path << std::endl;
            std::cout << "    Autoindex: " << (location.autoindex ? "on" : "off") << std::endl;
            std::cout << "    Methods: ";
            for (size_t k = 0; k < location.allowed_methods.size(); ++k) {
                std::cout << location.allowed_methods[k] << " ";
            }
            std::cout << std::endl;
            std::cout << "    CGI Extensions:" << std::endl;
            for (std::map<std::string, std::string>::const_iterator it = location.cgi_extensions.begin();
                 it != location.cgi_extensions.end(); ++it) {
                std::cout << "      " << it->first << " -> " << it->second << std::endl;
            }
        }
    }
}

std::string ConfigParser::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

void ConfigParser::parseServerDirective(const std::string& line, ServerConfig& server) {
    std::istringstream iss(line);
    std::string directive;
    iss >> directive;
    
    if (directive == "listen") {
        std::string listen_addr;
        iss >> listen_addr;
        // Remove semicolon
        if (!listen_addr.empty() && listen_addr[listen_addr.length() - 1] == ';') {
            listen_addr = listen_addr.substr(0, listen_addr.length() - 1);
        }
        
        size_t colon_pos = listen_addr.find(':');
        if (colon_pos != std::string::npos) {
            server.listen_ip = listen_addr.substr(0, colon_pos);
            std::string port_str = listen_addr.substr(colon_pos + 1);
            std::istringstream port_ss(port_str);
            port_ss >> server.port;
        }
        std::cout << "  Listen: " << server.listen_ip << ":" << server.port << std::endl;
    }
    else if (directive == "server_name") {
        std::string name;
        while (iss >> name) {
            if (!name.empty() && name[name.length() - 1] == ';') {
                name = name.substr(0, name.length() - 1);
            }
            server.server_names.push_back(name);
        }
        std::cout << "  Server names parsed" << std::endl;
    }
    else if (directive == "root") {
        iss >> server.root;
        if (!server.root.empty() && server.root[server.root.length() - 1] == ';') {
            server.root = server.root.substr(0, server.root.length() - 1);
        }
        std::cout << "  Root: " << server.root << std::endl;
    }
    else if (directive == "client_max_body_size") {
        std::string size_str;
        iss >> size_str;
        if (!size_str.empty() && size_str[size_str.length() - 1] == ';') {
            size_str = size_str.substr(0, size_str.length() - 1);
        }
        std::istringstream size_ss(size_str);
        size_ss >> server.client_max_body_size;
        std::cout << "  Max body size: " << server.client_max_body_size << std::endl;
    }
    else if (directive == "error_page") {
        int code;
        std::string path;
        iss >> code >> path;
        if (!path.empty() && path[path.length() - 1] == ';') {
            path = path.substr(0, path.length() - 1);
        }
        server.error_pages[code] = path;
        std::cout << "  Error page: " << code << " -> " << path << std::endl;
    }
}

void ConfigParser::parseLocationDirective(const std::string& line, Location& location) {
    std::istringstream iss(line);
    std::string directive;
    iss >> directive;
    
    if (directive == "index") {
        iss >> location.index;
        if (!location.index.empty() && location.index[location.index.length() - 1] == ';') {
            location.index = location.index.substr(0, location.index.length() - 1);
        }
        std::cout << "    Index: " << location.index << std::endl;
    }
    else if (directive == "allowed_methods") {
        std::string method;
        while (iss >> method) {
            if (!method.empty() && method[method.length() - 1] == ';') {
                method = method.substr(0, method.length() - 1);
            }
            location.allowed_methods.push_back(method);
        }
        std::cout << "    Methods parsed" << std::endl;
    }
    else if (directive == "upload_path") {
        iss >> location.upload_path;
        if (!location.upload_path.empty() && location.upload_path[location.upload_path.length() - 1] == ';') {
            location.upload_path = location.upload_path.substr(0, location.upload_path.length() - 1);
        }
        std::cout << "    Upload path: " << location.upload_path << std::endl;
    }
    else if (directive == "root") {
        iss >> location.root;
        if (!location.root.empty() && location.root[location.root.length() - 1] == ';') {
            location.root = location.root.substr(0, location.root.length() - 1);
        }
        std::cout << "    Location root: " << location.root << std::endl;
    }
    else if (directive == "autoindex") {
        std::string value;
        iss >> value;
        if (!value.empty() && value[value.length() - 1] == ';') {
            value = value.substr(0, value.length() - 1);
        }
        location.autoindex = (value == "on" || value == "true");
        std::cout << "    Autoindex: " << (location.autoindex ? "on" : "off") << std::endl;
    }
    else if (directive == "cgi_extension") {
        std::string extension, executor;
        iss >> extension >> executor;
        if (!executor.empty() && executor[executor.length() - 1] == ';') {
            executor = executor.substr(0, executor.length() - 1);
        }
        location.cgi_extensions[extension] = executor;
        std::cout << "    CGI extension: " << extension << " -> " << executor << std::endl;
    }
}