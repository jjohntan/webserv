#include "cgi.hpp"
#include "cgi_helper.hpp"


//constructor
CGIHandler::CGIHandler() {}

//destructor
CGIHandler::~CGIHandler() {}

bool CGIHandler::needsCGI(const std::string& filepath, const std::map<std::string, std::string>& cgi_extensions) {
    return CGIHelper::isCGIScript(filepath, cgi_extensions);
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
    std::string normalized_script_path = CGIHelper::normalizePath(script_path);
    std::cout << "[DEBUG] CGI executeCGI - Normalized script_path: " << normalized_script_path << std::endl;
    
    // check whether is CGI script (.py) , if not then 404
    if (!CGIHelper::isCGIScript(normalized_script_path, cgi_extensions)) {
        result.success = false;
        result.status_code = 404;
        result.status_message = CGIHelper::getStatusMessage(404);
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
        result.status_message = CGIHelper::getStatusMessage(404);
        result.headers = "Content-Type: text/html";
        result.body = "<html><body><h1>404 Not Found</h1><p>CGI script file does not exist.</p></body></html>";
        result.content = result.headers + "\r\n\r\n" + result.body;
        return result;
    }
    
    if (access(full_script_path.c_str(), X_OK) != 0) {
        result.success = false;
        result.status_code = 403;
        result.status_message = CGIHelper::getStatusMessage(403);
        result.headers = "Content-Type: text/html";
        result.body = "<html><body><h1>403 Forbidden</h1><p>CGI script is not executable.</p></body></html>";
        result.content = result.headers + "\r\n\r\n" + result.body;
        return result;
    }
    
    // Find the appropriate executor
    std::string extension = CGIHelper::getFileExtension(normalized_script_path);
    std::string executor = CGIHelper::findCGIExecutor(extension, cgi_extensions);
    std::cout << "[DEBUG] CGI executeCGI - Extension: " << extension << std::endl;
    std::cout << "[DEBUG] CGI executeCGI - Executor: " << executor << std::endl;
    
    if (executor.empty()) {
        result.success = false;
        result.status_code = 500;
        result.status_message = CGIHelper::getStatusMessage(500);
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
    CGIHelper::setupEnvironment(env_vars, request, normalized_script_path, query_string);
    
    /*
    input_pipe → parent writes POST body, child reads it as stdin.
    output_pipe → child writes CGI output, parent reads it.
    */
    int input_pipe[2];
    int output_pipe[2];
    
    if (pipe(input_pipe) == -1 || pipe(output_pipe) == -1) {
        result.success = false;
        result.status_code = 500;
        result.status_message = CGIHelper::getStatusMessage(500);
        result.headers = "Content-Type: text/html";
        result.body = "<html><body><h1>500 Internal Server Error</h1><p>Failed to create pipes for CGI execution.</p></body></html>";
        result.content = result.headers + "\r\n\r\n" + result.body;
        return result;
    }
    
    // Create environment array
    // -> to convert std::map of environment vars into a char*[] format for execve.
    char** env = CGIHelper::createEnvArray(env_vars);
    
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
        CGIHelper::freeEnvArray(env);
        result.success = false;
        result.status_code = 500;
        result.status_message = CGIHelper::getStatusMessage(500);
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
        
        //loop
        while (timeout_count < max_timeout) {
            // Use select to check if data is available with timeout
            fd_set readfds;                    // File descriptor set for select()
            struct timeval timeout;            // Timeout structure
            
            FD_ZERO(&readfds);                // Clear the file descriptor set
            FD_SET(output_pipe[0], &readfds); // Add output pipe read end to set
            timeout.tv_sec = 0;               // No seconds timeout
            timeout.tv_usec = 100000;         // 100ms timeout
            
            int select_result = select(output_pipe[0] + 1, &readfds, NULL, NULL, &timeout);
            
            if (select_result > 0 && FD_ISSET(output_pipe[0], &readfds)) {
                // >0 data is available to read; =0 timeout; <0 error
                bytes_read = read(output_pipe[0], buffer, sizeof(buffer));
                if (bytes_read > 0) {
                    output.append(buffer, bytes_read);
                } else if (bytes_read == 0) {
                    // EOF reached, child close pipe and end loop
                    break;
                }
            } else if (select_result == 0) {
                // Timeout - no data available
                if (timeout_count % 10 == 0) {
                    std::cout << "[DEBUG] CGI pipe read timeout... " << (timeout_count / 10) << " seconds elapsed" << std::endl;
                }
            } else {
                // Error in select
                break;
            }
            
            // Always increment timeout counter to track total elapsed time
            timeout_count++;
        }
        
        close(output_pipe[0]);
        
        //error handling when cgi timeout >10
        if (timeout_count >= max_timeout) {
            std::cout << "[DEBUG] CGI pipe read timeout after 10 seconds" << std::endl;
            kill(pid, SIGKILL);
            int kill_status;
            waitpid(pid, &kill_status, 0); // waits for the child process to actually terminate
            
            result.success = false;
            result.status_code = 504; // Gateway Timeout
            result.status_message = "Gateway Timeout";
            result.headers = "Content-Type: text/html";
            result.body = "<html><body><h1>504 Gateway Timeout</h1><p>CGI script execution timed out.</p></body></html>";
            result.content = result.headers + "\r\n\r\n" + result.body;
            CGIHelper::freeEnvArray(env);
            return result;
        }

        std::cout << "output =" << output << std::endl;
        
        // Wait for CGI script process to actually exit, avoid hangs
        int status = 0;
        if (!CGIHelper::waitForChildWithTimeout(pid, 10, status)) { // 10 second timeout to match test expectations
            result.success = false;
            result.status_code = 504; // Gateway Timeout
            result.status_message = "Gateway Timeout";
            result.headers = "Content-Type: text/html";
            result.body = "<html><body><h1>504 Gateway Timeout</h1><p>CGI script execution timed out.</p></body></html>";
            result.content = result.headers + "\r\n\r\n" + result.body;
            CGIHelper::freeEnvArray(env);
            return result;
        }
        
        // Clean up
        CGIHelper::freeEnvArray(env);
        
        // Check if CGI script executed successfully
        if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            if (exit_status != 0) {
                result.success = false;
                result.status_code = 500;
                result.status_message = CGIHelper::getStatusMessage(500);
                result.headers = "Content-Type: text/html";
                result.body = "<html><body><h1>500 Internal Server Error</h1><p>CGI script execution failed with exit code " + intToString(exit_status) + ".</p></body></html>";
                result.content = result.headers + "\r\n\r\n" + result.body;
                return result;
            }
        } else if (WIFSIGNALED(status)) {
            int signal_num = WTERMSIG(status);
            result.success = false;
            result.status_code = 500;
            result.status_message = CGIHelper::getStatusMessage(500);
            result.headers = "Content-Type: text/html";
            result.body = "<html><body><h1>500 Internal Server Error</h1><p>CGI script was terminated by signal " + intToString(signal_num) + ".</p></body></html>";
            result.content = result.headers + "\r\n\r\n" + result.body;
            return result;
        }
        
        // Parse the output
        CGIHelper::parseOutput(output, result);

        return result;
    }
}
