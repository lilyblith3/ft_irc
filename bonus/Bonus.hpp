#ifndef BOT_HPP
#define BOT_HPP

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <cstdlib>

#define BOT_NICK "ircbot"
#define BOT_USER "ircbot"
#define BOT_CHANNEL "#botchan"
#define BUFFER_SIZE 512

class Bot
{
private:
    int fd;
    int port;
    std::string ip;
    std::string password;
    std::string nickname;
    std::string channel;

public:
    Bot(int port, const std::string &ip, const std::string &password);
    ~Bot();

    void connectTooServer();
    void authenticate();
    void joinChannel();
    void sendMessage(const std::string &msg);
    void handleMessages(const std::string &msg);
    std::string extractNickname(const std::string &msg);
    void run();

    void handleDCC(const std::string &nick, const std::string &ctcp);
    void receiveFile(const std::string &ip, int port, const std::string &filename, size_t filesize);
};

#endif