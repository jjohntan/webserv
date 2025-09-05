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
#include <set>
#include <fcntl.h>
#include "http/HTTP.hpp"
#include "http/HTTPRequest/HTTPRequest.hpp"


class Server
{
	private:
		std::vector<int> listening_sockets;
		std::vector<struct pollfd> pfds;
		std::vector<ServerConfig> servers; // configurations parsed from config file
		// stored root for single-server compatibility (optional)
		std::string root;
		
		// helper
		void addNewConnection(int listen_fd, std::map<int, HTTPRequest> &request_map);
		// void readClientData(int i);
		// void	readClientData(std::map<int, HTTPRequest>& requestMap, size_t &i);
		void addPfds(int client_fd);
		void removePfds(int i);
		bool isListeningSocket(int fd);
	public:
		// default constructor
		Server();
		// constructor
		Server(int port, const std::string& root, const std::vector<ServerConfig>& servers);
		// start listening on ports derived from the provided ServerConfig(s)
		// destructor
		~Server();
	
		int createListeningSocket(const std::string& port_str);
		void run();
		bool start();
};

#endif