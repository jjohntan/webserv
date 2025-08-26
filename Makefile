#------Colours------#
DEF_COLOR='\033[0;39m'
RESET = '\033[0;0m'
BLACK='\033[0;30m'
RED='\033[1;91m'
GREEN='\033[1;92m'
YELLOW='\033[0;93m'
BLUE='\033[0;94m'
MAGENTA='\033[0;95m'
CYAN='\033[0;96m'
GRAY='\033[0;90m'
WHITE='\033[0;97m'
PINK  = '\033[35m'

# Program Name
NAME = Webserv

# Compiler
CC = c++
CFLAGS = -Wall -Wextra -Werror -std=c++98 -g3 $(FSAN)
FSAN = -fsanitize=address
RM = rm -f

# Source and Object
SRCS =	main.cpp \
		Server.cpp \
		http/HTTPRequest/HTTPRequest.cpp \
		http/HTTPResponse/HTTPResponse.cpp \
		http/HTTP.cpp \
		config_files/config.cpp \
		cgi_handler/cgi.cpp

OBJ = $(SRCS:.cpp=.o)

# Rules
all: $(NAME)

$(NAME): $(OBJ)
	@ echo $(GREEN)" 🍕 Compiling "$(RED)"[$(NAME)]"$(GREEN)"..."$(RESET)
	@ $(CC) $(CFLAGS) $(OBJ) -o $(NAME)
	@ echo $(RED)" 🍟 [$(NAME)]"$(GREEN)" successfully compiled!"$(RESET)
	@ echo $(GREEN)" 🌭 Your"$(RED)" [$(NAME)] "$(GREEN)"is ready to use"$(RESET)

# $@ = target file
# $< = first dependency
# $^ = all dependencies
%.o: %.cpp
	@ $(CC) $(CFLAGS) -c $< -o $@

clean:
	@ echo $(CYAN)" 🥨 Cleaning Object Files..."$(RESET)
	@ $(RM) $(OBJ)

fclean: clean
	@ echo $(MAGENTA)" 🥯 Removing "$(RED)"[$(NAME)]"$(GREEN)"..."$(RESET)
	@ $(RM) $(NAME)
# $(RM) KL_shrubbery

re : fclean all
