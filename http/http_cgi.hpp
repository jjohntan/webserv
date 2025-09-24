#ifndef HTTP_CGI_HPP
#define HTTP_CGI_HPP

#include "HTTPRequest/HTTPRequest.hpp"
#include "../cgi_handler/cgi.hpp"
#include "../config_files/config.hpp"
#include <map>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <cstdio>

// Forward declaration to avoid circular includes
class Server;

// CGI and file serving functions
CGIResult runCGI(const HTTPRequest& request, const std::string& script_path, const std::map<std::string, std::string>& cgi_extensions, const std::string& working_directory);
std::string serveFile(const std::string& filePath);
std::string generateDirectoryListing(const std::string& dirPath);

// Server configuration helper functions
const ServerConfig* findServerConfig(const HTTPRequest& request, const std::vector<ServerConfig>& servers);
bool isCGIEnabled(const std::string& path, const ServerConfig* server_config);
std::map<std::string, std::string> getCGIExtensions(const std::string& path, const ServerConfig* server_config);

// Main request processing function
void handleRequestProcessing(const HTTPRequest& request, int socketFD, const std::vector<ServerConfig>& servers, Server& srv);

#endif
