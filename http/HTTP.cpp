#include "HTTP.hpp"
#include "http_cgi.hpp"
#include <poll.h>
#include <dirent.h>
#include <sys/stat.h>

void	printRequest(const HTTPRequest &req)
{
	std::cout << GREEN << "\n--- Request Line ---\n" << RESET;
	std::cout << "Method: " << req.getMethod() << std::endl;
	std::cout << "Path: " << req.getPath() << std::endl;
	std::cout << "Version: " << req.getVersion() << std::endl;

	std::cout << GREEN << "\n--- Headers ---\n" << RESET;
	const std::map<std::string, std::string>& headers = req.getHeaderMap();
	for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it)
		std::cout << it->first << ": " << it->second << std::endl;

	std::cout << GREEN << "\n--- Body ---\n" << RESET;
	std::cout << req.getRawBody() << std::endl;
}


// added server
void	readClientData(int socketFD, std::map<int, HTTPRequest>& requestMap, std::vector<struct pollfd>& fds, size_t &i, const std::vector<ServerConfig>& servers)
{
	char	buffer[READ_BYTES] = {0};
	ssize_t	read_bytes = recv(socketFD, buffer, READ_BYTES, 0);

	if (read_bytes <= 0)
	{
		// Remove client socket from poll set and the map
		close(socketFD);
		requestMap.erase(socketFD);
		fds.erase(fds.begin() + i);
		--i; // Decrement to not skip element after erase
		return ;
	}
	std::string	data(buffer, read_bytes);
	bool	isClearing = false;
	isClearing = processClientData(socketFD, requestMap, data, servers);
	if (isClearing == true)
	{
		// Remove client socket from poll set and the map
		close(socketFD);
		requestMap.erase(socketFD);
		fds.erase(fds.begin() + i);
		--i; // Decrement to not skip element after erase
	}
	
}


bool	processClientData(int socketFD, std::map<int, HTTPRequest>& requestMap, std::string	data, const std::vector<ServerConfig>& servers)
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

			
			// Call the main CGI and file serving function
			handleRequestProcessing(requestMap[socketFD], socketFD, servers);

			// Sending response back to corresponding socket
			std::cout << "Sending Response Back to Socket\n";
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