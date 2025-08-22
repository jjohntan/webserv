#include "HTTPResponse.hpp"

int main()
{
	// Simulate a raw status line and HTTP body
	std::string statusLine = "Status: 200 OK\r\n";
	std::string body = "Content-Type: text/plain\r\nContent-Length: 13\r\nConnection: keep-alive\r\n\r\nHello, World!";

	// Fake socketFD for testing logic (no send needed)
	int fakeSocketFD = 123;

	// Construct HTTPResponse object
	HTTPResponse response(statusLine, body, fakeSocketFD);

	// Process the response (status parsing and formatting)
	response.processHTTPResponse();

	// Output verification
	std::cout << CYAN << "=== HTTPResponse Internal State ===" << RESET << std::endl;
	std::cout << YELLOW << "Status Line:        " << RESET << response.getStatusLine();
	std::cout << YELLOW << "Formatted Status:   " << RESET << response.getRawResponse().substr(0, response.getRawResponse().find("\r\n") + 2);
	std::cout << YELLOW << "Status Code:        " << RESET << response.getStatusCode() << std::endl;
	std::cout << YELLOW << "Status Message:     " << RESET << response.getStatusMessage() << std::endl;
	std::cout << YELLOW << "Socket FD:          " << RESET << response.getSocketFD() << std::endl;

	std::cout << YELLOW << "Raw HTTP Response:  " << RESET << std::endl;
	std::cout << response.getRawResponse() << std::endl;

	return 0;
}
