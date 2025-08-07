#ifndef HTTPRESPONSE_HPP
# define HTTPRESPONSE_HPP

#include <string>
#include <iostream>

# define RED "\033[31m"
# define GREEN "\033[32m"
# define BLUE "\033[34m"
# define YELLOW "\033[33m"
# define CYAN "\033[36m"
# define MAGENTA "\033[35m"
# define WHITE "\033[37m"
# define RESET "\033[0m"

class	HTTPResponse
{
	private:
		std::string	_statusLine;
		std::string	_content;
		int	_socketFD;

		HTTPResponse();
	public:
		HTTPResponse(std::string statusLine, std::string content, int	socketFD);
		~HTTPResponse();
		HTTPResponse(const HTTPResponse &other);
		const HTTPResponse	&operator=(const HTTPResponse &other);

		/* Getters */
		const std::string	&getStatusLine() const;
		const std::string	&getContent() const;
		const int			&getSocketFD() const;

		/* Setters */
		void	setStatusLine(std::string &statusLine);
		void	setContent(std::string &content);
		void	setSocketFD(int	socketFD);

};

#endif