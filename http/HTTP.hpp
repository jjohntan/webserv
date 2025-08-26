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
# include <map>
# include <string>
# include <poll.h> 

void	printRequest(const HTTPRequest &req);
std::string	generateResponseBody(); // for hardcoded body
void	readClientData(int socketFD, std::map<int, HTTPRequest>& requestMap, std::vector<struct pollfd>& fds, size_t &i);
bool	processClientData(int socketFD, std::map<int, HTTPRequest>& requestMap, std::string	data);

#endif