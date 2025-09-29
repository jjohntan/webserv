#include "cgi_helper.hpp"
#include "cgi.hpp"
#include <cerrno>
#include <signal.h>

// Global utility function
std::string intToString(int value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

namespace CGIHelper {

// Environment setup helpers
void setupEnvironment(std::map<std::string, std::string>& env_vars, const HTTPRequest& request, const std::string& script_path, const std::string& query_string, const std::string& server_name, int server_port) {
    env_vars.clear();
    setupStandardCGIVars(env_vars, request, script_path, query_string, server_name, server_port);
    setupHTTPHeaders(env_vars, request);
}

void setupStandardCGIVars(std::map<std::string, std::string>& env_vars, const HTTPRequest& request, const std::string& script_path, const std::string& query_string, const std::string& server_name, int server_port) {
    //  HTTPRequest getters (from nelson)
    addEnvironmentVar(env_vars, "REQUEST_METHOD", request.getMethod());
    addEnvironmentVar(env_vars, "SCRIPT_NAME", script_path);
    addEnvironmentVar(env_vars, "PATH_INFO", script_path);
    addEnvironmentVar(env_vars, "QUERY_STRING", query_string);
    addEnvironmentVar(env_vars, "SERVER_PROTOCOL", request.getVersion());
    addEnvironmentVar(env_vars, "GATEWAY_INTERFACE", "CGI/1.1");
    addEnvironmentVar(env_vars, "SERVER_SOFTWARE", "webserv/1.0");
    
    // Server information - use actual server name and port
    addEnvironmentVar(env_vars, "SERVER_NAME", server_name);
    addEnvironmentVar(env_vars, "SERVER_PORT", intToString(server_port));
    addEnvironmentVar(env_vars, "REMOTE_ADDR", "127.0.0.1");
    
    // Content length and type
    const std::map<std::string, std::string>& headers = request.getHeaderMap();
    std::map<std::string, std::string>::const_iterator ct_it = headers.find("content-type");
    
    // Debug: Print all headers
    std::cout << "[DEBUG] CGI setupStandardCGIVars - All headers:" << std::endl;
    for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it) {
        std::cout << "[DEBUG] Header: '" << it->first << "' = '" << it->second << "'" << std::endl;
    }
    
    if (request.getMethod() == "POST") {
        const std::vector<char>& body = request.getBodyVector();
        addEnvironmentVar(env_vars, "CONTENT_LENGTH", intToString(body.size()));
        
        // Get content type from headers
        if (ct_it != headers.end()) {
            addEnvironmentVar(env_vars, "CONTENT_TYPE", ct_it->second);
            std::cout << "[DEBUG] CGI setupStandardCGIVars - Setting CONTENT_TYPE for POST: " << ct_it->second << std::endl;
        } else {
            addEnvironmentVar(env_vars, "CONTENT_TYPE", "application/x-www-form-urlencoded");
            std::cout << "[DEBUG] CGI setupStandardCGIVars - Using default CONTENT_TYPE for POST" << std::endl;
        }
    } else {
        addEnvironmentVar(env_vars, "CONTENT_LENGTH", "0");
        
        // Set CONTENT_TYPE if present in headers, even for non-POST requests
        if (ct_it != headers.end()) {
            addEnvironmentVar(env_vars, "CONTENT_TYPE", ct_it->second);
            std::cout << "[DEBUG] CGI setupStandardCGIVars - Setting CONTENT_TYPE for " << request.getMethod() << ": " << ct_it->second << std::endl;
        } else {
            std::cout << "[DEBUG] CGI setupStandardCGIVars - No CONTENT_TYPE header found for " << request.getMethod() << " request" << std::endl;
        }
    }
}

void setupHTTPHeaders(std::map<std::string, std::string>& env_vars, const HTTPRequest& request) {
    const std::map<std::string, std::string>& headers = request.getHeaderMap();
    
    // Convert HTTP headers to CGI environment variables
    for (std::map<std::string, std::string>::const_iterator it = headers.begin();
         it != headers.end(); ++it) {
        std::string key = "HTTP_" + toUpperCase(it->first);
        addEnvironmentVar(env_vars, key, it->second);
    }
}

void addEnvironmentVar(std::map<std::string, std::string>& env_vars, const std::string& key, const std::string& value) {
    env_vars[key] = value;
}

// Extract file extension from path
std::string getFileExtension(const std::string& filepath) {
    size_t dot_pos = filepath.find_last_of('.');
    if (dot_pos != std::string::npos && dot_pos < filepath.length() - 1) {
        return filepath.substr(dot_pos);
    }
    return "";
}

bool isCGIScript(const std::string& filepath, const std::map<std::string, std::string>& cgi_extensions) {
    std::string extension = getFileExtension(filepath);
    return cgi_extensions.find(extension) != cgi_extensions.end();
}

/*
check if the file extension is in the cgi_extensions map

-> something like this  ( cgi_extensions[".py"] = "/usr/bin/python3";)
if extension not found, return empty string
*/
std::string findCGIExecutor(const std::string& extension, const std::map<std::string, std::string>& cgi_extensions) {
    std::map<std::string, std::string>::const_iterator it = cgi_extensions.find(extension);
    if (it != cgi_extensions.end()) {
        return it->second;
    }
    return "";
}

std::string normalizePath(const std::string& path) {
    std::string result = path;
    
    // Remove duplicate slashes
    size_t pos = 0;
    while ((pos = result.find("//", pos)) != std::string::npos) {
        result.erase(pos, 1);
    }
    
    // Remove duplicate "./cgi_bin" if it appears twice
    pos = result.find("./cgi_bin/./cgi_bin");
    if (pos != std::string::npos) {
        result.erase(pos, 11); // Remove first "./cgi_bin/"
    }
    
    return result;
}

// Output processing helpers
void parseOutput(const std::string& output, CGIResult& result) {
    if (output.empty()) {
        result.success = false;
        result.status_code = 500;
        result.status_message = getStatusMessage(500);
        result.headers = "Content-Type: text/html";
        result.body = "<html><body><h1>500 Internal Server Error</h1><p>CGI script produced no output.</p></body></html>";
        result.content = result.headers + "\r\n\r\n" + result.body;
        return;
    }
    
    result.output = output;  // full raw output
    
    std::cout << "[DEBUG] CGI parseOutput - Raw output length: " << output.length() << std::endl;
    
    // Find the double CRLF that separates headers from body
    size_t header_end = output.find("\r\n\r\n");
    size_t separator_length = 4; // "\r\n\r\n"
    
    if (header_end == std::string::npos) {
        header_end = output.find("\n\n");
        if (header_end != std::string::npos) {
            separator_length = 2; // "\n\n"
        }
    }
    
    std::cout << "[DEBUG] CGI parseOutput - Header end position: " << header_end << std::endl;
    std::cout << "[DEBUG] CGI parseOutput - Separator length: " << separator_length << std::endl;
    
    if (header_end != std::string::npos) {
        result.headers = output.substr(0, header_end);
        size_t body_start = header_end + separator_length;
        if (body_start < output.length()) {
            result.body = output.substr(body_start);
        }
        std::cout << "[DEBUG] CGI parseOutput - Parsed headers: '" << result.headers << "'" << std::endl;
        std::cout << "[DEBUG] CGI parseOutput - Body length: " << result.body.length() << std::endl;
    } else {
        // No headers found, treat entire output as body
        result.body = output;
        result.headers = "Content-Type: text/html";
        std::cout << "[DEBUG] CGI parseOutput - No headers found, using default" << std::endl;
    }
    
    // Extract status code from headers (look for Status: header)
    result.status_code = extractStatusFromHeaders(result.headers);
    
    // Generate status message based on status code
    result.status_message = getStatusMessage(result.status_code);
    
    // Create content (headers + body)
    result.content = result.headers + "\r\n\r\n" + result.body;
    
    result.success = true;

    std::cout << "[DEBUG] CGI executeCGI - Socket FD: " << result.socketFD << std::endl;
    std::cout << "[DEBUG] CGI parseOutput - Status code: " << result.status_code << std::endl;
    std::cout << "[DEBUG] CGI parseOutput - Status message: " << result.status_message << std::endl;
    std::cout << "[DEBUG] CGI parseOutput - Content length: " << result.content.length() << std::endl;
    std::cout << "[DEBUG] CGI parseOutput - Content : " << result.content << std::endl;
}

int extractStatusFromHeaders(const std::string& headers) {
    std::istringstream header_stream(headers);
    std::string line;
    
    while (std::getline(header_stream, line)) {
        // Remove carriage return if present
        if (!line.empty() && line[line.length() - 1] == '\r') {
            line.erase(line.length() - 1);
        }
        
        // Look for Status header (case-insensitive)
        if (line.length() >= 7) {
            std::string prefix = line.substr(0, 7);
            // Convert to lowercase manually for C++98 compatibility
            for (size_t i = 0; i < prefix.length(); ++i) {
                prefix[i] = std::tolower(prefix[i]);
            }
            
            if (prefix == "status:") {
                std::string status_part = line.substr(7);
                // Trim leading spaces
                size_t start = status_part.find_first_not_of(" \t");
                if (start != std::string::npos) {
                    status_part = status_part.substr(start);
                    // Extract just the numeric part
                    std::istringstream status_stream(status_part);
                    int status_code;
                    if (status_stream >> status_code) {
                        return status_code;
                    }
                }
            }
        }
    }
    return 200; // Default to 200 if no Status header found
}

std::string getStatusMessage(int status_code) {
    switch (status_code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        default: return "Unknown";
    }
}

// Process management helpers
bool waitForChildWithTimeout(pid_t pid, int timeout_seconds, int& status) {
    pid_t result;
    int timeout_count = 0;
    const int max_checks = timeout_seconds * 10; // Check every 100ms
    
    std::cout << "[DEBUG] CGI waiting for child process " << pid << " with timeout " << timeout_seconds << " seconds" << std::endl;
    
    // Use a polling approach with precise timing
    while (timeout_count < max_checks) {
        result = waitpid(pid, &status, WNOHANG);
        
        if (result == pid) {
            // Child process has exited
            std::cout << "[DEBUG] CGI child process " << pid << " exited with status: " << status << std::endl;
            return true;
        } else if (result == -1) {
            std::cout << "[DEBUG] CGI waitpid failed with errno: " << errno << std::endl;
            return false;
        }
        
        // Child is still running, wait 100ms and check again
        usleep(100000); // 100ms
        timeout_count++;
        
        // Print progress every second
        if (timeout_count % 10 == 0) {
            std::cout << "[DEBUG] CGI still waiting... " << (timeout_count / 10) << " seconds elapsed" << std::endl;
        }
    }
    
    // Timeout occurred, kill the child process
    std::cout << "[DEBUG] CGI timeout after " << timeout_seconds << " seconds - killing child process " << pid << std::endl;
    kill(pid, SIGKILL);
    
    // Wait for the process to actually die
    int kill_status;
    waitpid(pid, &kill_status, 0);
    status = kill_status;
    
    std::cout << "[DEBUG] CGI child process " << pid << " killed with status: " << status << std::endl;
    return false; // Timeout
}

char** createEnvArray(const std::map<std::string, std::string>& env_vars) {
    char** env = new char*[env_vars.size() + 1];
    size_t i = 0;
    
    for (std::map<std::string, std::string>::const_iterator it = env_vars.begin();
         it != env_vars.end(); ++it, ++i) {
        std::string env_string = it->first + "=" + it->second;
        env[i] = new char[env_string.length() + 1];
        std::strcpy(env[i], env_string.c_str());
    }
    env[i] = NULL;
    
    return env;
}

void freeEnvArray(char** env) {
    if (!env) return;
    
    for (size_t i = 0; env[i]; ++i) {
        delete[] env[i];
    }
    delete[] env;
}


std::string toUpperCase(const std::string& str) {
    std::string result = str;
    for (size_t i = 0; i < result.length(); ++i) {
        if (result[i] == '-') {
            result[i] = '_';
        } else {
            result[i] = std::toupper(result[i]);
        }
    }
    return result;
}

} 
