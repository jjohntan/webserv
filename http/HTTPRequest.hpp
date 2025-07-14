#ifndef HTTPREQUEST_HPP
# define HTTPREQUEST_HPP

#include <map>
#include <vector>
#include <iostream>
#include <sstream>  // For std::istringstream, std::ostringstream, std::stringstream
#include <string>
#include <stdexcept>

# define RED "\033[31m"
# define GREEN "\033[32m"
# define BLUE "\033[34m"
# define YELLOW "\033[33m"
# define CYAN "\033[36m"
# define MAGENTA "\033[35m"
# define WHITE "\033[37m"
# define RESET "\033[0m"

class	HTTPRequest
{
	private:
		std::string	_rawString;
		std::map<std::string, std::string> _header;
		std::vector<char> _body;

		std::string	_request_line;
		void	extractHeader();
		void	processRequestLine(std::string &line);
		void	trimBackslashR(std::string &line)

	public:
		// HTTPRequest();
		HTTPRequest(std::string rawString);
		~HTTPRequest();
		HTTPRequest(const HTTPRequest &other);
		const HTTPRequest	&operator=(const HTTPRequest &other);

		const std::string getRawString() const;
		void setRawString(std::string rawString);

		class EmptyRawString : public std::exception
		{
			public: 
				const char *what() const throw ();
		}

};


#endif