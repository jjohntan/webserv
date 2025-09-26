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
#include "cgi_helper.hpp"


struct CGIResult {
    std::string output;
    std::string headers;
    std::string body;
    std::string content; //add ( body + header)
    std::string status_message; //add
    int status_code;
    int socketFD; //add
    bool success;
    
    CGIResult() : status_code(200), success(false) {}
};

class CGIHandler {
private:
    std::map<std::string, std::string> env_vars;
    
public:
    CGIHandler();
    ~CGIHandler();
    
    
    CGIResult executeCGI(const HTTPRequest& request, 
                        const std::string& script_path,
                        const std::map<std::string, std::string>& cgi_extensions,
                        const std::string& working_directory);
    
   
    bool needsCGI(const std::string& filepath, const std::map<std::string, std::string>& cgi_extensions);
    std::string extractScriptName(const std::string& uri);
    std::string extractQueryString(const std::string& path);
    void clearEnvironment();
};

#endif