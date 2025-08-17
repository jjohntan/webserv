#include "Server.hpp"

#define PORT "8080"

void Server::removePfds(int i)
{
	pfds.erase(pfds.begin() + i);
}

void Server::addPfds(int client_fd)
{
	struct pollfd pfd;
	pfd.fd = client_fd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	pfds.push_back(pfd);
}

void Server::readClientData(int i)
{
	char buf[1024];
	int sender_fd = pfds[i].fd;
	int bytes_received = recv(sender_fd, buf, sizeof(buf), 0);// receive incoming data from a connected client

	if (bytes_received <= 0)
	{
		if (bytes_received == 0)
		{
			printf("server: socket %d closed\n", sender_fd);
		}
		else
		{
			perror("recv");
		}
		close(sender_fd);
		removePfds(i);
	}
	else
	{
		printf("server: recv from fd %d: %s\n", sender_fd, buf);

		for (int j = 0; j < pfds.size(); j++)
		{
			int dest_fd = pfds[j].fd;
			if (dest_fd != socket_fd && dest_fd != sender_fd)
			{
				if (send(dest_fd, buf, bytes_received, 0) == -1)
				{
					perror("send");
				}
			}
		}
	}
}

void Server::addNewConnection()
{
	struct sockaddr_storage remote_addr;
	socklen_t addr_len = sizeof(remote_addr);
	int client_fd = accept(socket_fd, (struct sockaddr *)&remote_addr, &addr_len);

	if (client_fd == -1)
	{
		perror("accept");
	}
	else
	{
		addPfds(client_fd);
	}
}

// Main loop
void Server::run()
{
	std::cout << "waiting for connections" << std::endl;
	while (true)
	{
		int ready_fd = poll(pfds.data(), pfds.size(),-1);
		if (ready_fd < 0)
		{
			perror("poll failed");
			break;
		}
		for (int i = 0; i < pfds.size(); i++)
		{
			if (pfds[i].revents & (POLLIN | POLLHUP))
			{
				if (pfds[i].fd == socket_fd)
				{
					addNewConnection();
				}
				else
				{
					readClientData(i);
				}
			}
			pfds[i].revents = 0;
		}
	}
}

int Server::createListeningSocket()
{
	int yes = 1;
	int rv;
	
	// hints: parameter specifies the preferred socket type, or protocol
	struct addrinfo hints, *ai, *p;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	
	if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0)
	{
		exit(1);
	}
	
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
	if (p == NULL)
	{
		return -1;
	}
	freeaddrinfo(ai);
	if (listen(socket_fd, 10) == -1)
	{
		perror("listen");
		return -1;
	}
	struct pollfd pfd;
	pfd.fd = socket_fd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	pfds.push_back(pfd);
	return socket_fd;
}

Server::Server(): socket_fd(-1)
{

}

Server::~Server()
{
	for (size_t i = 0; i < pfds.size(); ++i)
	{
		close(pfds[i].fd);
	}
}