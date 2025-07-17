#ifndef REQUEST_HPP
# define REQUEST_HPP

# define RED "\033[31m"
# define GREEN "\033[32m"
# define BLUE "\033[34m"
# define YELLOW "\033[33m"
# define CYAN "\033[36m"
# define MAGENTA "\033[35m"
# define WHITE "\033[37m"
# define RESET "\033[0m"

# include <map>
# include "HTTPRequest.hpp"

HTTPRequest	storeSocketMap(size_t	socketID, std::string &text);

#endif