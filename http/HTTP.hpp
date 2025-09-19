#ifndef HTTP_HPP
# define HTTP_HPP

# define RED "\033[31m"
# define GREEN "\033[32m"
# define BLUE "\033[34m"
# define YELLOW "\033[33m"
# define CYAN "\033[36m"
# define MAGENTA "\033[35m"
# define WHITE "\033[37m"
# define RESET "\033[0m"

# define READ_BYTES 4096

# include "HTTPRequest/HTTPRequest.hpp"
# include "HTTPResponse/HTTPResponse.hpp"
# include "HTTPResponse/ErrorResponse.hpp"
# include "../cgi_handler/cgi.hpp"
# include "../config_files/config.hpp"
# include <map>
# include <string>


// const ServerConfig* getActiveServer(const std::vector<ServerConfig>& serverConfig);
// const Location* getMatchingLocation(const std::string& path,
//                                     const std::vector<ServerConfig>& serverConfig);
const	Location* getMatchingLocation(const std::string &path, const ServerConfig* servercConfig);
bool	methodAllowed(const HTTPRequest &request, const Location *Location);
bool	checkAllowedMethod(const HTTPRequest &request, int socketFD, const std::vector<ServerConfig> &serverConfig);
bool	checkPayLoad(const HTTPRequest &request, int socketFD, const std::vector<ServerConfig> &serverConfig);
const char* reasonPhrase(int code);
bool	checkRedirectResponse(const HTTPRequest &request, int socketFD, const std::vector<ServerConfig> &serverConfig);


void	printRequest(const HTTPRequest &req);
// std::string	generateResponseBody(); // for hardcoded body
void	readClientData(int socketFD, std::map<int, HTTPRequest>& requestMap, std::vector<struct pollfd>& fds, size_t &i, const std::vector<ServerConfig>& servers);
bool	processClientData(int socketFD, std::map<int, HTTPRequest>& requestMap, std::string	data, const std::vector<ServerConfig>& servers);

#endif