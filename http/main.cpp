#include "HTTPRequest.hpp"
#include <iostream>
#include <string>
#include "Request.hpp"

int main()
{
	// ✅ Test Case 1: Unchunked
	std::string unchunkedRequest =
		"POST /submit HTTP/1.1\r\n"
		"Host: example.com\r\n"
		"Content-Length: 13\r\n"
		"Content-Type: text/plain\r\n"
		"\r\n"
		"Hello, world!";

	HTTPRequest req1;
	req1.feed(unchunkedRequest);
	if (req1.isHeaderComplete() && req1.isBodyComplete())
	{
		std::cout << MAGENTA << "\n====== Unchunked Request ======\n" << RESET;
		printRequest(req1);
	}
	else
	{
		std::cerr << RED << "Unchunked request not fully parsed.\n" << RESET;
	}

	// ✅ Test Case 2: Chunked (with no trailers)
	std::string chunkedRequest =
		"POST /upload HTTP/1.1\r\n"
		"Host: example.com\r\n"
		"Transfer-Encoding: chunked\r\n"
		"\r\n"
		"7\r\n"
		"Mozilla\r\n"
		"9\r\n"
		"Developer\r\n"
		"7\r\n"
		"Network\r\n"
		"0\r\n"
		"\r\n";

	HTTPRequest req2;
	req2.feed(chunkedRequest);
	if (req2.isHeaderComplete() && req2.isBodyComplete())
	{
		std::cout << MAGENTA << "\n====== Chunked Request (No Trailer) ======\n" << RESET;
		printRequest(req2);
	}
	else
	{
		std::cerr << RED << "Chunked request not fully parsed.\n" << RESET;
	}

	// ✅ Test Case 3: Chunked (with trailers)
	std::string chunkedWithTrailer =
		"POST /upload HTTP/1.1\r\n"
		"Host: example.com\r\n"
		"Transfer-Encoding: chunked\r\n"
		"\r\n"
		"5\r\n"
		"Hello\r\n"
		"6\r\n"
		"World!\r\n"
		"0\r\n"
		"Expires: Wed, 21 Oct 2015 07:28:00 GMT\r\n"
		"\r\n";

	HTTPRequest req3;
	req3.feed(chunkedWithTrailer);
	if (req3.isHeaderComplete() && req3.isBodyComplete())
	{
		std::cout << MAGENTA << "\n====== Chunked Request (With Trailer) ======\n" << RESET;
		printRequest(req3);
	}
	else
	{
		std::cerr << RED << "Chunked request with trailer not fully parsed.\n" << RESET;
	}

	return 0;
}
