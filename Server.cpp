#include "Server.hpp"

#include <set>

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

		for (unsigned int j = 0; j < pfds.size(); j++)
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
	struct sockaddr_storage client_addr;
	socklen_t addr_len = sizeof(client_addr);
	int client_fd = accept(socket_fd, (struct sockaddr *)&client_addr, &addr_len);

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
		for (unsigned int i = 0; i < pfds.size(); i++)
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

Server::Server(): socket_fd(-1)
{

}

Server::Server(int port, const std::string& root, const std::vector<ServerConfig>& servers)
: socket_fd(-1), servers(servers), root(root)
{
	(void)port; // legacy single-port ctor keeps signature but real ports come from servers vector
}

bool Server::start()
{
	// collect unique ports from servers and create a listening socket for each
	std::set<int> ports;
	for (size_t i = 0; i < servers.size(); ++i) {
		ports.insert(servers[i].port);
	}
	// if no servers provided, fall back to default socket_fd if previously set
	if (ports.empty()) {
		// try to create a default listener on 8080
		int fd = createListeningSocket(std::string("8080"));
		return fd >= 0;
	}

	for (std::set<int>::const_iterator it = ports.begin(); it != ports.end(); ++it) {
		std::ostringstream oss;
		oss << *it;
		int fd = createListeningSocket(oss.str());
		if (fd < 0) {
			std::cerr << "Failed to create listening socket on port " << *it << std::endl;
			return false;
		}
	}
	return true;
}

int Server::createListeningSocket(const std::string& port_str)
{
	int opt = 1;
	int rv;
	
	// hints: parameter specifies the preferred socket type, or protocol
	struct addrinfo hints, *ai, *p;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;// use IPV4
	hints.ai_socktype = SOCK_STREAM;// use TCP
	hints.ai_flags = AI_PASSIVE;
    
	if ((rv = getaddrinfo(NULL, port_str.c_str(), &hints, &ai)) != 0)
	{
		fprintf(stderr, "server: %s\n", gai_strerror(rv));
		return -1;
	}
	
	for (p = ai; p != NULL; p = p->ai_next)
	{
		// creating socket
		socket_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (socket_fd < 0)
		{
			continue;
		}
		
		// setting serverFd to allow multiple connection
		setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));
		
		// binding socket to the network address and port
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
	
	//listening for incoming connection
	if (listen(socket_fd, 10) == -1)
	{
		perror("listen");
		return -1;
	}
	struct pollfd pfd;
	pfd.fd = socket_fd;// monitor server socket
	pfd.events = POLLIN;
	pfd.revents = 0;
	pfds.push_back(pfd);
	return socket_fd;
}

Server::~Server()
{
	for (size_t i = 0; i < pfds.size(); ++i)
	{
		close(pfds[i].fd);
	}
}

	// Server

	// socket
	//    |
	// bind
	//    |
	// listen
	//    |
	// accept
	//    |
	// recv
	//    |
	// send
	//    |
	// recv