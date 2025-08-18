#ifndef HTTPREQUEST_HPP
# define HTTPREQUEST_HPP

#include <map>
#include <vector>
#include <iostream>
#include <sstream>  // For std::istringstream, std::ostringstream, std::stringstream
#include <string>
#include <stdexcept>

# define RED "\033[31m"
# define GREEN "\033[32m"
# define BLUE "\033[34m"
# define YELLOW "\033[33m"
# define CYAN "\033[36m"
# define MAGENTA "\033[35m"
# define WHITE "\033[37m"
# define RESET "\033[0m"

class	HTTPRequest
{
	private:
		int			_socketFD;
		std::string	_rawString;
		std::string	_rawHeader;
		std::string	_rawBody;
		bool	_headerComplete;
		bool	_bodyComplete;

		/* Header */
		std::map<std::string, std::string> _header;

		/* Body */
		std::vector<char> _body;

		/* Body - Chunked */
		bool	_isChunked;
		bool	_chunkedComplete;
		size_t	_bodyPos; // cursor position to store where previous reading pos
		size_t	_chunkSize;
		void	skipHeader();
		bool	readChunkSize();
		bool	checkLastChunk();
		bool	readChunkData();

		/* Body - Unchunked */
		size_t	_content_length;

		/* Request Line */
		std::string	_request_line;
		size_t	_request_line_len;
		std::string	_method;
		std::string	_path;
		std::string	_version;

		void	extractRequestLine();
		void	processRequestLine(std::string &line);
		void	trimBackslashR(std::string &line);
		void	trimSpaces(std::string	&line);

		/* Header */
		void	extractHeader();
		void	analyzeHeader();
		void	checkContentLength();
		void	checkChunked();

		/* Body */
		void	processChunked();
		void	processUnchunked();

	public:
		HTTPRequest();
		HTTPRequest(int socketFD);
		~HTTPRequest();
		HTTPRequest(const HTTPRequest &other);
		const HTTPRequest	&operator=(const HTTPRequest &other);

		void	feed(std::string &data);

		/* Getters */
		const std::string &getRawString() const;
		const std::string &getRawHeader() const;
		const std::string &getRawBody() const;
		const std::map<std::string, std::string> &getHeaderMap() const;
		const std::vector<char> &getBodyVector() const;
		bool isHeaderComplete() const;
		bool isBodyComplete() const;
		bool isChunked() const;
		const std::string &getMethod() const;
		const std::string &getPath() const;
		const std::string &getVersion() const;

		/* Setters */
		void setRawString(std::string &rawString);
		void setRawHeader(const std::string &rawHeader);
		void setRawBody(const std::string &rawBody);
		void setHeaderMap(const std::map<std::string, std::string> &header);
		void setBodyVector(const std::vector<char> &body);
		void setMethod(const std::string &method);
		void setPath(const std::string &path);
		void setVersion(const std::string &version);

		class EmptyRawString : public std::exception
		{
			public: 
				const char *what() const throw ();
		};

		class EmptyRawHeader : public std::exception
		{
			public: 
				const char *what() const throw ();
		};

		class ContentLengthConversionFailed : public std::exception
		{
			public:
				const char *what() const throw ();
		};
};

#endif