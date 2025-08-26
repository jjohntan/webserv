#ifndef CGI_HPP
#define CGI_HPP

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

// C++98 compatible string conversion
std::string intToString(int value);

struct CGIResult {
    std::string output;
    std::string headers;
    std::string body;
    int status_code;
    bool success;
    
    CGIResult() : status_code(200), success(false) {}
};

class CGIHandler {
private:
    std::map<std::string, std::string> env_vars;
    
    // Helper methods
    void setupEnvironment(const HTTPRequest& request, const std::string& script_path, const std::string& query_string);
    void setupStandardCGIVars(const HTTPRequest& request, const std::string& script_path, const std::string& query_string);
    void setupHTTPHeaders(const HTTPRequest& request);
    void addEnvironmentVar(const std::string& key, const std::string& value);
    std::string getFileExtension(const std::string& filepath);
    bool isCGIScript(const std::string& filepath, const std::map<std::string, std::string>& cgi_extensions);
    std::string findCGIExecutor(const std::string& extension, const std::map<std::string, std::string>& cgi_extensions);
    void parseOutput(const std::string& output, CGIResult& result);
    char** createEnvArray();
    void freeEnvArray(char** env);
    std::string toUpperCase(const std::string& str);
    
public:
    CGIHandler();
    ~CGIHandler();
    
    // Main CGI execution method
    CGIResult executeCGI(const HTTPRequest& request, 
                        const std::string& script_path,
                        const std::map<std::string, std::string>& cgi_extensions,
                        const std::string& working_directory);
    
    // Utility methods
    bool needsCGI(const std::string& filepath, const std::map<std::string, std::string>& cgi_extensions);
    std::string extractScriptName(const std::string& uri);
    std::string extractQueryString(const std::string& path);
    void clearEnvironment();
};

#endif // CGI_HPP