#include "Server.hpp"

#define PORT "8080";

void Server::removeFromPfds()
{
	client_fd--;
}

void Server::addToPfds()
{
	client_fd++;
}

void Server::readClientData()
{
	= recv();
}

void Server::addNewConnection()
{
	client_fd = accept();

	if (client_fd == -1)
	{
	}
	else
	{
		addToPfds();
	}
}

void Server::run()
{
	while (true)
	{
		int ready_fd = poll(pfds, ,2000);
		if (ready_fd < 0)
		{
			perror("poll failed");
		}
		for (int i = 0; i < ; i++)
		{
			if ( == socket_fd)
			{
				addNewConnection();
			}
			else
			{
				readClientData();
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
		socket_fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (socket_fd < 0)
		{
			continue;
		}
		setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
		if (bind(socket_fd, p->ai_addr, p->ai_addrlen) < 0)
		{
			close(socket_fd);
			continue;
		}
		break;
	}
	return socket_fd;
}
