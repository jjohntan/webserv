/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jetan <jetan@student.42kl.edu.my>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/09 14:07:40 by jetan             #+#    #+#             */
/*   Updated: 2025/07/09 15:09:34 by jetan            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <iostream>

int main()
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
	sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_port = htons(8080);
	address.sin_addr.s_addr = INADDR_ANY;
	
	//binding server socket
	bind(serverFd, (sockaddr *)&address, sizeof(address));
	
	//listening for connection
	listen(serverFd, 5);
	
	//accepting a client connection
	int clientSocket = accept(serverFd, NULL, NULL);
	
	//receiving data from the client
	char buffer[1024] = {0};
	recv(clientSocket, buffer, sizeof(buffer), 0);
	std::cout << "Message from client: " << buffer << std::endl;

	//closing the socket
	close (serverFd);
}