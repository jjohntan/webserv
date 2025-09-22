// Use the same helpers the router uses
#include "HTTP.hpp"
#include "http_cgi.hpp"
#include "../Server.hpp"
#include <poll.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>      // close
#include <sys/socket.h>  // recv
#include <sstream>
#include "../Server.hpp"

const ServerConfig* getActiveServer(const std::vector<ServerConfig>& serverConfig)
{
	if (serverConfig.empty())
		return (NULL);
	return (&serverConfig[0]);
}

const Location* getMatchingLocation(const std::string& path,
									const std::vector<ServerConfig>& serverConfig)
{
	return (getMatchingLocation(path, getActiveServer(serverConfig)));
}

const	Location* getMatchingLocation(const std::string &path, const ServerConfig* servercConfig)
{
	if (!servercConfig)
		return (NULL);
	const	Location* best = NULL;
	size_t	best_len = 0;

	// Keep comparing until get a best matched location
	for (size_t	i = 0; i < servercConfig->locations.size(); ++i)
	{
		const Location &current = servercConfig->locations[i];
		if (path.find(current.path) == 0 && current.path.length() > best_len)
		{
			best = &current;
			best_len = current.path.length();
		}
	}
	return (best);
}

bool	methodAllowed(const HTTPRequest &request, const Location *Location)
{
	// Default "Allow" if not configured
	if (!Location || Location->allowed_methods.empty())
		return (true);
	// [ADD] RFC: If GET is allowed, treat HEAD as allowed too.
	// We check this first so HEAD won’t be rejected when only GET is listed.
	if (request.getMethod() == "HEAD")
	{
		for (size_t i = 0; i < Location->allowed_methods.size(); ++i)
		{
			if (Location->allowed_methods[i] == "GET") 
				return (true);
		}
	}
	for (size_t	i = 0; i < Location->allowed_methods.size(); ++i)
	{
		if (request.getMethod() == Location->allowed_methods[i])
			return (true);
	}
	return (false);
}

bool	checkAllowedMethod(const HTTPRequest &request, int socketFD, const std::vector<ServerConfig> &serverConfig, Server& srv) // [CHANGE]
{
	const ServerConfig *active = findServerConfig(request, serverConfig);
	if (!active) 
		return (false);

	std::string	path = request.getPath();
	const Location *matching_location = getMatchingLocation(path, active);
	if (!matching_location)
		return (false);

	// 405 Method Not Allowed (with Allow:)
	if (matching_location && !methodAllowed(request, matching_location))
	{
		std::ostringstream allow;
		for (size_t i = 0; i < matching_location->allowed_methods.size(); ++i)
		{
			if (i)
				allow << ", ";
			allow << matching_location->allowed_methods[i];
		}
		// RFC requires Allow header for 405
		std::string extra = "Allow: " + allow.str() + "\r\n";
		extra += request.connectionHeader(request.isConnectionAlive());

		ErrorResponse resp(405, "Method Not Allowed", *active, extra, socketFD);              // [CHANGE]
		srv.queueResponse(socketFD, resp.getRawResponse()); 
		return (true);
	}
	return (false);
}

/*
	413 Payload Too Large (client_max_body_size enforcement)
*/
bool	checkPayLoad(const HTTPRequest &request, int socketFD, const std::vector<ServerConfig> &serverConfig, Server& srv) // [CHANGE]
{
	const ServerConfig *active = findServerConfig(request, serverConfig);
	if (!active)
		return (false);
	if (active->client_max_body_size > 0)
	{
		const std::vector<char>&body = request.getBodyVector();
		if (body.size() > active->client_max_body_size)
		{
			// Provide Connection header matching keep-alive decision
			std::string extra = request.connectionHeader(request.isConnectionAlive());
			ErrorResponse resp(413, "Payload Too Large", *active, extra, socketFD);           // [CHANGE]
			srv.queueResponse(socketFD, resp.getRawResponse());                               // [ADD]
			return (true);
		}
	}
	return (false);
}

const char* reasonPhrase(int code)
{
	switch (code)
	{
		case 301: 
			return ("Moved Permanently");
		case 302: 
			return ("Found");
		case 303: 
			return ("See Other");
		case 307: 
			return ("Temporary Redirect");
		case 308:
			return ("Permanent Redirect");
		default:
			return ("Redirect");
	}
}

/*
	That block sends a 3xx redirection response when a <location> in your config says to redirect.
	Location
		redirect_code (301, 302, 303, 307, 308)
		redirect_url (where to send the client)
*/
bool	checkRedirectResponse(const HTTPRequest &request, int socketFD, const std::vector<ServerConfig> &serverConfig, Server& srv) // [CHANGE]
{
	const ServerConfig* active = findServerConfig(request, serverConfig);
	if (!active)
		return (false);
	std::string	path = request.getPath();
	const Location* matching_location = getMatchingLocation(path, active);
	if (!matching_location)
		return (false);

	if (matching_location && matching_location->redirect_code > 0 && !matching_location->redirect_url.empty())
	{
		const int code = matching_location->redirect_code;
		const std::string &url = matching_location->redirect_url;
		const char *reason = reasonPhrase(code);
		bool include_body = true;
		if (request.getMethod() == "HEAD")
			include_body = false;
		std::string body;
		if (include_body)
		{
			body  = "<!DOCTYPE html><html><head><meta charset=\"utf-8\"/>";
			body += "<title>"; body += reason; body += "</title></head><body>";
			body += "<h1>"; body += reason; body += "</h1>";
			body += "<p><a href=\""; body += url; body += "\">";
			body += url; body += "</a></p></body></html>";
		}

		std::ostringstream out;
		out << "Location: " << url << "\r\n"
			<< "Content-Type: text/html\r\n"
			<< request.connectionHeader(request.isConnectionAlive())
			<< "\r\n";
		if (include_body)
			out << body;

		HTTPResponse resp(reason, code, out.str(), socketFD);                                          // [CHANGE]
		srv.queueResponse(socketFD, resp.getRawResponse()); 
		return (true);
	}
	return (false);
}

void	printRequest(const HTTPRequest &request)
{
	std::cout << GREEN << "\n--- Request Line ---\n" << RESET;
	std::cout << "Method: " << request.getMethod() << std::endl;
	std::cout << "Path: " << request.getPath() << std::endl;
	std::cout << "Version: " << request.getVersion() << std::endl;

	std::cout << GREEN << "\n--- Headers ---\n" << RESET;
	const std::map<std::string, std::string>& headers = request.getHeaderMap();
	for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it)
		std::cout << it->first << ": " << it->second << std::endl;

	std::cout << GREEN << "\n--- Body ---\n" << RESET;
	std::cout << request.getRawBody() << std::endl;
}

void	readClientData(int socketFD, std::map<int, HTTPRequest>& requestMap, std::vector<struct pollfd>& fds, size_t &i, const std::vector<ServerConfig>& servers, Server& srv) // [CHANGE]
{
	char	buffer[READ_BYTES] = {0};
	ssize_t	read_bytes = recv(socketFD, buffer, READ_BYTES, 0);

	if (read_bytes <= 0)
	{
		// Remove client socket from poll set and the map
		close(socketFD);
		requestMap.erase(socketFD);
		fds.erase(fds.begin() + i);
		--i; // Decrement to not skip element after erase
		return ;
	}
	std::string	data(buffer, read_bytes);
	bool	isClearing = false;
	isClearing = processClientData(socketFD, requestMap, data, servers, srv); // [CHANGE]
	if (isClearing == true)
	{
		// Remove client socket from poll set and the map
		close(socketFD);
		requestMap.erase(socketFD);
		fds.erase(fds.begin() + i);
		--i; // Decrement to not skip element after erase
	}
	
}

/*
	HTTP/1.1 pipelining	
	client sends multiple requests back-to-back on the same TCP connection without waiting for the previous response	
*/

bool	processClientData(int socketFD, std::map<int, HTTPRequest>& requestMap, std::string data, const std::vector<ServerConfig>& servers, Server& srv) // [CHANGE]
{
	try
	{
		HTTPRequest& req = requestMap[socketFD];
		req.feed(data);

		// Process as many pipelined requests as are fully buffered
		while (req.isHeaderComplete() && req.isBodyComplete())
		{
			std::cout << "Request From Socket " << socketFD << " had successfully converted into object!\n";
			printRequest(req);

			if (checkAllowedMethod(req, socketFD, servers, srv) ||
				checkPayLoad(req, socketFD, servers, srv) ||
				checkRedirectResponse(req, socketFD, servers, srv))
			{
				bool closeIt = !req.isConnectionAlive();
				if (closeIt)
					srv.markCloseAfterWrite(socketFD);      // [ADD] close after queued bytes flush

				// keep remainder (if any), then reset and re-feed it
				size_t used = req.endOfMessageOffset();
				std::string tail = req.getRawString().substr(used); // get the content of next pipeline request
				req.resetForNextRequest();
				if (!tail.empty())
				{
					req.feed(tail);
					continue; // loop for next buffered request
				}
				return (false); // do not force-close here
			}

			// normal response path
			handleRequestProcessing(req, socketFD, servers, srv); // [CHANGE] queues internally
			bool closeIt = !req.isConnectionAlive();
				if (closeIt)
					srv.markCloseAfterWrite(socketFD);      // [ADD] close after queued bytes flush

			// keep remainder (if any), then reset and re-feed it
			size_t used = req.endOfMessageOffset();
			std::string tail = req.getRawString().substr(used);
			req.resetForNextRequest();
			if (!tail.empty())
			{
				req.feed(tail);
				continue; // process next pipelined request
			}
			return (false);
		}
		return (false); // need more bytes for next request
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error while processing client data from socket "
				<< socketFD << ": " << e.what() << "\n";

		// Try to derive the right server config from what we know about this fd
		const ServerConfig* active = NULL;

		// If we still have a request object for this fd, try to use its Host header
		std::map<int, HTTPRequest>::const_iterator it = requestMap.find(socketFD);
		if (it != requestMap.end())
		{
			// findServerConfig is declared in http_cgi.hpp (which you already include)
			active = findServerConfig(it->second, servers);
		}

		// Fallback: if nothing resolved, use the first configured server
		if (!active && !servers.empty())
			active = &servers[0];

		if (active)
		{
			ErrorResponse err(400, "Bad Request", *active, socketFD);
			srv.queueResponse(socketFD, err.getRawResponse());
			srv.markCloseAfterWrite(socketFD);   // [ADD] close after sending the error
		}
		else
		{
			// Last-resort literal response (no config available)
			const std::string body =
				"<!DOCTYPE html><html><head><meta charset=\"utf-8\"/>"
				"<title>400 Bad Request</title></head><body>"
				"<h1>400 Bad Request</h1></body></html>";
			std::ostringstream out;
			out << "Content-Type: text/html\r\n"
				<< "Connection: close\r\n\r\n"
				<< body;
			HTTPResponse err("Bad Request", 400, out.str(), socketFD);
			srv.queueResponse(socketFD, err.getRawResponse());
			srv.markCloseAfterWrite(socketFD);   // [ADD]
		}

		return (false); // let POLLOUT flush then close
	}
}
