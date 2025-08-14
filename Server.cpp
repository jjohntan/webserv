#include "Server.hpp"

#define PORT "8080";

void Server::run()
{
	while (true)
	{
		int = poll(listener, ,2000);
		if ()
		for (int i = 0; i < ; i++)
		{
			if ( == listener)
			{
				
			}
			else
			{
				
			}
		}
	}
}

void Server::init()
{
	
}

int Server::createListener()
{
	int yes = 1;
	int rv;
	// hints: parameter specifies the preferred socket type, or protocol
	struct addrinfo hints, *ai, *p;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	
	// if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0)
	// {
		
	// }
	
	for (p = ai; p != NULL; p = p->ai_next)
	{
		listener = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (listener < 0)
		{
			continue;
		}
		setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
		if (bind(listener, p->ai_addr, p->ai_addrlen) < 0)
		{
			close(listener);
			continue;
		}
		break;
	}
	return listener;
}