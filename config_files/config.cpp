#include "config.hpp"

// ==================== CONSTRUCTORS ====================

ServerConfig::ServerConfig() : port(0), client_max_body_size(0) {
}

// ==================== MAIN CONFIGURATION FUNCTIONS ====================

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
    int line_num = 0;
    
    while (std::getline(file, line)) {
        line_num++;
        line = trim(line);
        
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') continue;
        
        if (line.find("server {") != std::string::npos) {
            current_server = ServerConfig();
            in_server = true;
            
        }
        else if (line == "}" && in_location) {
            if (validateLocationConfig(current_location)) {
                current_server.locations.push_back(current_location);
                
            } else {
                std::cout << "Error: Invalid location configuration at line " << line_num << std::endl;
            }
            in_location = false;
        }
        else if (line == "}" && in_server) {
            if (validateServerConfig(current_server)) {
                servers.push_back(current_server); // vector - adds an element to the end of a vector
               
            } else {
                std::cout << "Error: Invalid server configuration at line " << line_num << std::endl;
            }
            in_server = false;
        }
        else if (in_server) {
            if (line.find("location") != std::string::npos) {
                current_location = Location();
                
                // parse for location / postion
                size_t start = line.find(' ') + 1;
                size_t end = line.find(' ', start);
                if (end == std::string::npos) {
                    end = line.find('{', start);
                }
                // Extract path from "location /path {"
                current_location.path = line.substr(start, end - start);
                current_location.path = trim(current_location.path);
                in_location = true;
                
            }
            else if (in_location) {
                parseLocationDirective(line, current_location);
            }
            else {
                parseServerDirective(line, current_server);
            }
        }
    }
    
    file.close();
    
    // Final validation
    if (!validateConfig(servers)) {
        std::cout << "Error: Configuration validation failed" << std::endl;
        return std::vector<ServerConfig>();
    }
    
    return servers;
}


bool ConfigParser::validateConfig(const std::vector<ServerConfig>& servers) {
    if (servers.empty()) {
        std::cout << "Error: No valid server configurations found" << std::endl;
        return false;
    }
    
    checkDuplicatePorts(servers);
    return true;
}

// ==================== PARSING HELPERS ====================

std::string ConfigParser::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n"); // part of std::string
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
        iss >> listen_addr; // Extract the address part (after "listen")
        
        size_t colon_pos = listen_addr.find(':'); // From "listen 127.0.0.1:8080;", listen_addr becomes "127.0.0.1:8080;"
        if (colon_pos != std::string::npos) {
            server.listen_ip = listen_addr.substr(0, colon_pos); 
            std::string port_str = listen_addr.substr(colon_pos + 1);
            std::istringstream port_ss(port_str);
            port_ss >> server.port;
        } else {
            // Only port specified, default to 0.0.0.0
            std::istringstream port_ss(listen_addr);
            port_ss >> server.port;
            server.listen_ip = "0.0.0.0";
        }
        
    }
    else if (directive == "server_name") {
        std::string name;
        while (iss >> name) {
            if (!name.empty()) {
                server.server_names.push_back(name);
            }
        }
    }
    else if (directive == "root") {
        iss >> server.root;
    }
    else if (directive == "client_max_body_size") {
        std::string size_str;
        iss >> size_str;
        
        // Parse size with suffixes (K, M, G)
        char suffix = size_str[size_str.length() - 1];
        size_t multiplier = 1;
        std::string num_str = size_str;
        
        if (suffix == 'K' || suffix == 'k') {
            multiplier = 1024;
            num_str = size_str.substr(0, size_str.length() - 1);
        } else if (suffix == 'M' || suffix == 'm') {
            multiplier = 1024 * 1024;
            num_str = size_str.substr(0, size_str.length() - 1);
        } else if (suffix == 'G' || suffix == 'g') {
            multiplier = 1024 * 1024 * 1024;
            num_str = size_str.substr(0, size_str.length() - 1);
        }
        
        std::istringstream size_ss(num_str);
        size_t base_size;
        size_ss >> base_size;
        server.client_max_body_size = base_size * multiplier;
    }
    else if (directive == "error_page") {
        int code;
        std::string path;
        iss >> code >> path;
        if (validateErrorCode(code)) {
            server.error_pages[code] = path;
        } else {
            std::cout << "  Warning: Invalid error code " << code << std::endl;
        }
    }
}

void ConfigParser::parseLocationDirective(const std::string& line, Location& location) {
    std::istringstream iss(line);
    std::string directive;
    iss >> directive;
    
    if (directive == "index") {
        iss >> location.index;
    }
    else if (directive == "allowed_methods") {
        std::string method;
        while (iss >> method) {
            if (validateMethod(method)) {
                location.allowed_methods.push_back(method);
            } else {
                std::cout << "    Warning: Invalid method " << method << std::endl;
            }
        }
    }
    else if (directive == "upload_path") {
        iss >> location.upload_path;
    }
    else if (directive == "root") {
        iss >> location.root;
    }
    else if (directive == "autoindex") {
        std::string value;
        iss >> value;
        location.autoindex = (value == "on" || value == "true");
    }
    else if (directive == "cgi_extension") {
        std::string extension, executor;
        iss >> extension >> executor;
        location.cgi_extensions[extension] = executor;
    }
    else if (directive == "return" || directive == "redirect") {
        int code;
        std::string url;
        iss >> code >> url;
        if (validateRedirectCode(code)) {
            location.redirect_code = code;
            location.redirect_url = url;
        } else {
            std::cout << "    Warning: Invalid redirect code " << code << std::endl;
        }
    }
    else if (directive == "redirect_code") {
        int code; iss >> code;
        if (validateRedirectCode(code))
            location.redirect_code = code;
    }
    else if (directive == "redirect_url") {
        std::string url; iss >> url;
        location.redirect_url = url;
    }
}

// ==================== VALIDATION HELPERS ====================

bool ConfigParser::validatePort(int port) {
    return port > 0 && port <= 65535;
}

bool ConfigParser::validateIP(const std::string& ip) {
    if (ip == "localhost" || ip == "0.0.0.0") return true;
    
    // Simple IPv4 validation
    std::istringstream iss(ip);
    std::string octet;
    int count = 0;
    
    while (std::getline(iss, octet, '.')) {
        if (count >= 4) return false;
        int num = std::atoi(octet.c_str());
        if (num < 0 || num > 255) return false;
        count++;
    }
    return count == 4;
}

bool ConfigParser::validateMethod(const std::string& method) {
    return method == "GET" || method == "POST" || method == "DELETE";
}

bool ConfigParser::validatePath(const std::string& path) {
    return !path.empty() && path[0] == '/';
}

bool ConfigParser::validateRedirectCode(int code) {
    return code == 301 || code == 302 || code == 303 || code == 307 || code == 308;
}

bool ConfigParser::validateErrorCode(int code) {
    // Common HTTP error codes
    return code == 400 || code == 401 || code == 403 || code == 404 || 
           code == 405 || code == 408 || code == 413 || code == 414 || 
           code == 500 || code == 501 || code == 502 || code == 503 || code == 504;
}

bool ConfigParser::validateServerConfig(const ServerConfig& server) {
    if (!validatePort(server.port)) {
        std::cout << "Error: Invalid port " << server.port << std::endl;
        return false;
    }
    
    if (!validateIP(server.listen_ip)) {
        std::cout << "Error: Invalid IP " << server.listen_ip << std::endl;
        return false;
    }
    
    if (server.root.empty()) {
        std::cout << "Error: Root directory not specified" << std::endl;
        return false;
    }
    
    return true;
}

bool ConfigParser::validateLocationConfig(const Location& location) {
    if (!validatePath(location.path)) {
        std::cout << "Error: Invalid location path " << location.path << std::endl;
        return false;
    }
    
    if (location.allowed_methods.empty()) {
        std::cout << "Warning: No allowed methods specified for " << location.path << std::endl;
    }
    
    return true;
}

void ConfigParser::checkDuplicatePorts(const std::vector<ServerConfig>& servers) {
    std::set<std::pair<std::string, int> > used_addresses;
    
    for (size_t i = 0; i < servers.size(); ++i) {
        std::pair<std::string, int> addr(servers[i].listen_ip, servers[i].port);
        if (used_addresses.find(addr) != used_addresses.end()) {
            std::cout << "Warning: Duplicate listen address " 
                      << servers[i].listen_ip << ":" << servers[i].port << std::endl;
        }
        used_addresses.insert(addr);
    }
}