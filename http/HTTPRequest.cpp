#include HTTPREQUEST_HPP

// HTTPRequest::HTTPRequest() {}

HTTPRequest::HTTPRequest(std::string rawString):
	_rawString(rawString)
{

}

HTTPRequest::HTTPRequest(const HTTPRequest &other):
	_rawString(other._rawString), _header(other._header), _body(other._body)
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

/************************************ HEADER******************************* */
void	HTTPRequest::extractHeader()
{
	if (this->_rawString.empty())
		throw (HTTPRequest::EmptyRawString());
	std::string	current_line;
	std::isstringstream stream(this->_rawString); // for getline()
	std::getline(stream, current_line);
	this->processRequestLine(&current_line);
}

void	HTTPRequest::processRequestLine(std::string &line)
{
	this->trimBackslashR(line);

	std::map<std::string, std::string>::iterator it;
	
	for (it = _header.begin(); )
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

/******************EXCEPTION******************* */
const char *HTTPRequest::EmptyRawString::what() const throw()
{
	return ("The Raw String in private member is empty!");
}
