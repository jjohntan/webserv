#include "Server.hpp"
#include <csignal>

//volatile: tells the compiler not to optimize this variable away, because it might change unexpectedly
volatile sig_atomic_t g_running = true;

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

void signalHandler(int signum)
{
	(void)signum;
	g_running = false;
}

void setupSignalHandler()
{
	struct sigaction sa;
	sa.sa_handler = signalHandler;// which function to call
	sigemptyset(&sa.sa_mask);// don't block other signal during handler
	sa.sa_flags = 0;// no special behavior
	sigaction(SIGINT, &sa, NULL);// attach handler for ctrl + c
	sigaction(SIGTERM, &sa, NULL);// attach handler for kill pid
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
		// make client non-blocking
		int flags = fcntl(client_fd, F_GETFL, 0);
		if (flags != -1) 
			fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

		// Optional: leave blocking; poll() ensures readiness.
		// If you switch to nonblocking, DO NOT inspect errno after send/read.
		addPfds(client_fd);
		requestMap[client_fd] = HTTPRequest(client_fd);// create new HTTPRequest if haven't
		client_state_[client_fd] = ClientState();// init outbox
		client_state_[client_fd].close_after_write = false;
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

void Server::checkTimeOut(std::map<int, HTTPRequest> request_map)
{
	time_t current_time = time(NULL);
	for (size_t i = 0; i < pfds.size(); i++)
	{
		int fd = pfds[i].fd;
		
		// Skip listening sockets for timeout checking
		if (isListeningSocket(fd))
			continue;
		
		// If this file descriptor exists in my activity map and the client has been idle longer than the timeout, then close the connection.
		if (last_activity.count(fd) && (current_time - last_activity[fd] > timeout))
		{
			std::cout << "Timeout closing fd " << fd << " (idle for " << (current_time - last_activity[fd]) << "s)" << std::endl;
			
			// Send 408 timeout response
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
}

// Main loop
void Server::run()
{
	std::map<int, HTTPRequest> request_map;
	setupSignalHandler();

	std::cout << "waiting for connections" << std::endl;
	while (g_running)
	{
		int ready_fd = poll(pfds.data(), pfds.size(), 100);
		if (ready_fd < 0)
		{
			perror("poll failed");
			break;
		}
		checkTimeOut(request_map);
		for (size_t i = 0; i < pfds.size(); i++)
		{
			// Handle error-y revents (prevents “mystery hangs”)
			// POLLERR: An error has occurred on this socket.
			// POLLNVAL: Invalid request: fd not open (only returned in revents; ignored in events)
			if (pfds[i].revents & (POLLERR | POLLNVAL))
			{
				int fd = pfds[i].fd;
				close(fd);
				client_state_.erase(fd);
				request_map.erase(fd);
				last_activity.erase(fd);
				pfds.erase(pfds.begin() + i);
				--i;
				continue;
			}

			// Handle exactly one write OR one read per client per poll tick
			// POLLOUT: Alert me when I can send() data to this socket without blocking.
			if (pfds[i].revents & POLLOUT)
			{
				int fd = pfds[i].fd; // Get the file descriptor for this client
				// Find the client state for this fd
				std::map<int, ClientState>::iterator csit = client_state_.find(fd);
				// If we have no state for this fd, or nothing left to send, stop POLLOUT(send buffer empty)
				if (csit == client_state_.end() || csit->second.outbox.empty())
				{
					disableWrite(fd); // Remove POLLOUT event, nothing to write
				}
				else
				{
					// Get the data buffer to send
					// second: the value
					const std::string &buf = csit->second.outbox;
					// Try to send as much as possible in one go
					ssize_t n = send(fd, buf.data(), buf.size(), 0);
					if (n > 0)
					{
						// Remove the bytes that were successfully sent
						csit->second.outbox.erase(0, static_cast<size_t>(n));
						// If all data has been sent
						if (csit->second.outbox.empty())
						{
							// If we want to close after sending (Connection close)
							if (csit->second.close_after_write)
							{
								// Close the socket and clean up all state
								close(fd);
								client_state_.erase(fd);
								request_map.erase(fd);
								last_activity.erase(fd);
								pfds.erase(pfds.begin() + i);
								--i; // Adjust index after erase
								// Don't touch pfds[i].revents after erase
								continue;
							}
							// Otherwise, just stop POLLOUT and go back to read-only
							disableWrite(fd);
						}
					}
					else
					{
						// If send() failed (n <= 0), log and close the connection
						// Do NOT inspect errno, just close and clean up
						std::cerr << "send failed on fd " << fd << "\n";
						close(fd);
						client_state_.erase(fd);
						request_map.erase(fd);
						last_activity.erase(fd);
						pfds.erase(pfds.begin() + i);
						--i;
					}
				}
				// Reset revents for this pollfd (we handled the event)
				pfds[i].revents = 0;
				// Update last activity time for this client
				last_activity[fd] = time(NULL);
				// Only one write OR one read per poll tick, so continue
				continue;
			}

			// check if someone ready to read (or closed)
			// POLLIN: There is data to read
			// POLLHUP: The remote side of the connection hung up.
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
	client_state_.clear();
	last_activity.clear();

	std::cout << "Server shut down gracefully" << std::endl;
}

int Server::createListeningSocket(const std::string& port_str)
{
	int opt = 1;
	int res;
	// ai: pointer to addinfo,point to the head of a linked list(first node)
	// ptr: traverse the linked list
	struct addrinfo hints, *ai, *ptr;
	
	// get host info, make socket, and connect it
	memset(&hints, 0, sizeof(hints));// make sure the struct is empty
	hints.ai_family = AF_INET;       // use IPv4
	hints.ai_socktype = SOCK_STREAM; // TCP strream sockets
	hints.ai_flags = AI_PASSIVE;// use my IP address

	// Allocate memory and create a new linked list of addrinfo
	if ((res = getaddrinfo(NULL, port_str.c_str(), &hints, &ai)) != 0)
	{
		fprintf(stderr, "server: %s\n", gai_strerror(res));
		return -1;
	}

	int sockfd = -1;
	
	// loop through all the results and bind to the first we can
	for (ptr = ai; ptr != NULL; ptr = ptr->ai_next)
	{
		// make a socket using the information from getaddrinfo()
		sockfd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (sockfd < 0)
		{
			continue;
		}
		// non-blocking listeners, set after socket()
		// Set to non-blocking
		fcntl(sockfd, F_SETFL, O_NONBLOCK);
		
		// set sockfd to allow multiple connection
		setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));
		
		// bind it to the port we passed in to getaddrinfo():
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
	pfd.events = POLLIN;// check ready to read(what u ask)
	pfd.revents = 0;// result of events(what u get)
	pfds.push_back(pfd);

	return sockfd;
}

bool Server::start()
{
	std::set<int> ports;// set automatically ensures that each port number appears only once, and keeps sorted in ascending order.
	// collect unique ports from servers and create a listening socket for each
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

	// create listening socket for each port
	for (std::set<int>::const_iterator it = ports.begin(); it != ports.end(); ++it)
	{
		std::ostringstream oss;
		oss << *it;// convert it to string
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

// ============================ Helpers ============================  //

/*
	Tell poll() that you have something to write
	"|=" adds bits without removing the current one
*/
void Server::enableWrite(int fd)
{
	for (size_t i = 0; i < pfds.size(); ++i)
	{
		if (pfds[i].fd == fd) 
		{
			pfds[i].events |= POLLOUT; 
			break;
		}
	}
}

/*
	Tell poll() that you have nothing to write
	"~Flags" means inverse the bits
	&= keeps everything else, but clears the POLLOUT bit.

	Suppose POLLIN = 0b0001, POLLOUT = 0b0100.
	Current events = 0b0101 (POLLIN + POLLOUT).
	~POLLOUT = 0b1011.
	events & ~POLLOUT = 0b0101 & 0b1011 = 0b0001 → POLLOUT cleared, POLLIN kept.
*/
void Server::disableWrite(int fd)
{
	for (size_t i = 0; i < pfds.size(); ++i)
	{
		if (pfds[i].fd == fd)
		{
			pfds[i].events &= ~POLLOUT;
			break;
		}
	}
}

void Server::queueResponse(int fd, const std::string& data)
{
	client_state_[fd].outbox.append(data);// store response
	enableWrite(fd);
}

// Ask to close once all queued bytes are sent
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
	// send(POLLOUT)
	//    |
	// recv