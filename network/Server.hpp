#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include <poll.h>
#include <cstdlib>
#include <cctype>
#include <csignal>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "CommandHandler.hpp"

class Client;

class Server
{
private:
    int                        port;
    std::string                password;
    int                        serverFd;
    std::vector<struct pollfd> pollFds;
    std::vector<Client *>      clients;
    CommandHandler             handler;

    void createSocket();
    void bindSocket();
    void listenSocket();
    bool setNonBlocking(int fd);

    void acceptClient();
    void receiveData(int index);
    void sendData(int index);
    void processClientData(int index);
    void drainResponses();
    void closeClientGracefully(int index);
    void removeClient(int index);

    int  findClientIndexByFd(int fd) const;
    bool validatePort(const std::string &portStr);

public:
    Server(const std::string &port, const std::string &password);
    ~Server();
    void run();
};

#endif
