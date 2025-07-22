#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <map>

class Server {
private:
    int server_fd;
    int port;
    std::string document_root;
    std::map<int, std::string> error_pages;
    
    void handleRequest(int client_fd);
    void sendErrorResponse(int client_fd, int error_code);
    bool endsWith(const std::string& str, const std::string& suffix);
    std::string getContentType(const std::string& path);
    
public:
    Server(int p, const std::string& root);
    ~Server();
    
    bool start();
    void run();
};

#endif