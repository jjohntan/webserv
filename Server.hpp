#ifndef SERVER_HPP
# define SERVER_HPP

#include <vector>
#include <string.h>
#include "config_files/config.hpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <iostream>

class Server
{
	private:
	int socket_fd;// store socket file descriptor
	std::vector<struct pollfd> pfds;
	std::vector<ServerConfig> servers; // configurations parsed from config file

	// stored root for single-server compatibility (optional)
	std::string root;
		// helper
		void addNewConnection();
		void readClientData(int i);
		void addPfds(int client_fd);
		void removePfds(int i);
	public:
	// constructor
	Server();
	Server(int port, const std::string& root, const std::vector<ServerConfig>& servers);
	// start listening on ports derived from the provided ServerConfig(s)
	bool start();
		// destructor
		~Server();
	
	int createListeningSocket(const std::string& port_str);
		void run();
};

#endif
