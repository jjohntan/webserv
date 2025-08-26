#include "Server.hpp"

std::map<int, HTTPRequest> g_requestMap;

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

void	Server::readClientData(std::map<int, HTTPRequest>& requestMap, size_t &i)
{
	char	buffer[READ_BYTES] = {0};
	ssize_t	read_bytes = recv(socket_fd, buffer, READ_BYTES, 0);

	if (read_bytes <= 0)
	{
		if (read_bytes == 0)
			std::cout << "Socket FD " << socket_fd << " disconnected\n";
		else
			perror("recv");
		// Remove client socket from poll set and the map
		close(socket_fd);
		requestMap.erase(socket_fd);
		pfds.erase(pfds.begin() + i);
		--i; // Decrement to not skip element after erase
		return ;
	}
	std::string	data(buffer, read_bytes);
	bool	isClearing = false;
	isClearing = processClientData(socket_fd, requestMap, data);
	if (isClearing == true)
	{
		// Remove client socket from poll set and the map
		close(socket_fd);
		requestMap.erase(socket_fd);
		pfds.erase(pfds.begin() + i);
		--i; // Decrement to not skip element after erase
	}
	
}

// void Server::readClientData(int i)
// {
// 	char buf[1024];
// 	int sender_fd = pfds[i].fd;
// 	int bytes_received = recv(sender_fd, buf, sizeof(buf), 0);// receive incoming data from a connected client
// 	if (bytes_received > 0)
// 		buf[bytes_received] = '\0';
	
// 	if (bytes_received <= 0)
// 	{
// 		if (bytes_received == 0)
// 		{
// 			printf("server: socket %d closed\n", sender_fd);
// 		}
// 		else
// 		{
// 			perror("recv");
// 		}
// 		close(sender_fd);
// 		removePfds(i);
// 	}
// 	else
// 	{
// 		printf("server: recv from fd %d: %s\n", sender_fd, buf);

// 		for (unsigned int j = 0; j < pfds.size(); j++)
// 		{
// 			int dest_fd = pfds[j].fd;
// 			if (dest_fd != socket_fd && dest_fd != sender_fd)
// 			{
// 				const char *response = "HTTP/1.1 200 OK\r\n"
//                        "Content-Length: 13\r\n"
//                        "Content-Type: text/plain\r\n"
//                        "\r\n"
//                        "Hello, world!";
// 				send(sender_fd, response, strlen(response), 0);// hardcode
// 				// if (send(dest_fd, buf, bytes_received, 0) == -1)
// 				// {
// 				// 	perror("send");
// 				// }
// 			}
// 		}
// 	}
// }

void Server::addNewConnection(int listen_fd)
{
	struct sockaddr_storage client_addr;
	socklen_t addr_len = sizeof(client_addr);
	int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &addr_len);

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
bool Server::isListeningSocket(int fd) {
    for (size_t i = 0; i < listening_sockets.size(); i++) {
        if (listening_sockets[i] == fd)
            return true;
    }
    return false;
}

void Server::run()
{
    std::cout << "waiting for connections" << std::endl;
    while (true)
    {
        int ready_fd = poll(pfds.data(), pfds.size(), -1);
        if (ready_fd < 0) {
            perror("poll failed");
            break;
        }

        for (size_t i = 0; i < pfds.size(); i++) {
            if (pfds[i].revents & (POLLIN | POLLHUP)) {
                if (isListeningSocket(pfds[i].fd))
				{
                    addNewConnection(pfds[i].fd);
                }
				else
				{
                    readClientData(g_requestMap, i);
                }
            }
            pfds[i].revents = 0;
        }
    }
}

int Server::createListeningSocket(const std::string& port_str)
{
    int opt = 1;
    int rv;
    struct addrinfo hints, *ai, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;       // use IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, port_str.c_str(), &hints, &ai)) != 0) {
        fprintf(stderr, "server: %s\n", gai_strerror(rv));
        return -1;
    }

    int sockfd = -1;
    for (p = ai; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd < 0) continue;

        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) < 0) {
            close(sockfd);
            sockfd = -1;
            continue;
        }
        break;
    }

    freeaddrinfo(ai);

    if (sockfd < 0) return -1;

    if (listen(sockfd, 10) == -1) {
        perror("listen");
        close(sockfd);
        return -1;
    }

    // keep track of listening socket
    listening_sockets.push_back(sockfd);

    // also add to poll set
    struct pollfd pfd;
    pfd.fd = sockfd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    pfds.push_back(pfd);

    return sockfd;
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

Server::Server()
{

}

Server::Server(int port, const std::string& root, const std::vector<ServerConfig>& servers)
: socket_fd(-1), servers(servers), root(root)
{
	(void)port; // legacy single-port ctor keeps signature but real ports come from servers vector
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