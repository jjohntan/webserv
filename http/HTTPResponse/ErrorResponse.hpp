#ifndef ERRORRESPONSE_HPP
# define ERRORRESPONSE_HPP

#include "HTTPResponse.hpp"

#include <string>
#include <sstream>
#include <fstream>

struct ServerConfig;

class	ErrorResponse: public HTTPResponse
{
	private:
		std::string	processContent(int statusCode, std::string statusMessage, const ServerConfig &serverConfig);
		std::string	processContent(int statusCode, std::string statusMessage, const ServerConfig &serverConfig, const std::string &extraHeaders);
		

		/* FilePath -> Root + Error/X.html */
		std::string	resolveErrorFilePath(int statusCode, const ServerConfig &serverConfig);
		std::string	joinRoot(const std::string &root, const std::string &path);
		bool	endWithSlash(const std::string &str);

		/* Get Content (Header + Body) from FilePath */
		bool	readFile(const std::string &path, std::string &out);
		std::string	makeHeaderBody(const std::string &filePath, int statusCode, const std::string &statusMessage);
		std::string	makeHeaderBody(const std::string &filePath, int statusCode, const std::string &statusMessage, const std::string &extraHeaders);
		std::string fallbackHTML(int statusCode, const std::string &statusMessage);

	public:
		ErrorResponse(int statusCode, const std::string &statusMessage, const ServerConfig &serverConfig, int socketFD);
		ErrorResponse(int statusCode, const std::string &statusMessage, const ServerConfig &serverConfig, const std::string &extraHeaders, int socketFD);
		

};

#endif