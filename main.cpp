#include "server.hpp"
#include "config_files/config.hpp"
#include <iostream>
#include <cstring>
#include <sstream>

void printUsage(const char* program_name) {
    std::cout << "Usage:" << std::endl;
    std::cout << "  " << program_name << " [config_file]                  - Run server with config file" << std::endl;
}


int runWithConfig(const std::string& config_file) {
    std::cout << "Reading config file: " << config_file << std::endl;
    
    ConfigParser parser;
    std::vector<ServerConfig> servers = parser.parseConfig(config_file);
    
    if (servers.empty()) {
        std::cout << "No servers found in config file!" << std::endl;
        return 1;
    }
    
    std::cout << "Successfully parsed " << servers.size() << " server(s)" << std::endl;
    parser.printConfig(servers);
    
    // Use the first server configuration
    const ServerConfig& server_config = servers[0];
    
    std::cout << "\nStarting web server with configuration..." << std::endl;
    std::cout << "Port: " << server_config.port << std::endl;
    std::cout << "Document root: " << server_config.root << std::endl;
    
    Server server(server_config.port, server_config.root);
    
    if (!server.start()) {
        return 1;
    }
    
    std::cout << "Web server started successfully!" << std::endl;
    std::cout << "Visit http://localhost:" << server_config.port << " to see your website!" << std::endl;
    std::cout << "Press Ctrl+C to stop the server" << std::endl;
    
    server.run();
    
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    std::string first_arg = argv[1];
    
    
    return runWithConfig(first_arg);
    
}