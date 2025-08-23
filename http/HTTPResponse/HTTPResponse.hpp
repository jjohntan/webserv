#ifndef HTTPRESPONSE_HPP
# define HTTPRESPONSE_HPP

#include <string>
#include <iostream>
#include <sys/socket.h>	// for send()
#include <unistd.h>		 // for close()
#include <sstream> // For std::istringstream
#include <cstdio> // For perror()

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
		std::string	_statusLine; // contain Status: 404 Not Found\r\n
		std::string	_modifyStatus; // contain 404 Not Found\r\n
		int			_statusCode;
		std::string	_statusMessage;
		std::string	_formatedStatus; // HTTP/1.1 404 Not Found\r\n
		std::string	_completeRawResponse;

		std::string	_content; // body & header
		int	_socketFD;

		HTTPResponse();
	public:
		HTTPResponse(std::string statusLine, std::string content, int socketFD); // normal response
		HTTPResponse( std::string statusMessage, int statusCode, std::string content, int socketFD);
		HTTPResponse(std::string status, int errorCode); // default content error
		HTTPResponse(std::string status, int errorCode, std::string	content);
		~HTTPResponse();
		HTTPResponse(const HTTPResponse &other);
		HTTPResponse	&operator=(const HTTPResponse &other);

		// void	processHTTPResponse();
		void	sendResponse() const;
		
		/* Status Line */
		void	convertStatusLine();
		void	extractStatusCodeAndMessage();
		void	reformatStatusLine();

		/* Response */
		void	addStatusLineToContent();

		/* Simple Error Page Generation */
		std::string HTTPResponse::generateErrorHTML(int code, const std::string &message) const;
		std::string HTTPResponse::buildErrorResponse() const;

		/* Utility */
		void	trimBackslashR(std::string &line);
		void	trimLeadingSpaces(std::string &line);

		/* Getters */
		const std::string	&getStatusLine() const;
		const std::string	&getContent() const;
		int					getSocketFD() const;
		const std::string	&getRawResponse() const;
		const std::string	&getStatusMessage() const;
		int					getStatusCode() const;

		/* Setters */
		void	setStatusLine(const std::string &statusLine);
		void	setContent(const std::string &content);
		void	setSocketFD(const int	socketFD);
		void	setStatusCode(const int statusCode);
		void	setStatusMessage(const std::string statusMessage);
};

#endif