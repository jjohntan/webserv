#include "HTTP.hpp"

/*
	Add a map from begining like the global_map
	When dealing with first connection, please create the HTTPRequest

	newConnection(serverFd, &pfds, &fd_count, &fd_size);
	requestMap[socketFD] = HTTPRequest(socketFD); // create a new object if new connection
*/

std::map<int, HTTPRequest> g_requestMap;

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

			HTTPResponse	response(cgiOutput.getStatusLine(),cgiOutput.getContent(), socketFD);
			response.processHTTPResponse();

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

		HTTPResponse error_response(400, "Bad Request");
		error_response.sendResponse();
		return (true);
	}
}
