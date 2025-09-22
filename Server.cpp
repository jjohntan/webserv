#include "Server.hpp"

#include <csignal>

volatile sig_atomic_t g_running = true;

void signalHandler(int signum)
{
    (void)signum;
    g_running = false;
}

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
	// cast it to a sockaddr pointer because of all the extra space it has for larger addresses
	struct sockaddr_storage client_addr;
	socklen_t addr_len = sizeof(client_addr);
	int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &addr_len);

	if (client_fd == -1)
	{
		perror("accept");
	}
	else
	{
		// [ADD] Optional: leave blocking; poll() ensures readiness.
		// If you switch to nonblocking, DO NOT inspect errno after send/read.
		addPfds(client_fd);
		requestMap[client_fd] = HTTPRequest(client_fd); // create new HTTPRequest if haven't
		client_state_[client_fd] = ClientState();       // [ADD] init outbox
	}
}

bool Server::isListeningSocket(int fd)
{
	for (size_t i = 0; i < listening_sockets.size(); i++)
	{
		if (listening_sockets[i] == fd)// socket is our listening socket
		{
			return true;
		}
	}
	return false;
}

// Main loop
void Server::run()
{
	std::map<int, HTTPRequest> request_map;

	std::cout << "waiting for connections" << std::endl;
	while (true)
	{
		int ready_fd = poll(pfds.data(), pfds.size(), -1);
		if (ready_fd < 0)
		{
			perror("poll failed");
			break;
		}

		for (size_t i = 0; i < pfds.size(); i++)
		{
			// [ADD] Handle exactly one write OR one read per client per poll tick
			if (pfds[i].revents & POLLOUT)
			{
				int fd = pfds[i].fd;
				std::map<int, ClientState>::iterator csit = client_state_.find(fd);
				if (csit == client_state_.end() || csit->second.outbox.empty())
				{
					disableWrite(fd); // nothing to write; stop POLLOUT  // [ADD]
				}
				else
				{
					const std::string &buf = csit->second.outbox;
					ssize_t n = send(fd, buf.data(), buf.size(), 0);
					if (n > 0)
					{
						// remove the bytes we wrote (single write per poll)       // [ADD]
						csit->second.outbox.erase(0, static_cast<size_t>(n));
						if (csit->second.outbox.empty())
							disableWrite(fd); // switch back to read-only         // [ADD]
					}
					else
					{
						// DO NOT inspect errno; generic log and close.            // [ADD]
						std::cerr << "send failed on fd " << fd << "\n";          // [ADD]
						close(fd);                                                 // [ADD]
						client_state_.erase(fd);                                   // [ADD]
						request_map.erase(fd);                                     // [ADD]
						pfds.erase(pfds.begin() + i);                              // [ADD]
						--i;                                                       // [ADD]
					}
				}
				pfds[i].revents = 0;
				continue; // one write OR one read per poll tick                   // [ADD]
			}

			// check if someone ready to read (or closed)
			if (pfds[i].revents & (POLLIN | POLLHUP))
			{
				// if is listener, it is a new connection
				if (isListeningSocket(pfds[i].fd))
				{
					addNewConnection(pfds[i].fd, request_map);// accept a new connection
				}
				// just a regular client
				else
				{
					readClientData(pfds[i].fd, request_map, pfds, i, servers, *this);
				}
			}
			pfds[i].revents = 0;
		}
	}
	 // Close all client connections
	for (size_t i = 0; i < pfds.size(); i++)
	{
		if (pfds[i].fd >= 0)
		{
			close(pfds[i].fd);
		}
	}

	// // Close server socket
	// if (sockfd >= 0)
	// {
	//     close(sockfd);
	//     sockfd = -1;
	// }

	// Clear the pollfd vector
	pfds.clear();

	std::cout << "Server shut down gracefully" << std::endl;
}

int Server::createListeningSocket(const std::string& port_str)
{
	int opt = 1;
	int res;
	//
	struct addrinfo hints, *ai, *ptr;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;       // use IPv4
	hints.ai_socktype = SOCK_STREAM; // TCP
	hints.ai_flags = AI_PASSIVE;

	//Allocate memory and create a new linked list of addrinfo
	if ((res = getaddrinfo(NULL, port_str.c_str(), &hints, &ai)) != 0)
	{
		fprintf(stderr, "server: %s\n", gai_strerror(res));
		return -1;
	}

	int sockfd = -1;
	
	// // Set sockfd to non-blocking
	// fcntl(sockfd, F_SETFL, O_NONBLOCK);
	for (ptr = ai; ptr != NULL; ptr = ptr->ai_next)
	{
		sockfd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (sockfd < 0)
		{
			continue;
		}
		// [ADD] If you want non-blocking listeners, set it **after** socket()
		fcntl(sockfd, F_SETFL, O_NONBLOCK);
		
		// set sockfd to allow multiple connection
		setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));

		if (bind(sockfd, ptr->ai_addr, ptr->ai_addrlen) < 0)
		{
			close(sockfd);
			sockfd = -1;
			continue;
		}
		break;
	}
	
	if (ptr == NULL)
	{
		return -1;
	}
	
	//deallocate memory created by geraddrinfo
	freeaddrinfo(ai);

	if (sockfd < 0)
	{
		return -1;
	}

	if (listen(sockfd, 10) == -1)
	{
		perror("listen");
		close(sockfd);
		return -1;
	}

	// keep track of listening socket
	listening_sockets.push_back(sockfd);

	// also add to poll set
	struct pollfd pfd;
	pfd.fd = sockfd;
	pfd.events = POLLIN;// check ready to read
	pfd.revents = 0;
	pfds.push_back(pfd);

	return sockfd;
}

bool Server::start()
{
	// collect unique ports from servers and create a listening socket for each
	std::set<int> ports;
	for (size_t i = 0; i < servers.size(); ++i)
	{
		ports.insert(servers[i].port);
	}
	// if no servers provided, fall back to default socket_fd if previously set
	if (ports.empty())
	{
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


// ============================ Helpers ============================  // [ADD]
void Server::enableWrite(int fd)
{
	for (size_t i = 0; i < pfds.size(); ++i)
		if (pfds[i].fd == fd) { pfds[i].events |= POLLOUT; break; }
}
void Server::disableWrite(int fd)
{
	for (size_t i = 0; i < pfds.size(); ++i)
		if (pfds[i].fd == fd) { pfds[i].events &= ~POLLOUT; break; }
}
void Server::queueResponse(int fd, const std::string& data)
{
	client_state_[fd].outbox.append(data);
	enableWrite(fd);
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