#include "HTTP.hpp"

/*
	Add a map from begining like the global_map
	When dealing with first connection, please create the HTTPRequest

	newConnection(serverFd, &pfds, &fd_count, &fd_size);
	requestMap[socketFD] = HTTPRequest(socketFD); // create a new object if new connection
*/

// std::map<int, HTTPRequest> g_requestMap;

void printRequest(const HTTPRequest &req)
{
	std::cout << BLUE << "\n--- Request Line ---\n" << RESET;
	std::cout << "Method: " << req.getMethod() << std::endl;
	std::cout << "Path: " << req.getPath() << std::endl;
	std::cout << "Version: " << req.getVersion() << std::endl;

	std::cout << GREEN << "\n--- Headers ---\n" << RESET;
	for (std::map<std::string, std::string>::const_iterator it = req.getHeaderMap().begin(); it != req.getHeaderMap().end(); ++it)
		std::cout << it->first << ": " << it->second << std::endl;

	std::cout << CYAN << "\n--- Body ---\n" << RESET;
	std::cout << req.getRawBody() << std::endl;
}

std::string generateHttpResponseBody()
{
	return
		"Date: Fri, 22 Aug 2025 08:00:00 GMT\r\n"
		"Server: Webserv/0.1 C++98\r\n"
		"Content-Type: text/html; charset=utf-8\r\n"
		"Content-Length: 1088\r\n"
		"Connection: close\r\n"
		"\r\n"
		"<!doctype html>\n"
		"<html lang=\"en\">\n"
		"<head>\n"
		"<meta charset=\"utf-8\">\n"
		"<title>It works!</title>\n"
		"<style>\n"
		"  body{margin:0;font-family:Arial,Helvetica,sans-serif;background:linear-gradient(135deg,#1e3a8a,#9333ea,#ef4444,#f59e0b,#10b981);background-size:400% 400%;animation:grad 15s ease infinite;color:#fff;min-height:100vh;display:flex;align-items:center;justify-content:center;}\n"
		"  .card{background:rgba(255,255,255,0.12);padding:2rem 3rem;border-radius:16px;backdrop-filter:blur(6px);box-shadow:0 10px 30px rgba(0,0,0,0.25);text-align:center;}\n"
		"  h1{margin:0 0 0.5rem;font-size:2.2rem;text-shadow:0 2px 8px rgba(0,0,0,0.35)}\n"
		"  p{margin:0.25rem 0 0;font-size:1.1rem}\n"
		"  .rainbow{background:linear-gradient(90deg,#f87171,#fbbf24,#34d399,#60a5fa,#a78bfa,#f472b6);-webkit-background-clip:text;background-clip:text;color:transparent;}\n"
		"  @keyframes grad{0%{background-position:0% 50%}50%{background-position:100% 50%}100%{background-position:0% 50%}}\n"
		"</style>\n"
		"</head>\n"
		"<body>\n"
		"  <div class=\"card\">\n"
		"    <h1 class=\"rainbow\">200 OK</h1>\n"
		"    <p>Hello from your tiny C++ web server.</p>\n"
		"  </div>\n"
		"</body>\n"
		"</html>\n";
}

void	readClientData(int socketFD, std::map<int, HTTPRequest>& requestMap, std::vector<struct pollfd>& fds, size_t &i)
{
	char	buffer[READ_BYTES] = {0};
	ssize_t	read_bytes = recv(socketFD, buffer, READ_BYTES, 0);

	if (read_bytes <= 0)
	{
		if (read_bytes == 0)
			std::cout << "Socket FD " << socketFD << " disconnected\n";
		else
			perror("recv");
		// Remove client socket from poll set and the map
		close(socketFD);
		requestMap.erase(socketFD);
		fds.erase(fds.begin() + i);
		--i; // Decrement to not skip element after erase
		return ;
	}
	std::string	data(buffer, read_bytes);
	bool	isClearing = false;
	isClearing = processClientData(socketFD, requestMap, data);
	if (isClearing == true)
	{
		// Remove client socket from poll set and the map
		close(socketFD);
		requestMap.erase(socketFD);
		fds.erase(fds.begin() + i);
		--i; // Decrement to not skip element after erase
	}
	
}

bool	processClientData(int socketFD, std::map<int, HTTPRequest>& requestMap, std::string	data)
{
	try
	{
		// Feed data to HTTPRequest
		requestMap[socketFD].feed(data);

		// Check whether HTTPRequest has complete data
		if (requestMap[socketFD].isHeaderComplete() == true && requestMap[socketFD].isBodyComplete() == true)
		{
			// Print Debug Message
			std::cout << "Request From Socket " << socketFD << "had successfully converted into object!\n";
			printRequest(requestMap[socketFD]);

			// From Winnie
			// cgiOutput	cgiOutput;
			// output = runcgi(requestMap[socketFD]);
			// HTTPResponse	response(output.status_message, output.status_code, output.content, socketFD);

			// Hardcoded for body
			HTTPResponse response("OK", 200, generateHttpResponseBody(), socketFD);

			// Sending response back to corresponding socket
			std::cout << "Sending Response Back to Socket\n";
			response.sendResponse();
			return (true);
		}

		return (false); // so that clearing not happen
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error while processing client data from socket " << socketFD << "!\n";

		HTTPResponse error_response("Bad Request", 400);
		error_response.sendResponse();
		return (true);
	}
}
