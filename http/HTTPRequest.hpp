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
		std::string	_rawString;
		std::string	_rawHeader;
		std::string	_rawBody;
		bool	_headerComplete;
		bool	_bodyComplete;

		std::map<std::string, std::string> _header;
		bool	_chunked;
		size_t	_content_length;
		std::vector<char> _body;

		/* Request Line */
		std::string	_request_line;
		size_t	_request_line_len;
		std::string	_method;
		std::string	_path;
		std::string	_version;

		void	extractRequestLine();
		void	processRequestLine(std::string &line);
		void	trimBackslashR(std::string &line);
		void	trimSpaces(std::string	&line)

		/* Header & Body */
		void	separateHeaderBody();

		/* Header */
		void	extractHeader();
		void	checkChunked();

		/* Body */
		void	processChunked();
		void	processUnchunked();

	public:
		HTTPRequest();
		// HTTPRequest(std::string rawString);
		~HTTPRequest();
		HTTPRequest(const HTTPRequest &other);
		const HTTPRequest	&operator=(const HTTPRequest &other);

		void	feed(std::string &data);

		const std::string getRawString() const;
		void setRawString(std::string rawString);

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