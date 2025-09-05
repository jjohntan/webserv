CXX = g++
CXXFLAGS = -std=c++98 -Wall -Wextra -Werror -g

# Target executable
WEBSERVER = webserver

# Source files
SOURCES = main.cpp \
          server.cpp \
          config_files/config.cpp \
          cgi_handler/cgi.cpp \
          http/HTTP.cpp \
          http/HTTPRequest/HTTPRequest.cpp \
          http/HTTPResponse/HTTPResponse.cpp

# Object files
OBJECTS = main.o \
          server.o \
          config.o \
          cgi.o \
          HTTP.o \
          HTTPRequest.o \
          HTTPResponse.o

# Header files
HEADERS = server.hpp \
          config_files/config.hpp \
          cgi_handler/cgi.hpp \
          http/HTTPRequest/HTTPRequest.hpp \
          http/HTTPResponse/HTTPResponse.hpp \
          http/HTTP.hpp

# Default target
all: $(WEBSERVER)

# Build the main executable
$(WEBSERVER): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(WEBSERVER) $(OBJECTS)
	@echo "Web server build complete! Executable: $(WEBSERVER)"

# Object file dependencies
main.o: main.cpp server.hpp config_files/config.hpp
	$(CXX) $(CXXFLAGS) -c main.cpp -o main.o

server.o: server.cpp server.hpp cgi_handler/cgi.hpp http/HTTPRequest/HTTPRequest.hpp http/HTTPResponse/HTTPResponse.hpp config_files/config.hpp http/HTTP.hpp
	$(CXX) $(CXXFLAGS) -c server.cpp -o server.o

config.o: config_files/config.cpp config_files/config.hpp
	$(CXX) $(CXXFLAGS) -c config_files/config.cpp -o config.o

cgi.o: cgi_handler/cgi.cpp cgi_handler/cgi.hpp http/HTTPRequest/HTTPRequest.hpp http/HTTPResponse/HTTPResponse.hpp
	$(CXX) $(CXXFLAGS) -c cgi_handler/cgi.cpp -o cgi.o


HTTP.o: http/HTTP.cpp http/HTTP.hpp http/HTTPRequest/HTTPRequest.hpp http/HTTPResponse/HTTPResponse.hpp cgi_handler/cgi.hpp
	$(CXX) $(CXXFLAGS) -c http/HTTP.cpp -o HTTP.o

HTTPRequest.o: http/HTTPRequest/HTTPRequest.cpp http/HTTPRequest/HTTPRequest.hpp
	$(CXX) $(CXXFLAGS) -c http/HTTPRequest/HTTPRequest.cpp -o HTTPRequest.o

HTTPResponse.o: http/HTTPResponse/HTTPResponse.cpp http/HTTPResponse/HTTPResponse.hpp
	$(CXX) $(CXXFLAGS) -c http/HTTPResponse/HTTPResponse.cpp -o HTTPResponse.o

# Clean targets
clean:
	rm -f $(OBJECTS)
	@echo "Cleaned object files"

fclean: clean
	rm -f $(WEBSERVER)
	@echo "Full clean complete - removed all compiled files"

re: fclean all
	@echo "Re-compilation complete!"

# Test CGI scripts
test-cgi:
	@echo "Testing CGI scripts..."
	@chmod +x cgi_bin/python/*.py
	@echo "CGI scripts are ready for testing"

# Run the server with default config
run: $(WEBSERVER)
	@echo "Starting webserver with test config..."
	./$(WEBSERVER) testconfig/test.conf

# Debug build
debug: CXXFLAGS += -DDEBUG -g3
debug: $(WEBSERVER)

.PHONY: all clean fclean re test-cgi run debug
