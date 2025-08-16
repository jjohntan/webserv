#include "Server.hpp"
#include "Client.hpp"

int main()
{
	Server server;
	
	try
	{
		server.createListener();
		server.init();
		server.run();
	}
	catch(const std::exception& e)
	{
		std::cerr << e.what() << '\n';
	}
	
}