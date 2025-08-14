#ifndef SERVER_HPP
# define SERVER_HPP

#include <vector>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
 #include <poll.h>

class Server
{
	private:
		int listener;
		std::vector<struct pfd> pfds;
	public:
		// constructor
		Server(/* args */);
		// destructor
		~Server();
	
		int createListener();
		void init();
		void run();
		
		// helper
		
};

#endif