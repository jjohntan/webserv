#include "config.hpp"
#include <algorithm>
#include <set>

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
    int line_num = 0;
    
    while (std::getline(file, line)) {
        line_num++;
        line = trim(line);
        
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') continue;
        
        if (line.find("server {") != std::string::npos) {
            current_server = ServerConfig();
            in_server = true;
            // Removed verbose output
        }
        else if (line == "}" && in_location) {
            if (validateLocationConfig(current_location)) {
                current_server.locations.push_back(current_location);
                // Removed verbose output
            } else {
                std::cout << "Error: Invalid location configuration at line " << line_num << std::endl;
            }
            in_location = false;
        }
        else if (line == "}" && in_server) {
            if (validateServerConfig(current_server)) {
                servers.push_back(current_server);
                // Removed verbose output
            } else {
                std::cout << "Error: Invalid server configuration at line " << line_num << std::endl;
            }
            in_server = false;
        }
        else if (in_server) {
            if (line.find("location") != std::string::npos) {
                current_location = Location();
                // Extract path from "location /path {"
                size_t start = line.find(' ') + 1;
                size_t end = line.find(' ', start);
                if (end == std::string::npos) {
                    end = line.find('{', start);
                }
                current_location.path = line.substr(start, end - start);
                current_location.path = trim(current_location.path);
                in_location = true;
                // Removed verbose output
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

void ConfigParser::printConfig(const std::vector<ServerConfig>& servers) {
    std::cout << "\n=== Configuration Summary ===" << std::endl;
    
    for (size_t i = 0; i < servers.size(); i++) {
        const ServerConfig& server = servers[i];
        std::cout << "\nServer " << (i + 1) << ":" << std::endl;
        std::cout << "  Listen: " << server.listen_ip << ":" << server.port << std::endl;
        
        std::cout << "  Server Names: ";
        for (size_t j = 0; j < server.server_names.size(); ++j) {
            std::cout << server.server_names[j];
            if (j < server.server_names.size() - 1) std::cout << " ";
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
            std::cout << "    Root: " << location.root << std::endl;
            std::cout << "    Index: " << location.index << std::endl;
            std::cout << "    Upload Path: " << location.upload_path << std::endl;
            std::cout << "    Autoindex: " << (location.autoindex ? "on" : "off") << std::endl;
            if (location.redirect_code > 0) {
                std::cout << "    Redirect: " << location.redirect_code << " -> " << location.redirect_url << std::endl;
            }
            std::cout << "    Methods: ";
            for (size_t k = 0; k < location.allowed_methods.size(); ++k) {
                std::cout << location.allowed_methods[k];
                if (k < location.allowed_methods.size() - 1) std::cout << " ";
            }
            std::cout << std::endl;
            if (!location.cgi_extensions.empty()) {
                std::cout << "    CGI Extensions:" << std::endl;
                for (std::map<std::string, std::string>::const_iterator it = location.cgi_extensions.begin();
                     it != location.cgi_extensions.end(); ++it) {
                    std::cout << "      " << it->first << " -> " << it->second << std::endl;
                }
            }
        }
    }
    std::cout << "\n=== Configuration Valid ===" << std::endl;
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
        } else {
            // Only port specified, default to 0.0.0.0
            std::istringstream port_ss(listen_addr);
            port_ss >> server.port;
            server.listen_ip = "0.0.0.0";
        }
        // Removed verbose output
    }
    else if (directive == "server_name") {
        std::string name;
        while (iss >> name) {
            if (!name.empty() && name[name.length() - 1] == ';') {
                name = name.substr(0, name.length() - 1);
            }
            if (!name.empty()) {
                server.server_names.push_back(name);
            }
        }
        // Removed verbose output
    }
    else if (directive == "root") {
        iss >> server.root;
        if (!server.root.empty() && server.root[server.root.length() - 1] == ';') {
            server.root = server.root.substr(0, server.root.length() - 1);
        }
        // Removed verbose output
    }
    else if (directive == "client_max_body_size") {
        std::string size_str;
        iss >> size_str;
        if (!size_str.empty() && size_str[size_str.length() - 1] == ';') {
            size_str = size_str.substr(0, size_str.length() - 1);
        }
        
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
        // Removed verbose output
    }
    else if (directive == "error_page") {
        int code;
        std::string path;
        iss >> code >> path;
        if (!path.empty() && path[path.length() - 1] == ';') {
            path = path.substr(0, path.length() - 1);
        }
        if (validateErrorCode(code)) {
            server.error_pages[code] = path;
            // Removed verbose output
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
        if (!location.index.empty() && location.index[location.index.length() - 1] == ';') {
            location.index = location.index.substr(0, location.index.length() - 1);
        }
        // Removed verbose output
    }
    else if (directive == "allowed_methods") {
        std::string method;
        while (iss >> method) {
            if (!method.empty() && method[method.length() - 1] == ';') {
                method = method.substr(0, method.length() - 1);
            }
            if (validateMethod(method)) {
                location.allowed_methods.push_back(method);
            } else {
                std::cout << "    Warning: Invalid method " << method << std::endl;
            }
        }
        // Removed verbose output
    }
    else if (directive == "upload_path") {
        iss >> location.upload_path;
        if (!location.upload_path.empty() && location.upload_path[location.upload_path.length() - 1] == ';') {
            location.upload_path = location.upload_path.substr(0, location.upload_path.length() - 1);
        }
        // Removed verbose output
    }
    else if (directive == "root") {
        iss >> location.root;
        if (!location.root.empty() && location.root[location.root.length() - 1] == ';') {
            location.root = location.root.substr(0, location.root.length() - 1);
        }
        // Removed verbose output
    }
    else if (directive == "autoindex") {
        std::string value;
        iss >> value;
        if (!value.empty() && value[value.length() - 1] == ';') {
            value = value.substr(0, value.length() - 1);
        }
        location.autoindex = (value == "on" || value == "true");
        // Removed verbose output
    }
    else if (directive == "cgi_extension") {
        std::string extension, executor;
        iss >> extension >> executor;
        if (!executor.empty() && executor[executor.length() - 1] == ';') {
            executor = executor.substr(0, executor.length() - 1);
        }
        location.cgi_extensions[extension] = executor;
        // Removed verbose output
    }
    else if (directive == "return" || directive == "redirect") {
        int code;
        std::string url;
        iss >> code >> url;
        if (!url.empty() && url[url.length() - 1] == ';') {
            url = url.substr(0, url.length() - 1);
        }
        if (validateRedirectCode(code)) {
            location.redirect_code = code;
            location.redirect_url = url;
            // Removed verbose output
        } else {
            std::cout << "    Warning: Invalid redirect code " << code << std::endl;
        }
    }
}

// Validation methods
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

bool ConfigParser::validateConfig(const std::vector<ServerConfig>& servers) {
    if (servers.empty()) {
        std::cout << "Error: No valid server configurations found" << std::endl;
        return false;
    }
    
    checkDuplicatePorts(servers);
    return true;
}