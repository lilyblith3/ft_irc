
CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -Iinclude -Inetwork -Icore -Icommands -Iparsing -Ibonus



NAME = ircserv
BONUS_NAME = ircbot



SRCS = main.cpp \
       network/Client.cpp \
       network/Server.cpp \
       core/CommandHandler.cpp \
       core/CommandState.cpp \
       parsing/CommandParser.cpp \
       core/Response.cpp \
       core/ChannelMode.cpp \
       commands/Authentication.cpp \
       commands/Invite.cpp \
       commands/Join.cpp \
       commands/Kick.cpp \
       commands/Mode.cpp \
       commands/Part.cpp \
       commands/Ping.cpp \
       commands/Privmsg.cpp \
       commands/Quit.cpp \
       commands/Topic.cpp

OBJS = $(SRCS:.cpp=.o)



BONUS_SRCS = Bot.cpp
BONUS_OBJS = $(BONUS_SRCS:.cpp=.o)



all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)



bonus: $(BONUS_NAME)

$(BONUS_NAME): $(BONUS_OBJS)
	$(CXX) $(CXXFLAGS) $(BONUS_OBJS) -o $(BONUS_NAME)


%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@



clean:
	rm -f $(OBJS) $(BONUS_OBJS)

fclean: clean
	rm -f $(NAME) $(BONUS_NAME)

re: fclean all

.PHONY: all bonus clean fclean re