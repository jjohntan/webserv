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
    ConfigParser parser;
    std::vector<ServerConfig> servers = parser.parseConfig(config_file);
    
    if (servers.empty()) {
        std::cout << "No servers found in config file!" << std::endl;
        return 1;
    }
    
    // Pass all server configs to enable multi-port support
    Server server(servers);
    
    if (!server.start()) {
        return 1;
    }
    
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