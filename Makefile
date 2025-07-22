CXX = g++
CXXFLAGS = -std=c++98 -Wall -Wextra -Werror 

WEBSERVER = webserver

# files
SOURCES = server.cpp config_files/config.cpp main.cpp
OBJECTS = server.o config.o main.o
HEADERS = server.hpp config_files/config.hpp

all: $(WEBSERVER)


$(WEBSERVER): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(WEBSERVER) $(OBJECTS)
	@echo "Web server build complete! Executable: $(WEBSERVER)"

server.o: server.cpp server.hpp
	$(CXX) $(CXXFLAGS) -c server.cpp -o server.o

config.o: config_files/config.cpp config_files/config.hpp
	$(CXX) $(CXXFLAGS) -c config_files/config.cpp -o config.o

main.o: main.cpp server.hpp config_files/config.hpp
	$(CXX) $(CXXFLAGS) -c main.cpp -o main.o


clean:
	rm -f $(OBJECTS)
	@echo "Cleaned object files"

fclean: clean
	rm -f $(WEBSERVER)
	@echo "Full clean complete - removed all compiled files"

re: fclean all
	@echo "Re-compilation complete!"


.PHONY: all clean fclean re