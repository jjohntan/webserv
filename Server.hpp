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

class Server
{
	private:
		int socket_fd;
		std::vector<struct pfd> pfds;
		int client_fd;
	public:
		// constructor
		Server(/* args */);
		// destructor
		~Server();
	
		int createListeningSocket();
		void init();
		void run();
		// helper
		void addNewConnection();
		void readClientData();
		void addToPfds();
		void removeFromPfds();
		
};

#endif
