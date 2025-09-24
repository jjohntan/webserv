#include "HTTPResponse.hpp"

HTTPResponse::HTTPResponse() {}

HTTPResponse::HTTPResponse(std::string statusLine, std::string content, int socketFD):
	_statusLine(statusLine),
	_statusCode(0),
	_content(content),
	_socketFD(socketFD)
{
	if (!_content.empty())
		this->separateHeaderBody();
	if (!_body.empty())
		this->countBodyLen();
	this->ensureContentLength();
	this->convertStatusLine();
	this->reformatStatusLine();
	this->addStatusLineToContent();
}

HTTPResponse::HTTPResponse(std::string statusMessage, int statusCode, std::string content, int socketFD):
	_statusCode(statusCode),
	_statusMessage(statusMessage),
	_content(content),
	_socketFD(socketFD)
{
	if (!_content.empty())
		this->separateHeaderBody();
	if (!_body.empty())
		this->countBodyLen();
	this->ensureContentLength();
	std::ostringstream	stream;
	stream << statusCode;
	std::string	codeStr = stream.str();
	this->_modifyStatus = codeStr + " " + statusMessage + "\r\n";
	this->reformatStatusLine();
	this->addStatusLineToContent();
}

/* For Error Page Generation - Default Body*/
HTTPResponse::HTTPResponse(std::string status, int errorCode, bool keep):
	_statusCode(errorCode),
	_statusMessage(status)
{
	std::ostringstream	stream;
	stream << errorCode;
	std::string	errorCodeStr = stream.str();
	this->_modifyStatus = errorCodeStr + " " + status + "\r\n";
	this->reformatStatusLine();
	this->_content = this->buildErrorResponse(keep);
	if (!_body.empty())
		this->countBodyLen();
	this->ensureContentLength();
	this->addStatusLineToContent();
}

/* For Error Page Generation - Passed in Body */
HTTPResponse::HTTPResponse(std::string status, int errorCode, std::string content):
	_statusCode(errorCode),
	_statusMessage(status)
{
	std::ostringstream	stream;
	stream << errorCode;
	std::string	errorCodeStr = stream.str();
	this->_modifyStatus = errorCodeStr + " " + status + "\r\n";
	this->reformatStatusLine();
	this->_content = content;
	if (!_content.empty())
		this->separateHeaderBody();
	if (!_body.empty())
		this->countBodyLen();
	this->ensureContentLength();
	this->addStatusLineToContent();
}

HTTPResponse::HTTPResponse(const HTTPResponse &other):
	_statusLine(other._statusLine),
	_modifyStatus(other._modifyStatus),
	_statusCode(other._statusCode),
	_statusMessage(other._statusMessage),
	_formatedStatus(other._formatedStatus),
	_completeRawResponse(other._completeRawResponse),
	_content(other._content),
	_header(other._header),
	_body(other._body),
	_bodyLen(other._bodyLen),
	_socketFD(other._socketFD)
{}

HTTPResponse &HTTPResponse::operator=(const HTTPResponse &other)
{
	if (this != &other)
	{
		_statusLine = other._statusLine;
		_modifyStatus = other._modifyStatus;
		_statusCode = other._statusCode;
		_statusMessage = other._statusMessage;
		_formatedStatus = other._formatedStatus;
		_completeRawResponse = other._completeRawResponse;
		_content = other._content;
		_header = other._header;
		_body = other._body;
		_bodyLen = other._bodyLen;
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

int	HTTPResponse::getSocketFD() const
{
	return (this->_socketFD);
}

const std::string &HTTPResponse::getRawResponse() const
{
	return (this->_completeRawResponse);
}

int HTTPResponse::getStatusCode() const
{
	return (this->_statusCode);
}

const std::string &HTTPResponse::getStatusMessage() const
{
	return (this->_statusMessage);
}

int	HTTPResponse::getBodyLen() const
{
	return (static_cast<int>(this->_bodyLen));
}

/****************************SETTERS***************************** */

void	HTTPResponse::setStatusLine(const std::string &statusLine)
{
	this->_statusLine = statusLine;
}

void	HTTPResponse::setContent(const std::string &content)
{
	this->_content = content;
}

void	HTTPResponse::setSocketFD(const int socketFD)
{
	this->_socketFD = socketFD;
}

void	HTTPResponse::setStatusCode(const int statusCode)
{
	this->_statusCode = statusCode;
}

void	HTTPResponse::setStatusMessage(const std::string statusMessage)
{
	this->_statusMessage = statusMessage;
}

/************************ CONVERSION ****************** */
// void	HTTPResponse::processHTTPResponse()
// {
// 	this->convertStatusLine();
// 	this->addStatusLineToContent();
// }


void	HTTPResponse::sendResponse() const
{
	// const std::string &response = this->_completeRawResponse;
	// ssize_t total_sent = 0;
	// ssize_t response_size = response.size();

	// while (total_sent < response_size)
	// {
	// 	ssize_t sent = send(_socketFD, response.c_str() + total_sent, response_size - total_sent, 0);
	// 	if (sent == -1)
	// 	{
	// 		std::cerr << "Failed to send response to socket FD " << _socketFD << std::endl;
	// 		break;
	// 	}
	// 	total_sent += sent;
	// }
	
	// [CHANGE] Do nothing: sending is handled by Server outbox (one write per poll tick).
	std::cerr << "[warn] sendResponse() is deprecated; use queued sending via Server.\n";
}
/************************ STATUS LINE ***************** */

/*
	Status: 404 Not Found\r\n
	to
	HTTP/1.1 404 Not Found\r\n
*/
void	HTTPResponse::convertStatusLine()
{
	size_t colon_index = 0;

	colon_index = this->_statusLine.find(":");
	if (colon_index == std::string::npos)
	{
		std::cerr << "Invalid status line format: " << _statusLine << std::endl;
		return;
	}
	this->_modifyStatus = this->_statusLine.substr(colon_index + 1);
	this->trimLeadingSpaces(this->_modifyStatus);
	this->extractStatusCodeAndMessage();
}

void	HTTPResponse::reformatStatusLine()
{
	std::string	version = "HTTP/1.1";

	this->_formatedStatus = version + " " + this->_modifyStatus;
}

void	HTTPResponse::extractStatusCodeAndMessage()
{
	std::istringstream	stream(_modifyStatus);
	int	status_code;
	std::string	status_message;

	// Extract status code
	if (!(stream >> status_code)) {
		std::cerr << "Invalid status code in: " << _modifyStatus << std::endl;
		_statusCode = 0;
		_statusMessage.clear();
		return;
	}

	// Extract rest as status message
	std::getline(stream, status_message);

	// Trim leading/trailing spaces and \r\n from message
	size_t start = status_message.find_first_not_of(" \t\r\n");
	size_t end = status_message.find_last_not_of(" \t\r\n");
	if (start != std::string::npos)
		_statusMessage = status_message.substr(start, end - start + 1);
	else
		_statusMessage.clear();

	// Store status code
	_statusCode = status_code;
}

/*************************** BODY & HEADER ********************* */

void	HTTPResponse::separateHeaderBody()
{
	// Find the CRLFCRLF sequence that separates headers from body
	std::string::size_type pos = this->_content.find("\r\n\r\n");

	if (pos != std::string::npos)
	{
		// Header: everything up to (but not including) CRLFCRLF
		this->_header = this->_content.substr(0, pos);

		// Body: everything after CRLFCRLF (skip 4 chars)
		this->_body = this->_content.substr(pos + 4);
	}
	else
	{
		// No delimiter found → assume the entire thing is header
		this->_header = this->_content;
		this->_body.clear();
	}
}

void	HTTPResponse::countBodyLen()
{
	if (!this->_body.empty())
		this->_bodyLen = this->_body.size();
	else
		this->_bodyLen = 0;
}

void	HTTPResponse::addStatusLineToContent()
{
	this->_completeRawResponse = this->_formatedStatus + this->_content;
}

void HTTPResponse::ensureContentLength()
{
	// If there is no body, keep headers as-is (e.g., 204/HEAD/no-body responses).
	// Caller should have added Content-Length appropriately.
	if (_body.empty())
		return;

	// Rebuild header without any existing Content-Length (case-insensitive),
	// then append the correct one computed from _body.size().
	std::istringstream hs(_header);
	std::ostringstream newHeader;
	std::string line;
	while (std::getline(hs, line))
	{
		// keep CR stripping logic consistent with separateHeaderBody()
		std::string::size_type n = line.size();
		if (n > 0 && line[n - 1] == '\r')
			line.erase(n - 1, 1);
		std::string lower = line;
		for (size_t i = 0; i < lower.size(); ++i)
			lower[i] = std::tolower(static_cast<unsigned char>(lower[i]));
		if (lower.rfind("content-length:", 0) != 0)   // keep all but Content-Length
			newHeader << line << "\r\n";
	}
	newHeader << "Content-Length: " << _body.size() << "\r\n";

	_header = newHeader.str();
	_content = _header + "\r\n" + _body;
}

/***************************** ERROR PAGES GENERATION ******************* */

std::string HTTPResponse::generateErrorHTML(int code, const std::string &message)
{
	std::ostringstream stream;
	stream	<< "<!DOCTYPE html>\n"
			<< "<html>\n<head><title>" << code << " " << message << "</title></head>\n"
			<< "<body><h1>" << code << " " << message << "</h1>\n"
			<< "<p>Your request is failed to be processed.</p></body>\n</html>\n";
	return (stream.str());
}

std::string HTTPResponse::buildErrorResponse(bool keep)
{
	this->_body = generateErrorHTML(_statusCode, _statusMessage);
	std::ostringstream stream;
	// No status line here; addStatusLineToContent() will prepend it.
	stream << "Content-Type: text/html\r\n"
			<< (keep ? "Connection: keep-alive\r\n" : "Connection: close\r\n")
			<< "\r\n"
			<< this->_body;
	return stream.str();
}

/******************** UTILITY***************************** */
/*
	line.size() == line.length() -> return number of characters in string
	line.erase(pos, len) --- pos == position; len == how many words
*/
void	HTTPResponse::trimBackslashR(std::string &line)
{
	if (!line.empty() && line[line.size() - 1] == '\r')
		line.erase(line.size() - 1, 1);
}

/*
	trim leading spaces only
*/
void	HTTPResponse::trimLeadingSpaces(std::string &line)
{
	size_t	start = line.find_first_not_of(" \t\r\n"); // find first word by skipping the set

	if (start == std::string::npos)
		line.clear(); // the lines are all whitespaces
	else
		line = line.substr(start);
}