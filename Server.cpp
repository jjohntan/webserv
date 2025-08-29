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

void Server::addNewConnection(int listen_fd, std::map<int, HTTPRequest> &requestMap)
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
		requestMap[client_fd] = HTTPRequest(client_fd); // create new HTTPRequest if haven't
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
    std::map<int, HTTPRequest> request_map;

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
                    addNewConnection(pfds[i].fd, request_map);
                }
				else
				{
                    readClientData(pfds[i].fd, request_map, pfds, i);
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

	for (std::set<int>::const_iterator it = ports.begin(); it != ports.end(); ++it)
    {
		std::ostringstream oss;
		oss << *it;
		int fd = createListeningSocket(oss.str());
		if (fd < 0)
        {
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
: servers(servers), root(root)
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