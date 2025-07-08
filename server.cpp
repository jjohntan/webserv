#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <iostream>

int main()
{
	//creating server socket
	//AF_INET: IPv4 protocol
	//SOCK_STREAM: TCP type socket
	int socketFd = socket(AF_INET, SOCK_STREAM, 0);
	
	//defining server address
	//htons: convert unsigned int from machine byte to network byte order;
	//INADDY_ANY: 
	sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_port = htons(8080);
	address.sin_addr.s_addr = INADDR_ANY;
	
	//binding server socket
	bind(socketFd, (sockaddr *)&address, sizeof(address));
	
	//listening for connection
	listen(socketFd, 5);
	
	//accepting a client connection
	int clientSocket= accept(socketFd, nullptr, nullptr);
	
	//receiving data from the client
	char buffer[1024] = {0};
	recv(clientSocket, buffer, sizeof(buffer), 0);
	std::cout << "Message from client: " << buffer << std::endl;

	//closing the socket
	close (socketFd);
}