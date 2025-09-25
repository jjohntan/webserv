#include "cgi.hpp"
#include <cerrno>
#include <signal.h>

std::string intToString(int value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

//constructor
CGIHandler::CGIHandler() {}

//destructor
CGIHandler::~CGIHandler() {}

bool CGIHandler::needsCGI(const std::string& filepath, const std::map<std::string, std::string>& cgi_extensions) {
    return isCGIScript(filepath, cgi_extensions);
}

std::string CGIHandler::extractScriptName(const std::string& uri) {
    size_t query_pos = uri.find('?');
    if (query_pos != std::string::npos) {
        return uri.substr(0, query_pos);
    }
    return uri;
}

std::string CGIHandler::extractQueryString(const std::string& path) {
    size_t question_pos = path.find('?');
    if (question_pos != std::string::npos && question_pos + 1 < path.length()) {
        return path.substr(question_pos + 1);
    }
    return "";
}

void CGIHandler::clearEnvironment() {
    env_vars.clear();
}

// read file extension
std::string CGIHandler::getFileExtension(const std::string& filepath) {
    size_t dot_pos = filepath.find_last_of('.');
    if (dot_pos != std::string::npos && dot_pos < filepath.length() - 1) {
        return filepath.substr(dot_pos);
    }
    return "";
}

bool CGIHandler::isCGIScript(const std::string& filepath, const std::map<std::string, std::string>& cgi_extensions) {
    std::string extension = getFileExtension(filepath);
    return cgi_extensions.find(extension) != cgi_extensions.end();
}

/*
check if the file extension is in the cgi_extensions map

-> something like this  ( cgi_extensions[".py"] = "/usr/bin/python3";)
if extension not found, return empty string
*/
std::string CGIHandler::findCGIExecutor(const std::string& extension, const std::map<std::string, std::string>& cgi_extensions) {
    std::map<std::string, std::string>::const_iterator it = cgi_extensions.find(extension);
    if (it != cgi_extensions.end()) {
        return it->second;
    }
    return "";
}

void CGIHandler::addEnvironmentVar(const std::string& key, const std::string& value) {
    env_vars[key] = value;
}

std::string CGIHandler::toUpperCase(const std::string& str) {
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

std::string CGIHandler::normalizePath(const std::string& path) {
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

char** CGIHandler::createEnvArray() {
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

void CGIHandler::freeEnvArray(char** env) {
    if (!env) return;
    
    for (size_t i = 0; env[i]; ++i) {
        delete[] env[i];
    }
    delete[] env;
}

void CGIHandler::setupHTTPHeaders(const HTTPRequest& request) {
    const std::map<std::string, std::string>& headers = request.getHeaderMap();
    
    // Convert HTTP headers to CGI environment variables
    for (std::map<std::string, std::string>::const_iterator it = headers.begin();
         it != headers.end(); ++it) {
        std::string key = "HTTP_" + toUpperCase(it->first);
        addEnvironmentVar(key, it->second);
    }
}

// this is where cgi function start ( main ) =================================
void CGIHandler::setupEnvironment(const HTTPRequest& request, const std::string& script_path, const std::string& query_string) {
    env_vars.clear();
    setupStandardCGIVars(request, script_path, query_string);
    setupHTTPHeaders(request);
}

void CGIHandler::setupStandardCGIVars(const HTTPRequest& request, const std::string& script_path, const std::string& query_string) {
    //  HTTPRequest getters (from nelson)
    addEnvironmentVar("REQUEST_METHOD", request.getMethod());
    addEnvironmentVar("SCRIPT_NAME", script_path);
    addEnvironmentVar("PATH_INFO", script_path);
    addEnvironmentVar("QUERY_STRING", query_string);
    addEnvironmentVar("SERVER_PROTOCOL", request.getVersion());
    addEnvironmentVar("GATEWAY_INTERFACE", "CGI/1.1");
    addEnvironmentVar("SERVER_SOFTWARE", "webserv/1.0");
    
    // Server information 
    addEnvironmentVar("SERVER_NAME", "localhost");
    addEnvironmentVar("SERVER_PORT", "8080");
    addEnvironmentVar("REMOTE_ADDR", "127.0.0.1");
    
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
        addEnvironmentVar("CONTENT_LENGTH", intToString(body.size()));
        
        // Get content type from headers
        if (ct_it != headers.end()) {
            addEnvironmentVar("CONTENT_TYPE", ct_it->second);
            std::cout << "[DEBUG] CGI setupStandardCGIVars - Setting CONTENT_TYPE for POST: " << ct_it->second << std::endl;
        } else {
            addEnvironmentVar("CONTENT_TYPE", "application/x-www-form-urlencoded");
            std::cout << "[DEBUG] CGI setupStandardCGIVars - Using default CONTENT_TYPE for POST" << std::endl;
        }
    } else {
        addEnvironmentVar("CONTENT_LENGTH", "0");
        
        // Set CONTENT_TYPE if present in headers, even for non-POST requests
        if (ct_it != headers.end()) {
            addEnvironmentVar("CONTENT_TYPE", ct_it->second);
            std::cout << "[DEBUG] CGI setupStandardCGIVars - Setting CONTENT_TYPE for " << request.getMethod() << ": " << ct_it->second << std::endl;
        } else {
            std::cout << "[DEBUG] CGI setupStandardCGIVars - No CONTENT_TYPE header found for " << request.getMethod() << " request" << std::endl;
        }
    }
}

// Helper method to extract status code from CGI headers
int CGIHandler::extractStatusFromHeaders(const std::string& headers) {
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

// Helper method to generate status message from status code
std::string CGIHandler::getStatusMessage(int status_code) {
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

/*
split into
result.headers
result.body
Marks result.success = true
*/
void CGIHandler::parseOutput(const std::string& output, CGIResult& result) {
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

CGIResult CGIHandler::executeCGI(const HTTPRequest& request, 
                                const std::string& script_path,
                                const std::map<std::string, std::string>& cgi_extensions,
                                const std::string& working_directory) {
    CGIResult result;
    
    // Set the socket FD from the request
    result.socketFD = request.getSocketFD();
    
    std::cout << "[DEBUG] CGI executeCGI - Original script_path: " << script_path << std::endl;
    std::cout << "[DEBUG] CGI executeCGI - Working directory: " << working_directory << std::endl;
     
    /*
    clean up the path to remove things like:
            Double slashes (//)
            . and .. references
            Any accidental duplicates (cgi_bin/cgi_bin/...)
    */
    std::string normalized_script_path = normalizePath(script_path);
    std::cout << "[DEBUG] CGI executeCGI - Normalized script_path: " << normalized_script_path << std::endl;
    
    // check whether is CGI script (.py) , if not then 404
    if (!isCGIScript(normalized_script_path, cgi_extensions)) {
        result.success = false;
        result.status_code = 404;
        result.status_message = getStatusMessage(404);
        result.headers = "Content-Type: text/html";
        result.body = "<html><body><h1>404 Not Found</h1><p>The requested CGI script was not found.</p></body></html>";
        result.content = result.headers + "\r\n\r\n" + result.body;
        return result;
    }
    
    // Check if the script file actually exists and is executable
    std::string full_script_path = working_directory + "/" + normalized_script_path;
    if (access(full_script_path.c_str(), F_OK) != 0) {
        result.success = false;
        result.status_code = 404;
        result.status_message = getStatusMessage(404);
        result.headers = "Content-Type: text/html";
        result.body = "<html><body><h1>404 Not Found</h1><p>CGI script file does not exist.</p></body></html>";
        result.content = result.headers + "\r\n\r\n" + result.body;
        return result;
    }
    
    if (access(full_script_path.c_str(), X_OK) != 0) {
        result.success = false;
        result.status_code = 403;
        result.status_message = getStatusMessage(403);
        result.headers = "Content-Type: text/html";
        result.body = "<html><body><h1>403 Forbidden</h1><p>CGI script is not executable.</p></body></html>";
        result.content = result.headers + "\r\n\r\n" + result.body;
        return result;
    }
    
    // Find the appropriate executor
    std::string extension = getFileExtension(normalized_script_path);
    std::string executor = findCGIExecutor(extension, cgi_extensions);
    std::cout << "[DEBUG] CGI executeCGI - Extension: " << extension << std::endl;
    std::cout << "[DEBUG] CGI executeCGI - Executor: " << executor << std::endl;
    
    if (executor.empty()) {
        result.success = false;
        result.status_code = 500;
        result.status_message = getStatusMessage(500);
        result.headers = "Content-Type: text/html";
        result.body = "<html><body><h1>500 Internal Server Error</h1><p>No executor found for CGI script.</p></body></html>";
        result.content = result.headers + "\r\n\r\n" + result.body;
        return result;
    }
    
    /*
    /cgi-bin/python/hello.py?name=World
    Script path: /cgi-bin/python/hello.py
    Query string: name=World
    */
    std::string query_string = extractQueryString(request.getPath());
    
    // Setup environment
    /*
    prepares variables like:

    REQUEST_METHOD=GET
    QUERY_STRING=name=World
    CONTENT_LENGTH=...
    SCRIPT_FILENAME=/path/to/hello.py
    */
    setupEnvironment(request, normalized_script_path, query_string);
    
    /*
    input_pipe → parent writes POST body, child reads it as stdin.
    output_pipe → child writes CGI output, parent reads it.
    */
    int input_pipe[2];
    int output_pipe[2];
    
    if (pipe(input_pipe) == -1 || pipe(output_pipe) == -1) {
        result.success = false;
        result.status_code = 500;
        result.status_message = getStatusMessage(500);
        result.headers = "Content-Type: text/html";
        result.body = "<html><body><h1>500 Internal Server Error</h1><p>Failed to create pipes for CGI execution.</p></body></html>";
        result.content = result.headers + "\r\n\r\n" + result.body;
        return result;
    }
    
    // Create environment array
    // -> to convert std::map of environment vars into a char*[] format for execve.
    char** env = createEnvArray();
    
    /*
    Two process now 
    Child: runs the CGI script
    Parent: waits and reads the outp
    */
    pid_t pid = fork();
    if (pid == -1) {
        close(input_pipe[0]);
        close(input_pipe[1]);
        close(output_pipe[0]);
        close(output_pipe[1]);
        freeEnvArray(env);
        result.success = false;
        result.status_code = 500;
        result.status_message = getStatusMessage(500);
        result.headers = "Content-Type: text/html";
        result.body = "<html><body><h1>500 Internal Server Error</h1><p>Failed to fork process for CGI execution.</p></body></html>";
        result.content = result.headers + "\r\n\r\n" + result.body;
        return result;
    }
    
    if (pid == 0) {
        // Child process
        
        std::string final_script_path = normalized_script_path;
        
        // Change to working directory if specified
        if (!working_directory.empty() && chdir(working_directory.c_str()) != 0) {
            exit(1);
        }
        
        // If we changed to a working directory, adjust the script path to be relative to that directory
        if (!working_directory.empty()) {
            // Remove the working directory part from the script path if it's there
            if (final_script_path.find(working_directory + "/") == 0) {
                final_script_path = final_script_path.substr(working_directory.length() + 1);
            } else if (final_script_path.find("./cgi_bin/") == 0) {
                final_script_path = final_script_path.substr(2); // Remove "./" but keep "cgi_bin/"
            } else if (final_script_path.find("./") == 0) {
                final_script_path = final_script_path.substr(2); // Remove "./"
            }
        }
        
        std::cout << "[DEBUG] CGI executeCGI - Final script path for execution: " << final_script_path << std::endl;

        
        // Redirect stdin and stdout
        dup2(input_pipe[0], STDIN_FILENO);
        dup2(output_pipe[1], STDOUT_FILENO);
        
        // Close unused pipe ends
        close(input_pipe[1]);
        close(output_pipe[0]);
        close(input_pipe[0]);
        close(output_pipe[1]);
        
        // Prepare arguments - script path as first argument as per webserv requirements
        char* args[3];
        args[0] = const_cast<char*>(executor.c_str());
        args[1] = const_cast<char*>(final_script_path.c_str());
        args[2] = NULL;
        
        // all cgi done, parent can start to read the cgi for output
        execve(executor.c_str(), args, env);
        
        exit(1);  // execve failed
    } else {
        // Parent process
        
        // Close unused pipe ends
        close(input_pipe[0]);
        close(output_pipe[1]);
        
        // Send request body to CGI script (for POST requests)
        if (request.getMethod() == "POST") {
            const std::vector<char>& body = request.getBodyVector();
            if (!body.empty()) {
                ssize_t bytes_written = write(input_pipe[1], &body[0], body.size());
                (void)bytes_written; // Avoid unused variable warning
            }
        }
        close(input_pipe[1]);  // Close input to signal EOF
        
        // Read response from CGI script with timeout
        std::string output;
        char buffer[4096];
        ssize_t bytes_read;
        int timeout_count = 0;
        const int max_timeout = 100; // 10 seconds (100 * 100ms)
        
        std::cout << "[DEBUG] CGI starting to read output with timeout" << std::endl;
        
        while (timeout_count < max_timeout) {
            // Use select to check if data is available with timeout
            fd_set readfds;
            struct timeval timeout;
            
            FD_ZERO(&readfds);
            FD_SET(output_pipe[0], &readfds);
            timeout.tv_sec = 0;
            timeout.tv_usec = 100000; // 100ms
            
            int select_result = select(output_pipe[0] + 1, &readfds, NULL, NULL, &timeout);
            
            if (select_result > 0 && FD_ISSET(output_pipe[0], &readfds)) {
                // Data is available, read it
                bytes_read = read(output_pipe[0], buffer, sizeof(buffer));
                if (bytes_read > 0) {
                    output.append(buffer, bytes_read);
                    timeout_count = 0; // Reset timeout counter when we get data
                } else if (bytes_read == 0) {
                    // EOF reached
                    break;
                }
            } else if (select_result == 0) {
                // Timeout - no data available
                timeout_count++;
                if (timeout_count % 10 == 0) {
                    std::cout << "[DEBUG] CGI pipe read timeout... " << (timeout_count / 10) << " seconds elapsed" << std::endl;
                }
            } else {
                // Error in select
                break;
            }
        }
        
        close(output_pipe[0]);
        
        if (timeout_count >= max_timeout) {
            std::cout << "[DEBUG] CGI pipe read timeout after 10 seconds" << std::endl;
        }

        std::cout << "output =" << output << std::endl;
        
        // Wait for child process to complete with timeout
        int status = 0;
        if (!waitForChildWithTimeout(pid, 10, status)) { // 10 second timeout to match test expectations
            result.success = false;
            result.status_code = 504; // Gateway Timeout
            result.status_message = "Gateway Timeout";
            result.headers = "Content-Type: text/html";
            result.body = "<html><body><h1>504 Gateway Timeout</h1><p>CGI script execution timed out.</p></body></html>";
            result.content = result.headers + "\r\n\r\n" + result.body;
            freeEnvArray(env);
            return result;
        }
        
        // Clean up
        freeEnvArray(env);
        
        // Check if CGI script executed successfully
        if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            if (exit_status != 0) {
                result.success = false;
                result.status_code = 500;
                result.status_message = getStatusMessage(500);
                result.headers = "Content-Type: text/html";
                result.body = "<html><body><h1>500 Internal Server Error</h1><p>CGI script execution failed with exit code " + intToString(exit_status) + ".</p></body></html>";
                result.content = result.headers + "\r\n\r\n" + result.body;
                return result;
            }
        } else if (WIFSIGNALED(status)) {
            int signal_num = WTERMSIG(status);
            result.success = false;
            result.status_code = 500;
            result.status_message = getStatusMessage(500);
            result.headers = "Content-Type: text/html";
            result.body = "<html><body><h1>500 Internal Server Error</h1><p>CGI script was terminated by signal " + intToString(signal_num) + ".</p></body></html>";
            result.content = result.headers + "\r\n\r\n" + result.body;
            return result;
        }
        
        // Parse the output
        parseOutput(output, result);

        return result;
    }
}

bool CGIHandler::waitForChildWithTimeout(pid_t pid, int timeout_seconds, int& status) {
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