#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <iostream>
#include <fcntl.h>
#include <poll.h>

int createListenerSocket(int port)
{
	//creating server socket
	int serverFd = socket(AF_INET, SOCK_STREAM, 0);
	if (serverFd == -1)
	{
		std::cout << "socket failed" << std::endl;
		exit (EXIT_FAILURE);
	}
	//Make socket non-blocking
	fcntl(serverFd, F_SETFL, O_NONBLOCK);
	//defining server address
	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	address.sin_addr.s_addr = INADDR_ANY;
	
	//binding server socket
	if (bind(serverFd, (sockaddr *)&address, sizeof(address)) == -1)
	{
		std::cout << "bind failed" << std::endl;
		exit (EXIT_FAILURE);
	}
	
	//listening for connection
	if (listen(serverFd, 5) == -1)
	{
		std::cout << "listen failed" << std::endl;
		exit(EXIT_FAILURE);
	}
	return (serverFd);
}

int main()
{
	//create socket and non-blocking
	int serverFd = createListenerSocket(8080);
	
	//init/set up pollfd
	struct pollfd pfds[1];
	
	pfds[0].fd = serverFd;//monitor server socket
	pfds[0].events = POLLIN;
	//main loop
	while (true)
	{
		int status = poll(pfds, 1, 2500);
		if (status == -1)
		{
			std::cout << "timeout" << std::endl;
		}
		//loop throught array of socket
		for (int i = 0; i < 1; i++)
		{
			if ((pfds[i].revents & POLLIN) != 1)
			{
				std::cout << "socket not ready to read" << std::endl;
				continue;
			}
			std::cout << pfds[i].fd << " ready for I/O operation" << std::endl;
			if (pfds[i].fd == serverFd)//socket is our listening server socket
			{
				std::cout << "accept new connection" << std::endl;
				//accepting a client connection
				int clientFd = accept(serverFd, NULL, NULL);
				
				//receiving data from the client
				char buffer[1024] = {0};
				recv(clientFd, buffer, sizeof(buffer), 0);
				std::cout << "Message from client: " << buffer << std::endl;
				
				std::string body = "<h1>Hello World, 8080!</h1>";
				
				std::string response = "HTTP/1.1 200 OK\r\n";
				response += "Content-Type: text/html\r\n";
				response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
				response += "Connection: close\r\n";
				response += "\r\n";
				response += body;
				
				send(clientFd, response.c_str(), response.size(), 0);
				std::cout << "------------------Hello-------------" << std::endl;
				close (clientFd);
			}
			else
			{
				std::cout << "read data from socket" << std::endl;
			}
		}
		std::cout << "------------------waiting for new connection-------------" << std::endl;
	}
	//closing the socket
	close (serverFd);
}