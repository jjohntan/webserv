CXX = g++
CXXFLAGS = -std=c++98 -Wall -Wextra -Werror 

WEBSERVER = webserver

# files
SOURCES = Server.cpp config_files/config.cpp main.cpp cgi_handler/cgi.cpp http/HTTPRequest.cpp
OBJECTS = Server.o config.o main.o cgi.o HTTPRequest.o
HEADERS = Server.hpp config_files/config.hpp cgi_handler/cgi.hpp http/HTTPRequest.hpp

all: $(WEBSERVER)


$(WEBSERVER): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(WEBSERVER) $(OBJECTS)
	@echo "Web server build complete! Executable: $(WEBSERVER)"

server.o: Server.cpp Server.hpp cgi_handler/cgi.hpp http/HTTPRequest.hpp config_files/config.hpp
	$(CXX) $(CXXFLAGS) -c Server.cpp -o server.o

config.o: config_files/config.cpp config_files/config.hpp
	$(CXX) $(CXXFLAGS) -c config_files/config.cpp -o config.o

main.o: main.cpp Server.hpp config_files/config.hpp
	$(CXX) $(CXXFLAGS) -c main.cpp -o main.o

cgi.o: cgi_handler/cgi.cpp cgi_handler/cgi.hpp http/HTTPRequest.hpp
	$(CXX) $(CXXFLAGS) -c cgi_handler/cgi.cpp -o cgi.o

HTTPRequest.o: http/HTTPRequest.cpp http/HTTPRequest.hpp
	$(CXX) $(CXXFLAGS) -c http/HTTPRequest.cpp -o HTTPRequest.o

clean:
	rm -f $(OBJECTS)
	@echo "Cleaned object files"

fclean: clean
	rm -f $(WEBSERVER)
	@echo "Full clean complete - removed all compiled files"

re: fclean all
	@echo "Re-compilation complete!"


.PHONY: all clean fclean re