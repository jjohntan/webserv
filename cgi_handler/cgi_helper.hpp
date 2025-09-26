#ifndef CGI_HELPER_HPP
#define CGI_HELPER_HPP

#include <string>
#include <map>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <cctype>
#include <cstring>
#include "../http/HTTPRequest/HTTPRequest.hpp"


struct CGIResult;

// Global utility function
std::string intToString(int value);

// Helper functions 
namespace CGIHelper {
  
    void setupEnvironment(std::map<std::string, std::string>& env_vars, const HTTPRequest& request, const std::string& script_path, const std::string& query_string);
    void setupStandardCGIVars(std::map<std::string, std::string>& env_vars, const HTTPRequest& request, const std::string& script_path, const std::string& query_string);
    void setupHTTPHeaders(std::map<std::string, std::string>& env_vars, const HTTPRequest& request);
    void addEnvironmentVar(std::map<std::string, std::string>& env_vars, const std::string& key, const std::string& value);
    

    std::string getFileExtension(const std::string& filepath);
    bool isCGIScript(const std::string& filepath, const std::map<std::string, std::string>& cgi_extensions);
    std::string findCGIExecutor(const std::string& extension, const std::map<std::string, std::string>& cgi_extensions);
    std::string normalizePath(const std::string& path);
    
   
    void parseOutput(const std::string& output, CGIResult& result);
    int extractStatusFromHeaders(const std::string& headers);
    std::string getStatusMessage(int status_code);
    
  
    bool waitForChildWithTimeout(pid_t pid, int timeout_seconds, int& status);
    char** createEnvArray(const std::map<std::string, std::string>& env_vars);
    void freeEnvArray(char** env);
    
    // Utility helpers
    std::string toUpperCase(const std::string& str);
}

#endif
