#include "HTTPRequest.hpp"

HTTPRequest::HTTPRequest():
	_headerComplete(false),
	_bodyComplete(false),
	_connectionAlive(true),
	_isChunked(false),
	_chunkedComplete(false),
	_bodyPos(0),
	_chunkSize(0),
	_content_length(0),
	_request_line_len(0),
	_useMultipartBoundary(false)
{}

HTTPRequest::HTTPRequest(int socketFD):
	_socketFD(socketFD),
	_headerComplete(false),
	_bodyComplete(false),
	_connectionAlive(true),
	_isChunked(false),
	_chunkedComplete(false),
	_bodyPos(0),
	_chunkSize(0),
	_content_length(0),
	_request_line_len(0),
	_useMultipartBoundary(false)
{}

HTTPRequest::HTTPRequest(const HTTPRequest &other):
	_socketFD(other._socketFD), _rawString(other._rawString), _rawHeader(other._rawHeader),
	_rawBody(other._rawBody), _headerComplete(other._headerComplete),
	_bodyComplete(other._bodyComplete), _connectionAlive(other._connectionAlive),_header(other._header),
	_body(other._body), _isChunked(other._isChunked),
	_chunkedComplete(other._chunkedComplete), _bodyPos(other._bodyPos),
	_chunkSize(other._chunkSize), _content_length(other._content_length),
	_request_line(other._request_line), _request_line_len(other._request_line_len),
	_method(other._method), _path(other._path), _version(other._version),
	_useMultipartBoundary(other._useMultipartBoundary), _boundary(other._boundary)
{}

HTTPRequest &HTTPRequest::operator=(const HTTPRequest &other)
{
	if (this != &other)
	{
		this->_socketFD = other._socketFD;
		this->_rawString = other._rawString;
		this->_rawHeader = other._rawHeader;
		this->_rawBody = other._rawBody;
		this->_header = other._header;
		this->_body = other._body;
		this->_headerComplete = other._headerComplete;
		this->_bodyComplete = other._bodyComplete;
		this->_connectionAlive = other._connectionAlive;
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
		this->_useMultipartBoundary = other._useMultipartBoundary;
		this->_boundary = other._boundary;
	}
	return (*this);
}

HTTPRequest::~HTTPRequest() {}

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

bool HTTPRequest::isConnectionAlive() const
{
	return (this->_connectionAlive);
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

const std::string &HTTPRequest::getQueryString() const
{
	return (this->_query);
}

const std::string &HTTPRequest::getVersion() const
{
	return (this->_version);
}

const int &HTTPRequest::getSocketFD() const
{
	return (this->_socketFD);
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

void HTTPRequest::setConnectionAlive(const bool &connectionAlive)
{
	this->_connectionAlive = connectionAlive;
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
	// Always store PATH **without** query (defensive)
	size_t q = path.find('?');
	if (q == std::string::npos)
		this->_path = path;
	else
		this->_path = path.substr(0, q);
}

void HTTPRequest::setQueryString(const std::string &query)
{
	this->_query = query;
}

void HTTPRequest::setVersion(const std::string &version)
{
	this->_version = version;
}

void HTTPRequest::setSocketFD(const int &socketFD)
{
	this->_socketFD = socketFD;
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
			this->_rawHeader = this->_rawString.substr(_request_line_len, header_end - _request_line_len);
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
	size_t crlf = this->_rawString.find("\r\n");
	if (crlf == std::string::npos)
		throw (HTTPRequest::EmptyRawString());
	std::string current_line = this->_rawString.substr(0, crlf);
	// Now _request_line_len includes the CRLF
	this->_request_line_len = crlf + 2;
	this->processRequestLine(current_line);
}

void	HTTPRequest::processRequestLine(std::string &line)
{
	this->trimBackslashR(line);
	this->_request_line = line;
	std::istringstream	line_stream(line);
	std::string	method;
	std::string target;   // may contain '?'
	std::string	version;

	line_stream >> method >> target >> version;
	this->_method = method;
	this->_version = version;

	// Split request-target into path + query (RFC 9112 §3.2)
	// We only support origin-form "path?query" here (which is what your server uses).
	// Any fragment ('#...') is ignored by HTTP and should never be sent.
	if (target.empty())
	{
		_path.clear();
		_query.clear();
		return;
	}
	// Absolute-form (proxy) or asterisk-form (*) fallbacks:
	// If it's "*", treat as root.
	if (target == "*")
	{
		_path = "*";     // keep it "*"
		_query.clear();
		return;
	}
	// Normal case: origin-form "/path[?query]"
	size_t qpos = target.find('?');
	if (qpos == std::string::npos)
	{
		_path = target;
		_query.clear();
	}
	else
	{
		_path  = target.substr(0, qpos);
		_query = target.substr(qpos + 1);
	}
	// Ensure path is non-empty; treat empty as "/"
	if (_path.empty())
		_path = "/";
}

/*
	line.size() == line.length() -> return number of characters in string
	line.erase(pos, len) --- pos == position; len == how many words
*/
void	HTTPRequest::trimBackslashR(std::string &line)
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
		this->trimBackslashR(line);
		if (line.empty())
			continue;
		colon_index = line.find(":");
		if (colon_index == std::string::npos)
		{
			std::cerr << YELLOW << "Malformed header line: " << line << RESET << std::endl;
			continue;
		}
		key = line.substr(0, colon_index);
		value = line.substr(colon_index + 1);

		// Trim the leading/trailing spaces
		this->trimSpaces(key);
		this->trimSpaces(value);

		// Convert header key to lower case for case insensitive
		for (size_t i = 0; i < key.length(); i++)
			key[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(key[i])));
		this->_header[key] = value;
	}
}

void HTTPRequest::analyzeHeader()
{
	this->checkChunked();
	if (this->_isChunked == false)
		this->checkContentLength();
	this->processConnection();
	// Multipart fallback: only when not chunked AND no Content-Length
	if (_isChunked == false && _content_length == 0 && hasMultipart() == true)
	{
		std::map<std::string, std::string>::const_iterator it = _header.find("content-type");
		if (it != _header.end())
		{
			std::string found;
			bool ok = extractBoundary(it->second, found);
			if (ok == true)
			{
				_boundary = found;
				_useMultipartBoundary = true;
			}
		}
	}
}


void HTTPRequest::checkChunked()
{
	std::map<std::string, std::string>::iterator it = _header.find("transfer-encoding");

	if (it == _header.end())
	{
		_isChunked = false;
		return;
	}

	std::string value = it->second;

	for (size_t i = 0; i < value.size(); ++i)
	{
		value[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(value[i])));
	}

	if (value.find("chunked") != std::string::npos)
	{
		_isChunked = true;
	}
	else
	{
		_isChunked = false;
	}
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

/*
	If request is HTTP/1.1 and header is not Connection: close → keep-alive.
	If request is HTTP/1.0 → only keep-alive when Connection: keep-alive.
*/
void	HTTPRequest::processConnection()
{
	const std::string version = this->_version;
	const std::map<std::string, std::string> header = this->_header;

	std::map<std::string, std::string>::const_iterator it = header.find("connection");
	bool hasConnection = false;
	if (it != header.end())
		hasConnection = true;
	std::string	connection = "";
	if (hasConnection == true)
		connection = it->second;
	
	// Make all words lowercase
	for (size_t	i = 0; i < connection.size(); ++i)
		connection[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(connection[i])));
	this->_connectionAlive = evaluateAlive(version, hasConnection, connection);
}

bool	HTTPRequest::evaluateAlive(const std::string version, const bool hasConnection, const std::string connection)
{
	if (version == "HTTP/1.1")
	{
		if (hasConnection == true)
		{
			if (connection.find("close") == std::string::npos)
				return (true);
			else
				return (false);
		}
		return (true);
	}
	if (version == "HTTP/1.0")
	{
		if (hasConnection == true)
		{
			if (connection.find("keep-alive") != std::string::npos)
				return (true);
			else
				return (false);
		}
		return (false);
	}
	return (false);
}

bool HTTPRequest::hasMultipart() const
{
    std::map<std::string, std::string>::const_iterator it = _header.find("content-type");

    if (it == _header.end())
    {
        return (false);
    }

    std::string ctype = it->second;

    for (size_t i = 0; i < ctype.size(); ++i)
    {
        ctype[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(ctype[i])));
    }

    if (ctype.find("multipart/form-data") != std::string::npos)
    {
        return (true);
    }

    return (false);
}

bool HTTPRequest::extractBoundary(const std::string &ctype, std::string &outBoundary) const
{
	// Work on a lower-cased copy to find "boundary="
	std::string lowered = ctype;
	for (size_t i = 0; i < lowered.size(); ++i)
		lowered[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowered[i])));
	size_t pos = lowered.find("boundary=");
	if (pos == std::string::npos)
		return (false);
	// Now use the original string to extract the exact token
	size_t start = pos + 9;
	while (start < ctype.size() && (ctype[start] == ' ' || ctype[start] == '\t'))
		++start;
	if (start >= ctype.size())
		return (false);
	if (ctype[start] == '"')
	{
		size_t endQuote = ctype.find('"', start + 1);
		if (endQuote == std::string::npos)
			return (false);
		if (endQuote <= start + 1)
			return (false);
		outBoundary = ctype.substr(start + 1, endQuote - (start + 1));
		if (outBoundary.empty())
			return (false);
		return (true);
	}
	size_t end = start;
	while (end < ctype.size()
		&& ctype[end] != ';'
		&& ctype[end] != ' '
		&& ctype[end] != '\t'
		&& ctype[end] != '\r'
		&& ctype[end] != '\n')
		++end;
	outBoundary = ctype.substr(start, end - start);
	if (outBoundary.empty())
		return (false);
	return (true);
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
void HTTPRequest::processUnchunked()
{
	size_t header_end = this->_rawString.find("\r\n\r\n");
	if (header_end == std::string::npos)
		return;
	size_t body_start = header_end + 4;
	// Case 1: Content-Length framing
	if (_content_length > 0)
	{
		if (this->_rawString.size() >= body_start + _content_length)
		{
			std::string text = this->_rawString.substr(body_start, _content_length);

			this->_rawBody = text;
			this->_body.insert(_body.end(), text.begin(), text.end());
			this->_bodyComplete = true;
		}
		return;
	}

	// Case 2: Multipart with boundary, no Content-Length (fallback)
	if (_useMultipartBoundary == true && _boundary.empty() == false)
	{
		// Closing sequence is typically "\r\n--<boundary>--\r\n"
		std::string closing = "\r\n--";
		closing += _boundary;
		closing += "--";

		size_t pos = this->_rawString.find(closing, body_start);

		if (pos != std::string::npos)
		{
			// Body ends right before the CRLF that precedes the closing marker.
			std::string text = this->_rawString.substr(body_start, pos - body_start);

			this->_rawBody = text;
			this->_body.assign(text.begin(), text.end());
			this->_bodyComplete = true;

			// Compute end-of-message position:
			// 1) move past "\r\n--<boundary>--"
			size_t message_end = pos + closing.size();

			// 2) some clients send a final "\r\n" after the closing boundary
			if (this->_rawString.size() >= message_end + 2)
			{
				if (this->_rawString.substr(message_end, 2) == "\r\n")
					message_end += 2;
			}

			// Store like chunked path does, so endOfMessageOffset() can use it
			this->_bodyPos = message_end;
		}

		return;
}

	// No explicit framing → treat as empty body (e.g. GET requests)
	this->_bodyComplete = true;
	this->_bodyPos = body_start;
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

/***************************************** Utility *************************************/

/*
	Add a connection header to your response

	Extra Note:
		You can tune timeout/max for your server
		return ("Connection: keep-alive\r\n
				Keep-Alive: timeout=15, max=100\r\n");
*/
std::string	HTTPRequest::connectionHeader(bool keep) const
{
	if (keep)
		return ("Connection: keep-alive\r\n");
	return ("Connection: close\r\n");
}

/*
	Reset internal state so the same TCP connection can accept another request

	NOTE: _connectionAlive remains whatever was decided for the *just served* request.
	It will be recomputed on the next request's headers.
*/
void HTTPRequest::resetForNextRequest()
{
	_rawString.clear();
	_rawHeader.clear();
	_rawBody.clear();
	_header.clear();
	_body.clear();
	_headerComplete = false;
	_bodyComplete = false;
	_isChunked = false;
	_chunkedComplete = false;
	_bodyPos = 0;
	_chunkSize = 0;
	_content_length = 0;
	_request_line.clear();
	_request_line_len = 0;
	_method.clear();
	_path.clear();
	_query.clear();
	_version.clear();
	_useMultipartBoundary = false;
	_boundary.clear();
}

size_t HTTPRequest::endOfMessageOffset() const
{
	size_t header_end = _rawString.find("\r\n\r\n");
	if (header_end == std::string::npos)
		return (0);

	size_t body_start = header_end + 4;

	// If chunked, or if we computed a message-end position (multipart fallback), use it
	if (_isChunked == true)
		return (_bodyPos);

	if (_useMultipartBoundary == true && _bodyComplete == true && _bodyPos > 0)
		return (_bodyPos);

	// Unchunked normal framing with Content-Length
	return (body_start + _content_length);
}
