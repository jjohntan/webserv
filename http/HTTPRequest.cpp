#include "HTTPRequest.hpp"

HTTPRequest::HTTPRequest():
	_headerComplete(false), _bodyComplete(false), _isChunked(false), _chunkedComplete(false),
	_bodyPos(0), _content_length(0), _chunkSize(0), _request_line_len(0)
{}

HTTPRequest::HTTPRequest(const HTTPRequest &other):
	_rawString(other._rawString), _rawHeader(other._rawHeader),
	_rawBody(other._rawBody), _headerComplete(other._headerComplete),
	_bodyComplete(other._bodyComplete), _header(other._header),
	_body(other._body), _isChunked(other._isChunked),
	_chunkedComplete(other._chunkedComplete), _bodyPos(other._bodyPos),
	_chunkSize(other._chunkSize), _content_length(other._content_length),
	_request_line(other._request_line), _request_line_len(other._request_line_len),
	_method(other._method), _path(other._path), _version(other._version)
{}

const HTTPRequest &HTTPRequest::operator=(const HTTPRequest &other)
{
	if (this != &other)
	{
		this->_rawString = other._rawString;
		this->_rawHeader = other._rawHeader;
		this->_rawBody = other._rawBody;
		this->_header = other._header;
		this->_body = other._body;
		this->_headerComplete = other._headerComplete;
		this->_bodyComplete = other._bodyComplete;
		this->_isChunked = other._isChunked;
		this->_chunkedComplete = other._chunkedComplete;
		this->_bodyPos = other._bodyPos;
		this->_chunkSize = other._chunkSize;
		this->_content_length = other._content_length;
		this->_request_line = other._request_line;
		this->_request_line_len = other._request_line_len;
		this->_method = other._method;
		this->_path = other._path;
		this->_version = other._version;
	}
	return (*this);
}

/************************GETTER & SETTER************************************ */
/* Getters */
const std::string	&HTTPRequest::getRawString() const
{
	return (this->_rawString);
}

const std::string &HTTPRequest::getRawHeader() const
{
	return (this->_rawHeader);
}

const std::string &HTTPRequest::getRawBody() const
{
	return (this->_rawBody);
}

const std::map<std::string, std::string> &HTTPRequest::getHeaderMap() const
{
	return (this->_header);
}

const std::vector<char> &HTTPRequest::getBodyVector() const
{
	return (this->_body);
}

bool HTTPRequest::isHeaderComplete() const
{
	return (this->_headerComplete);
}

bool HTTPRequest::isBodyComplete() const
{
	return (this->_bodyComplete);
}

bool HTTPRequest::isChunked() const
{
	return (this->_isChunked);
}

const std::string &HTTPRequest::getMethod() const
{
	return (this->_method);
}

const std::string &HTTPRequest::getPath() const
{
	return (this->_path);
}

const std::string &HTTPRequest::getVersion() const
{
	return (this->_version);
}

/* Setters */
void	HTTPRequest::setRawString(std::string &rawString)
{
	this->_rawString = rawString;
}

void HTTPRequest::setRawHeader(const std::string &rawHeader)
{
	this->_rawHeader = rawHeader;
}

void HTTPRequest::setRawBody(const std::string &rawBody)
{
	this->_rawBody = rawBody;
}

void HTTPRequest::setHeaderMap(const std::map<std::string, std::string> &header)
{
	this->_header = header;
}

void HTTPRequest::setBodyVector(const std::vector<char> &body)
{
	this->_body = body;
}

void HTTPRequest::setMethod(const std::string &method)
{
	this->_method = method;
}

void HTTPRequest::setPath(const std::string &path)
{
	this->_path = path;
}

void HTTPRequest::setVersion(const std::string &version)
{
	this->_version = version;
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
			this->analyzeHeader();
		}
	}
	// Process body after header complete
	if (this->_headerComplete == true && this->_bodyComplete == false)
	{
		if (this->_isChunked == true)
			this->processChunked();
		else
			this->processUnchunked();
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

	line_stream >> method >> path >> version;
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
		if (line.empty())
			continue;
		this->trimBackslashR(line);
		colon_index = line.find(":");
		if (colon_index == std::string::npos)
			std::cerr << YELLOW << "Malformed header line: " << line << RESET << std::endl;
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

void	HTTPRequest::analyzeHeader()
{
	this->checkChunked();
	if (this->_isChunked == false)
		this->checkContentLength();
}

void	HTTPRequest::checkChunked()
{
	std::map<std::string, std::string>::iterator it = _header.find("transfer-encoding");

	if (it != _header.end()) // found
		this->_isChunked = true;
	else
		this->_isChunked = false;
}

void	HTTPRequest::checkContentLength()
{
	std::map<std::string, std::string>::iterator it = _header.find("content-length");

	if (it != _header.end()) // found key
	{
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
		std::cerr << "There is no content-length in the header!\n";
}

/*********************BODY******************************* */
/********************CHUNKED********************************* */
void	HTTPRequest::processChunked()
{
	if (_bodyPos == 0)
		this->skipHeader();
	while (_chunkedComplete == false && _bodyPos < _rawString.size())
	{
		if (_chunkSize == 0 && readChunkSize() == false)
			break;
		if (_chunkSize == 0) // received chunksize and it's 0\r\n
		{
			if (checkLastChunk() == false)
				break;
		}
		if (readChunkData() == false)
			break;
	}
}

void	HTTPRequest::skipHeader()
{
	size_t	header_end = this->_rawString.find("\r\n\r\n");
	size_t	body_start = header_end + 4; // to skip "\r\n\r\n"
		_bodyPos = body_start;
}

bool	HTTPRequest::readChunkSize()
{
	size_t	end_line = _rawString.find("\r\n", _bodyPos);
	if (end_line != std::string::npos) // found
	{
		std::string	line = _rawString.substr(_bodyPos, end_line - _bodyPos);
		std::istringstream	stream(line);
		stream >> std::hex >> _chunkSize;
		_bodyPos = end_line + 2; // skip the digit and trailling /r/n (e.g. 4/r/n)
		return (true);
	}
	else
		return (false); // because couldn't find a newline, need to fetch data
}

bool	HTTPRequest::checkLastChunk()
{
	// Look for final CRLF of last chunk 
	size_t	trailer_start = _bodyPos;
	size_t	trailer_end = _rawString.find("\r\n\r\n", trailer_start);

	// Found complete trailer section (Handle 0\r\n[trailer-headers]\r\n\r\n)
	if (trailer_end != std::string::npos)
	{
		_chunkedComplete = true;
		_bodyComplete = true;
		_bodyPos = trailer_end + 4;
		return (true);
	}

	// handle simple "\r\n" if no trailer headers
	// Handle 0\r\n\r\n
	if (_rawString.substr(_bodyPos, 2) == "\r\n")
	{
		_chunkedComplete = true;
		_bodyComplete = true;
		_bodyPos += 2;
		return (true);
	}
	return (false);
}

bool	HTTPRequest::readChunkData()
{
	if (_rawString.size() < _bodyPos + _chunkSize + 2) // current raw data has fewer data than required
		return (false);
	std::string	chunk = _rawString.substr(_bodyPos, _chunkSize);
	_rawBody += chunk; // store the data
	_body.insert(_body.end(), chunk.begin(), chunk.end()); // store in the vector
	_bodyPos += _chunkSize + 2; // skip the trailing /r/n && chunk body
	_chunkSize = 0; // reset
	return (true);
}

/****************************UNCHUNKED*************************** */
void	HTTPRequest::processUnchunked()
{
	size_t	header_end = this->_rawString.find("\r\n\r\n");
	size_t	body_start = header_end + 4; // to skip "\r\n\r\n"
	if (this->_rawString.size() >= body_start + this->_content_length) // to ensure full body is received
	{
		std::string	text = this->_rawString.substr(body_start, this->_content_length);
		this->_rawBody = text;
		this->_body.insert(_body.end(), text.begin(), text.end());
		this->_bodyComplete = true;
	}
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
