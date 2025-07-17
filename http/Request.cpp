#include "Request.hpp"

std::map<int, HTTPRequest>_socketMap;

void	storeSocketMap(int socketFD, std::string &text)
{
	HTTPRequest &req = _socketMap[socketFD];
	req.feed(text);
}