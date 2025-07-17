#include "HTTPRequest.hpp"

HTTPRequest::HTTPRequest() {}

// HTTPRequest::HTTPRequest(std::string rawString):
// 	_rawString(rawString), _chunked(false), _content_length(0)
// {
// 	// try
// 	// {
// 	// 	this->extractRequestLine();
// 	// 	this->separateHeaderBody();
// 	// 	this->extractHeader();
// 	// 	this->checkChunked();
// 	// 	if (this->_chunked == true)
// 	// 		this->processChunked();
// 	// 	else
// 	// 		this->processUnchunked();
// 	// }
// 	// catch (std::exception &e)
// 	// 	std::cout << e.what() << std::endl;
// }

HTTPRequest::HTTPRequest(const HTTPRequest &other):
	_rawString(other._rawString), _header(other._header), _body(other._body), _chunked(other._chunked),
	_content_length(other._content_length)
{}

const HTTPRequest &HTTPRequest::operator=(const HTTPRequest &other)
{
	if (this != &other)
	{
		this->_rawString = other._rawString;
		this->_header = other._header;
		this->_body = other._body;
	}
	return (*this);
}

const std::string	HTTPRequest::getRawString() const
{
	return (this->_rawString);
}

void	HTTPRequest::setRawString(std::string rawString)
{
	this->_rawString = rawString;
}
/****************************** READING ***************************************** */

void	HTTPRequest::feed(std::string &data)
{
	this->_rawString += data;

	if (this->_headerComplete == false)
	{
		size_t	header_end = this->_rawString.find("\r\n\r\n");
		if (header_end != std::string::npos) // found header-body separator
		{
			this->_headerComplete = true;
			this->extractRequestLine();
			this->_rawHeader = this->_rawString.substr(0, header_end);
			this->extractHeader();
			this->checkChunked();
		}
	}
}

/************************************ REQUEST LINE ******************************* */
void	HTTPRequest::extractRequestLine()
{
	if (this->_rawString.empty())
		throw (HTTPRequest::EmptyRawString());
	std::string	current_line;
	std::istringstream stream(this->_rawString); // for getline()
	
	std::getline(stream, current_line);
	this->_request_line_len = current_line.size(); // doesn't include '/n'
	this->processRequestLine(current_line);
}

void	HTTPRequest::processRequestLine(std::string &line)
{
	this->trimBackslashR(line);
	this->_request_line = line;
	std::istringstream	line_stream(line);
	std::string	method;
	std::string	path;
	std::string	version;

	this->_method = method;
	this->_path = path;
	this->_version = version;
}

/*
	line.size() == line.length() -> return number of characters in string
	line.erase(pos, len) --- pos == position; len == how many words
*/
void	HTTPRequest::trimBackSlashR(std::string &line)
{
	if (!line.empty() && line[line.size() - 1] == '\r')
		line.erase(line.size() - 1, 1);
}

/*
	trim leading and trailing spaces
*/
void	HTTPRequest::trimSpaces(std::string &line)
{
	size_t	start = line.find_first_not_of(" \t\r\n"); // find first word by skipping the set
	size_t	end = line.find_last_not_of(" \t\r\n"); // find the last word by skipping the set

	if (start == std::string::npos)
		line.clear(); // the lines are all whitespaces
	else
		line = line.substr(start, end - start + 1);
}

void	HTTPRequest::separateHeaderBody()
{
	size_t	header_start = this->_request_line_len + 1;
	size_t	header_end = this->_rawString.find("\r\n\r\n");
	if (header_end != std::string::npos)
	{
		this->_header = _rawString.substr(header_start, header_end - header_start); // extract only header
		this->_body = _rawString.substr(header_end + 4); // + 4 to skip "\r\n\r\n"
	}
}

/*************************HEADER***************************** */

void	HTTPRequest::extractHeader()
{
	std::string	key;
	std::string	value;
	size_t	colon_index = 0;
	std::istringstream	stream(this->_rawHeader);
	std::string	line;

	if (_rawHeader.empty())
		throw (HTTPRequest::EmptyRawHeader());
	while (std::getline(stream, line))
	{
		this->trimBackslashR(&line);
		colon_index = line.find(":");
		if (colon_index == std::string::npos)
			continue; // skip malformed line
		key = line.substr(0, colon_index);
		value = line.substr(colon_index + 1);

		// Trim the leading/trailing spaces
		this->trimSpaces(key);
		this->trimSpaces(value);

		// Convert header key to lower case for case insensitive
		for (size_t i = 0; i < key.length(); i++)
			key[i] = std::tolower(key[i]);

		this->_header[key] = value;
	}
}

void	HTTPRequest::checkChunked()
{
	std::map<std::string, std::string>::iterator it = _header.find("content-length");

	if (it != _header.end()) // found key
	{
		this->_chunked = false;
		std::istringstream	stream(it->second);
		size_t	len;

		stream >> len;
		if (stream.fail())
			throw (HTTPRequest::ContentLengthConversionFailed());
		else
		{
			std::cout << "Converted value: " << len << std::endl;
			this->_content_length = len;
		}
	}
	else
		this->_chunked = true;
}

/*********************BODY******************************* */
void	HTTPRequest::processChunked()
{

}

void	HTTPRequest::processUnchunked()
{

}

/******************EXCEPTION******************* */
const char *HTTPRequest::EmptyRawString::what() const throw()
{
	return ("Invalid Raw String: Empty Raw String!");
}

const char *HTTPRequest::EmptyRawHeader::what() const throw()
{
	return ("Invalid Raw Header: Empty Raw Header!");
}

const char *HTTPRequest::ContentLengthConversionFailed::what() const throw()
{
	return ("Invalid Header Value: Conversion Content-Length Failed!");
}
