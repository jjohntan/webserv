/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   client.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jetan <jetan@student.42kl.edu.my>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/09 14:07:45 by jetan             #+#    #+#             */
/*   Updated: 2025/07/15 20:18:32 by jetan            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <unistd.h>

int createClient(int port)
{
	//creating client socket
	//AF_INET: IPv4 protocol
	//SOCK_STREAM: TCP type socket
	//0: default protocol
	int clientFd = socket(AF_INET, SOCK_STREAM, 0);
	
	return (clientFd);
}

int main()
{
	int port = 8080;
	int clientFd = createClient(port);
	//defining client address
	//sin_family: specifies the address family
	//sin_port: specifies the port number and must be used with htons()
	//htons: convert unsigned int from machine byte to network byte order;
	//sin_addr: holds the IP address returned by inet_addr() to be used in 
	//the socket connection
	sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_port = htons(8080);
	address.sin_addr.s_addr = INADDR_ANY;
	
	//connecting to the server
	connect(clientFd, (sockaddr *)&address, sizeof(address));
	
	//sending data to the server
	const char *message = "Hello, server!";
	send(clientFd, message, std::strlen(message), 0);
	
	//closing socket
	close(clientFd);
}