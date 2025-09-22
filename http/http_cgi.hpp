#ifndef HTTP_CGI_HPP
#define HTTP_CGI_HPP

#include "HTTPRequest/HTTPRequest.hpp"
#include "../cgi_handler/cgi.hpp"
#include "../config_files/config.hpp"
#include <map>
#include <string>
#include <vector>

// [CHANGE] needed for stat()
#include <sys/stat.h>
// [CHANGE] needed for std::remove
#include <cstdio>

#include <ctime>

// [ADD] forward declaration to avoid heavy include in header
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
void handleRequestProcessing(const HTTPRequest& request, int socketFD, const std::vector<ServerConfig>& servers, Server& srv); // [CHANGE]

// Save an upload for POST /upload/ using the configured upload_path.
// Supports multipart/form-data (with filename) and application/octet-stream.
// Returns true on success and sets saved_path to the resulting absolute path.
bool saveUploadedBody(const HTTPRequest& req,
                      const Location* loc,
                      const ServerConfig* sc,
                      std::string& saved_path,
                      std::string& error);

// Small helpers
std::string getHeaderCI(const std::map<std::string,std::string>& h, const std::string& key);
std::string joinPath(const std::string& dir, const std::string& base);

#endif
