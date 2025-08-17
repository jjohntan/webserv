#ifndef SERVER_HPP
# define SERVER_HPP

#include <vector>
#include <string.h>
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
		int socket_fd;
		std::vector<struct pollfd> pfds;
		// helper
		void addNewConnection();
		void readClientData(int i);
		void addPfds(int client_fd);
		void removePfds(int i);
	public:
		// constructor
		Server(/* args */);
		// destructor
		~Server();
	
		int createListeningSocket();
		void run();
};

#endif
