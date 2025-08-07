#include "HTTPResponse.hpp"

HTTPResponse::HTTPResponse() {}

HTTPResponse::HTTPResponse(std::string statusLine, std::string content, int socketFD):
	_statusLine(statusLine), _content(content), _socketFD(socketFD)
{}

HTTPResponse::HTTPResponse(const HTTPResponse &other):
	_statusLine(other._statusLine),
	_content(other._content),
	_socketFD(other._socketFD)
{}

const HTTPResponse &HTTPResponse::operator=(const HTTPResponse &other)
{
	if (this != &other)
	{
		_statusLine = other._statusLine;
		_content = other._content;
		_socketFD = other._socketFD;
	}
	return (*this);
}

HTTPResponse::~HTTPResponse()
{}

/***********************GETTTERS************************* */

const std::string	&HTTPResponse::getStatusLine() const
{
	return (this->_statusLine);
}

const std::string	&HTTPResponse::getContent() const
{
	return (this->_content);
}

const std::string	&HTTPResponse::getSocketFD() const
{
	return (this->_socketFD);
}

/****************************SETTERS***************************** */

void	HTTPResponse::setStatusLine(std::string &statusLine)
{
	this->_statusLine = statusLine;
}

void	HTTPResponse::setContent(std::string &content)
{
	this->_content = content;
}

void	HTTPResponse::setSocketFD(int socketFD)
{
	this->_socketFD = socketFD;
}