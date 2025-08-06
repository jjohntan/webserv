#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <iostream>
#include <fcntl.h>
#include <poll.h>
#include <vector>
#include <stdlib.h>

void deletePfds(struct pollfd **pfds, int i, int *fd_count)
{
	(*pfds)[i] = (*pfds)[*fd_count - 1];
	(*fd_count)--;
}

void addPfds(struct pollfd **pfds, int newfd, int *fd_count, int *fd_size)
{
	if (*fd_count == *fd_size)
	{
		*fd_size *= 2;
		*pfds = (struct pollfd*)realloc(*pfds, sizeof(**pfds) * (*fd_size));
	}
	
	(*pfds)[*fd_count].fd = newfd;
	(*pfds)[*fd_count].events = POLLIN;//check ready to read
	(*pfds)[*fd_count].revents = 0;
	
	(*fd_count)++;
}

void newConnection(int sockfd, struct pollfd **pfds, int *fd_count, int *fd_size)
{
	struct sockaddr_in client_addr;
	socklen_t addrlen = sizeof(client_addr);
	
	int clientFd = accept(sockfd, (struct sockaddr*)&client_addr, &addrlen);
	if (clientFd == -1)
	{
		perror("accept error");
		return;
	}
	// Add a new file descriptor to the pollfd array
	addPfds(pfds, clientFd, fd_count, fd_size);
}

void readData(int sockfd, int *fd_count, struct pollfd **pfds, int i)
{
	char buf[256];
	int dest_fd;
	
	int sender_fd = (*pfds)[i].fd;
	int bytes = recv(sender_fd, buf, sizeof(buf), 0);
	
	if (bytes <= 0)
	{
		if (bytes == 0)
		{
			printf("client disconnected: %d", sender_fd);
		}
		else
		{
			perror("recv");
		}
		close(sender_fd);
		// Remove an fd from the poll_fds array
		deletePfds(pfds, i, fd_count);
	}
	else
	{
		for (int j = 0; j < *fd_count; j++)
		{
			dest_fd = (*pfds)[j].fd;
			if (dest_fd != sockfd && dest_fd != sender_fd)
			{
				if (send(dest_fd, buf, bytes, 0) == -1)
				{
					perror("send");
				}
			}
		}
	}
}

int createListenerSocket(int port)
{ 
	//creating server socket
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1)
	{
		perror("socket failed");
		exit (EXIT_FAILURE);
	}
	
	const int enable = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) == -1)
		perror("setsockopt failed");
	//Make socket non-blocking
	if (fcntl(sockfd, F_SETFL, O_NONBLOCK) == -1)
	{
		perror("fcntl");
	}
	//defining server address
	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	address.sin_addr.s_addr = INADDR_ANY;
	
	//binding server socket
	if (bind(sockfd, (sockaddr *)&address, sizeof(address)) == -1)
	{
		perror("bind failed");
		exit (EXIT_FAILURE);
	}
	
	//listening for connection
	if (listen(sockfd, 5) == -1)
	{
		perror("listen failed");
		exit(EXIT_FAILURE);
	}
	return (sockfd);
}

int main()
{
	//create socket and non-blocking
	int serverFd = createListenerSocket(8080);
	if ( serverFd == -1)
	{
		perror("error create listener socker");
	}
	
	//init/set up pollfd
	int fd_size = 5;
	int fd_count = 0;
	struct pollfd *pfds = (struct pollfd*)malloc(sizeof(*pfds) * fd_size);
	
	pfds[0].fd = serverFd;//monitor server socket
	pfds[0].events = POLLIN;// check ready to read
	//main loop
	while (true)
	{
		int status = poll(pfds, fd_count, 2000);
		if (status == -1)
		{
			perror("listen failed");
			std::cout << "error" << std::endl;
		}
		//loop throught array of socket
		for (int i = 0; i < fd_count; i++)
		{
			if ((pfds[i].revents & POLLIN))
			{
				std::cout << "socket not ready to read" << std::endl;
				continue;
			}
			std::cout << pfds[i].fd << " ready for I/O operation" << std::endl;
			if (pfds[i].fd == serverFd)//socket is our listening server socket
			{
				std::cout << "accept new connection" << std::endl;
				//accepting a client connection
				newConnection(serverFd, &pfds, &fd_count, &fd_size);
			}
			else
			{
				readData(serverFd, &fd_count, &pfds, i);
			}
		}
		std::cout << "------------------waiting for new connection-------------" << std::endl;
	}
	//closing the socket
	close (serverFd);
}