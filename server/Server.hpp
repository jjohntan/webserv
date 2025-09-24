#ifndef SERVER_HPP
# define SERVER_HPP

#include <vector>
#include <string.h>
#include "../config_files/config.hpp"
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
#include "../http/HTTP.hpp"
#include "../http/HTTPRequest/HTTPRequest.hpp"


class Server
{
	private:
		std::vector<int> listening_sockets;
		std::vector<struct pollfd> pfds;
		std::vector<ServerConfig> servers; // configurations parsed from config file
		// stored root for single-server compatibility (optional)
		std::string root;
		std::map<int, time_t> last_activity; //track last activity per fd
		int timeout;

		// [ADD] Per-client write buffer + close-after-write flag
		struct ClientState { std::string outbox; bool close_after_write; };
		std::map<int, ClientState> client_state_; // by client fd  // [ADD]
		
		// helper
		void addNewConnection(int listen_fd, std::map<int, HTTPRequest> &request_map);
		void addPfds(int client_fd);
		void removePfds(int i);
		bool isListeningSocket(int fd);
		void checkTimeOut(std::map<int, HTTPRequest> request_map);
		void enableWrite(int fd);   // [ADD]
		void disableWrite(int fd);  // [ADD]

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
		void queueResponse(int fd, const std::string& data); // [ADD]
		void markCloseAfterWrite(int fd); // [ADD]
		friend void readClientData(int socketFD, std::map<int, HTTPRequest>& requestMap, std::vector<struct pollfd>& fds, size_t &i, const std::vector<ServerConfig>& servers, Server& srv);

};

#endif