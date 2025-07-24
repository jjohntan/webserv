#include "Request.hpp"

std::map<int, HTTPRequest>_socketMap;

void	storeSocketMap(int socketFD, std::string &text)
{
	HTTPRequest &req = _socketMap[socketFD];
	req.feed(text);
}

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
