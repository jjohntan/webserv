#include "Server.hpp"
#include <csignal>

void sendTimeoutResponse(int fd)
{
	// Read the 408.html file
	std::ifstream file("408.html");
	std::string body;
	
	if (file.is_open())
	{
		body = std::string((std::istreambuf_iterator<char>(file)), 
						std::istreambuf_iterator<char>());
		file.close();
	}
	else
	{
		body = "<html><body><h1>408 Request Timeout</h1></body></html>";
	}
	
	// Use stringstream for number to string conversion
	std::stringstream ss;
	ss << body.length();
	
	std::string response = "HTTP/1.1 408 Request Timeout\r\n"
						"Connection: close\r\n"
						"Content-Type: text/html\r\n"
						"Content-Length: " + ss.str() + "\r\n\r\n" +
						body;
	
	send(fd, response.c_str(), response.length(), MSG_DONTWAIT);
	std::cout << "Sent 408 timeout response to fd " << fd << std::endl;
}

volatile sig_atomic_t g_running = true;

void signalHandler(int signum)
{
	(void)signum;
	g_running = false;
}

/**
 * Remove a file descriptor at a give index from the set
 */
void Server::removePfds(int i)
{
	pfds.erase(pfds.begin() + i);
}

/**
 * Add a new file descriptor to the set
 */
void Server::addPfds(int client_fd)
{
	struct pollfd pfd;
	pfd.fd = client_fd;
	pfd.events = POLLIN;// check ready to read
	pfd.revents = 0;
	pfds.push_back(pfd);
}

void Server::addNewConnection(int listen_fd, std::map<int, HTTPRequest> &requestMap)
{
	// cast it to a sockaddr pointer because of all the extra space it has for larger addresses
	struct sockaddr_storage client_addr;
	socklen_t addr_len = sizeof(client_addr);
	// now accept an incoming connection
	int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &addr_len);

	if (client_fd == -1)
	{
		perror("accept");
	}
	else
	{
		// [ADD] make client non-blocking
		int flags = fcntl(client_fd, F_GETFL, 0);
		if (flags != -1) 
			fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);  // [ADD]

		// [ADD] Optional: leave blocking; poll() ensures readiness.
		// If you switch to nonblocking, DO NOT inspect errno after send/read.
		addPfds(client_fd);
		requestMap[client_fd] = HTTPRequest(client_fd); // create new HTTPRequest if haven't
		client_state_[client_fd] = ClientState();       // [ADD] init outbox
		client_state_[client_fd].close_after_write = false; // [ADD]
		last_activity[client_fd] = time(NULL);
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
	while (g_running) // [CHANGE] use your SIGINT flag to exit the loop cleanly
	{
		int ready_fd = poll(pfds.data(), pfds.size(), 1000);
		if (ready_fd < 0)
		{
			perror("poll failed");
			break;
		}

		time_t current_time = time(NULL);
		for (size_t i = 0; i < pfds.size(); i++)
		{
			int fd = pfds[i].fd;
			
			// Skip listening sockets for timeout checking
			if (isListeningSocket(fd))
				continue;
				
			if (last_activity.count(fd) && (current_time - last_activity[fd] > timeout))
			{
				std::cout << "Timeout closing fd " << fd << " (idle for " 
						<< (current_time - last_activity[fd]) << "s)" << std::endl;
				
				// Send proper 408 timeout response
				sendTimeoutResponse(fd);
				
				// Clean up
				close(fd);
				client_state_.erase(fd);
				request_map.erase(fd);
				last_activity.erase(fd);
				removePfds(i);
				--i; // Adjust index after removal
				continue;
			}
		}
		for (size_t i = 0; i < pfds.size(); i++)
		{
			 // Handle error-y revents (prevents “mystery hangs”)
			if (pfds[i].revents & (POLLERR | POLLNVAL)) {     // [ADD]
				int fd = pfds[i].fd;                          // [ADD]
				close(fd);                                    // [ADD]
				client_state_.erase(fd);                      // [ADD]
				request_map.erase(fd);                        // [ADD]
				last_activity.erase(fd);
				pfds.erase(pfds.begin() + i);                 // [ADD]
				--i;                                          // [ADD]
				continue;                                     // [ADD]
			}                                                 // [ADD]

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
						{
							if (csit->second.close_after_write)
							{
								// graceful close after fully flushed               // [ADD]
								close(fd);                                         // [ADD]
								client_state_.erase(fd);                           // [ADD]
								request_map.erase(fd);                             // [ADD]
								last_activity.erase(fd);
								pfds.erase(pfds.begin() + i);                      // [ADD]
								--i;                                               // [ADD]
								// do not touch pfds[i].revents after erase        // [ADD]
								continue;                                          // [ADD]
							}
							disableWrite(fd); // switch back to read-only
						}
					}
					else
					{
						// DO NOT inspect errno; generic log and close.            // [ADD]
						std::cerr << "send failed on fd " << fd << "\n";          // [ADD]
						close(fd);                                                 // [ADD]
						client_state_.erase(fd);                                   // [ADD]
						request_map.erase(fd);                                     // [ADD]
						last_activity.erase(fd);
						pfds.erase(pfds.begin() + i);                              // [ADD]
						--i;                                                       // [ADD]
					}
				}
				pfds[i].revents = 0;
				last_activity[fd] = time(NULL);
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

	// Clear the pollfd vector
	pfds.clear();

	std::cout << "Server shut down gracefully" << std::endl;
}

int Server::createListeningSocket(const std::string& port_str)
{
	int opt = 1;
	int res;
	struct addrinfo hints, *ai, *ptr;
	
	// get host info, make socket, and connect it
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;       // use IPv4
	hints.ai_socktype = SOCK_STREAM; // TCP strream sockets
	hints.ai_flags = AI_PASSIVE;// use my IP address

	//Allocate memory and create a new linked list of addrinfo
	if ((res = getaddrinfo(NULL, port_str.c_str(), &hints, &ai)) != 0)
	{
		fprintf(stderr, "server: %s\n", gai_strerror(res));
		return -1;
	}

	int sockfd = -1;
	
	// loop through all the result s and bind to the first we can
	for (ptr = ai; ptr != NULL; ptr = ptr->ai_next)
	{
		// make a socket using the information from getaddrinfo()
		sockfd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (sockfd < 0)
		{
			continue;
		}
		// [ADD] If you want non-blocking listeners, set it **after** socket()
		// Set to non-blocking
		fcntl(sockfd, F_SETFL, O_NONBLOCK);
		
		// set sockfd to allow multiple connection
		setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));
		
		if (bind(sockfd, ptr->ai_addr, ptr->ai_addrlen) < 0)
		{
			close(sockfd);
			sockfd = -1;
			continue;
		}
		break;// connected successfully
	}
	
	// looped off the end of the list with no connection
	if (ptr == NULL)
	{
		return -1;
	}
	
	// deallocate memory created by geraddrinfo
	freeaddrinfo(ai);

	if (sockfd < 0)
	{
		return -1;
	}

	// set sockfd up to be a server socket
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
	pfd.fd = sockfd;// the socket descriptor
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

Server::Server() {}

Server::Server(int port, const std::string& root, const std::vector<ServerConfig>& servers)
: servers(servers), root(root), timeout(15)
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

// [ADD] Ask to close once all queued bytes are sent
void Server::markCloseAfterWrite(int fd)
{
	std::map<int, ClientState>::iterator it = client_state_.find(fd);
	if (it != client_state_.end())
	{
		it->second.close_after_write = true;
		// ensure POLLOUT wakes us to flush & close
		enableWrite(fd);
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