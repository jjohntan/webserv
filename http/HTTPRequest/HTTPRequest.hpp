#ifndef HTTPREQUEST_HPP
# define HTTPREQUEST_HPP

#include <map>
#include <vector>
#include <iostream>
#include <sstream>  // For std::istringstream, std::ostringstream, std::stringstream
#include <string>
#include <stdexcept>
#include <cctype>


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
		bool	_connectionAlive;

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
		std::string	_query;
		std::string	_version;

		bool	_useMultipartBoundary;   // true when we should use boundary-terminated framing
		std::string _boundary;               // boundary token without the leading "--"

		void	extractRequestLine();
		void	processRequestLine(std::string &line);
		void	trimBackslashR(std::string &line);
		void	trimSpaces(std::string	&line);

		/* Header */
		void	extractHeader();
		void	analyzeHeader();
		void	checkContentLength();
		void	checkChunked();
		void	processConnection();
		bool	evaluateAlive(const std::string version, const bool hasConnection, const std::string connection);
		bool	hasMultipart() const;
		bool	extractBoundary(const std::string &ctype, std::string &outBoundary) const;

		/* Body */
		void	processChunked();
		void	processUnchunked();

	public:
		HTTPRequest();
		HTTPRequest(int socketFD);
		~HTTPRequest();
		HTTPRequest(const HTTPRequest &other);
		HTTPRequest	&operator=(const HTTPRequest &other);

		void	feed(std::string &data);
		void	resetForNextRequest();
		size_t	endOfMessageOffset() const;

		/* Getters */
		const std::string &getRawString() const;
		const std::string &getRawHeader() const;
		const std::string &getRawBody() const;
		const std::map<std::string, std::string> &getHeaderMap() const;
		const std::vector<char> &getBodyVector() const;
		bool isHeaderComplete() const;
		bool isBodyComplete() const;
		bool isConnectionAlive() const;
		bool isChunked() const;
		const std::string &getMethod() const;
		const std::string &getPath() const;
		const std::string &getQueryString() const;
		const std::string &getVersion() const;
		const int &getSocketFD() const;

		/* Setters */
		void setRawString(std::string &rawString);
		void setRawHeader(const std::string &rawHeader);
		void setRawBody(const std::string &rawBody);
		void setConnectionAlive(const bool &connectionAlive);
		void setHeaderMap(const std::map<std::string, std::string> &header);
		void setBodyVector(const std::vector<char> &body);
		void setMethod(const std::string &method);
		void setPath(const std::string &path);
		void setQueryString(const std::string &query);
		void setVersion(const std::string &version);
		void setSocketFD(const int &socketFD);

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

		/* Utility */
		std::string	connectionHeader(bool keep) const;

};

#endif