#include "ErrorResponse.hpp"
#include "../../config_files/config.hpp"

ErrorResponse::ErrorResponse(int statusCode, const std::string &statusMessage, const ServerConfig &serverConfig, int socketFD):
	HTTPResponse(statusMessage, 
				statusCode,
				processContent(statusCode, statusMessage, serverConfig),
				socketFD)
{}

ErrorResponse::ErrorResponse(int statusCode, const std::string &statusMessage, const ServerConfig &serverConfig, const std::string &extraHeaders, int socketFD):
	HTTPResponse(statusMessage,
				statusCode,
				processContent(statusCode, statusMessage, serverConfig, extraHeaders),
				socketFD)
{}

std::string	ErrorResponse::processContent(int statusCode, std::string statusMessage, const ServerConfig &serverConfig)
{
	std::string path = resolveErrorFilePath(statusCode, serverConfig);
	std::string output = makeHeaderBody(path, statusCode, statusMessage);
	return (output);
}

std::string	ErrorResponse::processContent(int statusCode, std::string statusMessage, const ServerConfig &serverConfig, const std::string &extraHeaders)
{
	std::string path = resolveErrorFilePath(statusCode, serverConfig);
	std::string output = makeHeaderBody(path, statusCode, statusMessage, extraHeaders);
	return (output);
}

std::string	ErrorResponse::resolveErrorFilePath(int statusCode, const ServerConfig &serverConfig)
{
	std::map<int, std::string>::const_iterator it = serverConfig.error_pages.find(statusCode);
	std::string	configured;
	if (it != serverConfig.error_pages.end())
		configured = it->second;
	else
		configured = "";
	
	if (configured.empty())
	{
		std::ostringstream	path;
		path << "/error/" << statusCode << ".html";
		configured = path.str();
	}
	configured = joinRoot(serverConfig.root, configured);
	return (configured);
}

/*
	root -> .pages/www  OR .pages/www/

	trim the leading "/" at the path
	/error/404.html ----> error/404.html

*/
std::string	ErrorResponse::joinRoot(const std::string &root, const std::string &path)
{
	// trim the leading "/"
	std::string	trimmed = path;
	while (!trimmed.empty() && (trimmed[0] == '/' || trimmed[0] == '\\'))
		trimmed.erase(trimmed.begin());
	if (root.empty())
		return (trimmed);
	if (endWithSlash(root))
		return (root + trimmed);
	return (root + "/" + trimmed);
}

bool	ErrorResponse::endWithSlash(const std::string &str)
{
	if (str.empty())
		return (false);
	size_t	last = str.size() - 1;
	if (str[last] == '/' || str[last] == '\\')
		return (true);
	return (false);
}

std::string	ErrorResponse::makeHeaderBody(const std::string &filePath, int statusCode, const std::string &statusMessage)
{
	std::string	body;
	if (!readFile(filePath, body))
		body = fallbackHTML(statusCode, statusMessage);
	
	std::ostringstream	out;
	out << "Content-Type: text/html\r\n"
		<< "Content-Length: " << body.size() << "\r\n"
		<< "Connection: close\r\n"
		<< "\r\n"
		<< body;

	return (out.str());
}

std::string	ErrorResponse::makeHeaderBody(const std::string &filePath, int statusCode, const std::string &statusMessage, const std::string &extraHeaders)
{
	std::string	body;
	if (!readFile(filePath, body))
		body = fallbackHTML(statusCode, statusMessage);
	
	std::ostringstream	out;
	if (!extraHeaders.empty())
		out << extraHeaders;
	
	out << "Content-Type: text/html\r\n"
		<< "Content-Length: " << body.size() << "\r\n";

	if (!hasConnectionHeader(extraHeaders))
		out << "Connection: close\r\n";

	out << "\r\n" << body;

	return (out.str());
}

/*
	std::ifstream → input file stream, used to read from files.
	path.c_str() → converts the std::string path into a const char* (C-style string), because C++98 ifstream constructor expects that.
	std::ios::in → open for reading.
	std::ios::binary → open in binary mode (important for non-text files or to prevent \r\n translation on Windows).
	

	file.rdbuf() → gives access to the file’s internal buffer.
*/
bool ErrorResponse::readFile(const std::string &path, std::string &out)
{
	std::ifstream	file(path.c_str(), std::ios::in | std::ios::binary);
	if (!file)
		return (false);
	
	std::ostringstream	content;
	content << file.rdbuf();
	out = content.str();
	return (true);
}

std::string ErrorResponse::fallbackHTML(int statusCode, const std::string &statusMessage)
{
	std::ostringstream html;
	html << "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\"/>"
			<< "<title>" << statusCode << " " << statusMessage << "</title></head>\n"
			<< "<body style=\"font-family:system-ui,Segoe UI,Roboto,Arial;"
			<< "margin:40px\"><h1>" << statusCode << " " << statusMessage << "</h1>\n"
			<< "<p>The server could not process your request.</p></body></html>\n";
	return (html.str());
}

/* Utility */
bool hasConnectionHeader(const std::string &s)
{
	for (size_t i = 0; i + 11 <= s.size(); ++i)
	{
		// look for "\nconnection:" or start-of-string "connection:"
		if ((i == 0 || s[i-1] == '\n' || (i>=2 && s[i-2]=='\r' && s[i-1]=='\n')))
		{
			// case-insensitive compare
			const char *p = "connection:";
			size_t j = 0;
			size_t k = i;
			while (p[j] && k < s.size() && std::tolower(s[k]) == p[j])
			{
				++j;
				++k;
			}
			if (!p[j])
				return (true);
		}
	}
	return (false);
}