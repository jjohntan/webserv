/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jetan <jetan@student.42kl.edu.my>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/09 14:07:40 by jetan             #+#    #+#             */
/*   Updated: 2025/07/14 18:05:47 by jetan            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <iostream>

int createListenerSocket(int port)
{
	//creating server socket
	//AF_INET: IPv4 protocol
	//SOCK_STREAM: TCP type socket
	//0: default protocol
	int serverFd = socket(AF_INET, SOCK_STREAM, 0);
	//defining server address
	//sin_family: specifies the address family
	//sin_port = specifies the port number and must be used with htons()
	//htons: convert unsigned int from machine byte to network byte order;
	//sin_addr: holds the IP address returned by inet_addr() to be used in 
	//the socket connection
	//INADDY_ANY: 
	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	address.sin_addr.s_addr = INADDR_ANY;
	
	//binding server socket
	bind(serverFd, (sockaddr *)&address, sizeof(address));
	//listening for connection
	listen(serverFd, 5);
	return (serverFd);
}

int main()
{
	int serverFd = createListenerSocket(8080);
	//accepting a client connection
	int clientSocket = accept(serverFd, NULL, NULL);
	
	//receiving data from the client
	char buffer[1024] = {0};
	recv(clientSocket, buffer, sizeof(buffer), 0);
	std::cout << "Message from client: " << buffer << std::endl;
	
	std::string body = "<h1>Hello World, 8080!</h1>";
	
	std::string response = "HTTP/1.1 200 OK\r\n";
	response += "Content-Type: text/html\r\n";
	response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
	response += "Connection: close\r\n";
	response += "\r\n";
	response += body;
	
	send(clientSocket, response.c_str(), response.size(), 0);

	//closing the socket
	close (serverFd);
}