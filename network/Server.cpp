#include "Server.hpp"
#include "Client.hpp"
#include <new>
#include <cerrno>

static volatile sig_atomic_t g_running = 1;

static void handleSignal(int)
{
    g_running = 0;
}

bool Server::validatePort(const std::string &portStr)
{
    if (portStr.empty())
        return false;
    std::size_t i = 0;
    while (i < portStr.size())
    {
        if (!std::isdigit(static_cast<unsigned char>(portStr[i])))
            return false;
        ++i;
    }
    char *end = NULL;
    long value = std::strtol(portStr.c_str(), &end, 10);
    if (end == NULL || *end != '\0')
        return false;
    if (value <= 0 || value > 65535)
        return false;
    port = static_cast<int>(value);
    return true;
}

Server::Server(const std::string &portStr, const std::string &passwordValue)
    : port(0), password(passwordValue), serverFd(-1)
{
    if (!validatePort(portStr))
    {
        std::cerr << "Error: invalid port: " << portStr << std::endl;
        std::exit(1);
    }
    handler.setServerPassword(password);
}

Server::~Server()
{
    std::size_t i = 0;
    while (i < clients.size())
    {
        delete clients[i];
        ++i;
    }
    clients.clear();
    if (serverFd != -1)
        close(serverFd);
}

bool Server::setNonBlocking(int fd)
{
    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
        return false;
    return true;
}

void Server::createSocket()
{
    serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd == -1)
    {
        std::cerr << "Error: socket() failed" << std::endl;
        std::exit(1);
    }

    int option = 1;
    if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) == -1)
    {
        std::cerr << "Error: setsockopt() failed" << std::endl;
        close(serverFd);
        serverFd = -1;
        std::exit(1);
    }

    if (!setNonBlocking(serverFd))
    {
        std::cerr << "Error: fcntl() failed" << std::endl;
        close(serverFd);
        serverFd = -1;
        std::exit(1);
    }
}

void Server::bindSocket()
{
    sockaddr_in address = sockaddr_in();
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverFd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == -1)
    {
        std::cerr << "Error: bind() failed" << std::endl;
        close(serverFd);
        serverFd = -1;
        std::exit(1);
    }
}

void Server::listenSocket()
{
    if (listen(serverFd, 10) == -1)
    {
        std::cerr << "Error: listen() failed" << std::endl;
        close(serverFd);
        serverFd = -1;
        std::exit(1);
    }
}

void Server::acceptClient()
{
    int clientFd = accept(serverFd, NULL, NULL);
    if (clientFd == -1)
        return;

    if (!setNonBlocking(clientFd))
    {
        close(clientFd);
        return;
    }

    int sndBufSize = 8 * 1024;
    setsockopt(clientFd, SOL_SOCKET, SO_SNDBUF, &sndBufSize, sizeof(sndBufSize));

    Client *client = NULL;
    try
    {
        client = new Client(clientFd);
        clients.push_back(client);
    }
    catch (const std::bad_alloc &)
    {
        if (client != NULL)
            delete client;
        else
            close(clientFd);
        throw;
    }

    struct pollfd pollFd;
    pollFd.fd = clientFd;
    pollFd.events = POLLIN;
    pollFd.revents = 0;
    pollFds.push_back(pollFd);

    handler.clientConnected(clientFd);
    std::cout << "Client connected: " << clientFd << std::endl;
}

int Server::findClientIndexByFd(int fd) const
{
    std::size_t i = 0;
    while (i < clients.size())
    {
        if (clients[i]->getFd() == fd)
            return static_cast<int>(i);
        ++i;
    }
    return -1;
}

void Server::removeClient(int index)
{
    if (index < 0 || index >= static_cast<int>(clients.size()))
        return;
    int fd = clients[index]->getFd();
    std::cout << "Client disconnected: " << fd << std::endl;
    handler.clientDisconnected(fd);
    drainResponses();
    delete clients[index];
    clients.erase(clients.begin() + index);
    pollFds.erase(pollFds.begin() + index + 1);
}

void Server::closeClientGracefully(int index)
{
    if (index < 0 || index >= static_cast<int>(clients.size()))
        return;
    if (clients[index]->hasOutput())
    {
        clients[index]->closeAfterFlush();
        pollFds[index + 1].events |= POLLOUT;
    }
    else
        removeClient(index);
}

void Server::receiveData(int index)
{
    if (index < 0 || index >= static_cast<int>(clients.size()))
        return;

    char buffer[16384];
    int fd = clients[index]->getFd();
    int bytesRead = recv(fd, buffer, sizeof(buffer), 0);

    if (bytesRead == 0)
    {
        closeClientGracefully(index);
        return;
    }
    if (bytesRead < 0)
        return;

    clients[index]->appendBuffer(buffer, bytesRead);
    processClientData(index);
}

static std::size_t findRescuableCommand(const std::string &raw, std::size_t maxLine)
{
    static const char *commands[] = {
        "PASS", "NICK", "USER", "JOIN", "PRIVMSG", "NOTICE", "MODE",
        "TOPIC", "INVITE", "KICK", "PART", "QUIT", "PING", "PONG"
    };
    const std::size_t commandCount = sizeof(commands) / sizeof(commands[0]);

    std::size_t start = 0;
    while (start < raw.size())
    {
        if (raw.size() - start > maxLine)
        {
            ++start;
            continue;
        }
        std::size_t c = 0;
        while (c < commandCount)
        {
            std::string word = commands[c];
            if (start + word.size() > raw.size())
            {
                ++c;
                continue;
            }
            bool match = true;
            std::size_t k = 0;
            while (k < word.size())
            {
                unsigned char a = static_cast<unsigned char>(raw[start + k]);
                if (std::toupper(a) != word[k])
                {
                    match = false;
                    break;
                }
                ++k;
            }
            if (!match)
            {
                ++c;
                continue;
            }
            if (start + word.size() < raw.size())
            {
                char after = raw[start + word.size()];
                if (after != ' ' && after != '\r' && after != '\n' && after != ':')
                {
                    ++c;
                    continue;
                }
            }
            return start;
        }
        ++start;
    }
    return std::string::npos;
}

void Server::processClientData(int index)
{
    if (index < 0 || index >= static_cast<int>(clients.size()))
        return;

    int fd = clients[index]->getFd();
    std::string &buffer = clients[index]->getBuffer();
    const std::size_t MAX_LINE = 512;
    std::size_t pos;

    while ((pos = buffer.find('\n')) != std::string::npos)
    {
        std::string line = buffer.substr(0, pos + 1);
        buffer.erase(0, pos + 1);

        if (line.size() > MAX_LINE)
        {
            std::size_t rescuePos = findRescuableCommand(line, MAX_LINE);
            if (rescuePos == std::string::npos)
                continue;
            line = line.substr(rescuePos);
        }
        if (!line.empty() && line[line.size() - 1] == '\n')
            line.erase(line.size() - 1);
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        handler.processLine(fd, line);
        drainResponses();

        if (index >= static_cast<int>(clients.size()) ||
            clients[index]->getFd() != fd)
            return;

        if (clients[index]->isClosing())
            return;
    }

    if (buffer.size() > MAX_LINE)
        buffer.clear();
}

static const std::size_t MAX_SENDQ = 256 * 1024;

void Server::drainResponses()
{
    CommandHandler::DrainEffects eff = handler.drain();

    std::map<int, std::string>::iterator it = eff.replies.begin();
    while (it != eff.replies.end())
    {
        int fd = it->first;
        const std::string &data = it->second;
        if (!data.empty())
        {
            int index = findClientIndexByFd(fd);
            if (index != -1)
            {
                clients[index]->addOutput(data);
                if (clients[index]->getOutput().size() > MAX_SENDQ)
                {
                    std::cout << "Client " << fd
                              << " exceeded output buffer limit, disconnecting"
                              << std::endl;
                    removeClient(index);
                }
                else
                    pollFds[index + 1].events |= POLLOUT;
            }
        }
        ++it;
    }

    std::size_t i = 0;
    while (i < eff.disconnects.size())
    {
        int fd = eff.disconnects[i];
        int index = findClientIndexByFd(fd);
        if (index != -1)
            closeClientGracefully(index);
        ++i;
    }
}

void Server::sendData(int index)
{
    if (index < 0 || index >= static_cast<int>(clients.size()))
        return;

    std::string &output = clients[index]->getOutput();
    if (output.empty())
    {
        pollFds[index + 1].events &= ~POLLOUT;
        if (clients[index]->isClosing())
            removeClient(index);
        return;
    }

    int fd = clients[index]->getFd();
    int bytesSent = send(fd, output.c_str(), output.size(), 0);
    if (bytesSent <= 0)
        return;

    output.erase(0, static_cast<std::size_t>(bytesSent));
    if (output.empty())
    {
        pollFds[index + 1].events &= ~POLLOUT;
        if (clients[index]->isClosing())
            removeClient(index);
    }
}

void Server::run()
{
    g_running = 1;

    std::signal(SIGINT, handleSignal);
    std::signal(SIGQUIT, handleSignal);
    std::signal(SIGPIPE, SIG_IGN);

    createSocket();
    bindSocket();
    listenSocket();

    struct pollfd serverPollFd;
    serverPollFd.fd = serverFd;
    serverPollFd.events = POLLIN;
    serverPollFd.revents = 0;
    pollFds.push_back(serverPollFd);

    std::cout << "Server listening on port " << port << std::endl;

    while (g_running)
    {
        try
        {
            int result = poll(&pollFds[0], pollFds.size(), -1);
            if (result == -1)
            {
                if (errno == EINTR)
                    continue;
                if (g_running)
                    std::cerr << "Error: poll() failed" << std::endl;
                break;
            }

            if (pollFds[0].revents & (POLLERR | POLLHUP | POLLNVAL))
            {
                std::cerr << "Error: server socket error" << std::endl;
                break;
            }
            if (pollFds[0].revents & POLLIN)
                acceptClient();

            int i = 0;
            while (i < static_cast<int>(clients.size()))
            {
                short revents = pollFds[i + 1].revents;
                if (revents == 0)
                {
                    ++i;
                    continue;
                }

                if ((revents & POLLIN) && !clients[i]->isClosing())
                {
                    int fdBefore = clients[i]->getFd();
                    receiveData(i);
                    if (i >= static_cast<int>(clients.size()) ||
                        clients[i]->getFd() != fdBefore)
                        continue;
                }

                if (i < static_cast<int>(clients.size()) &&
                    (revents & (POLLHUP | POLLERR | POLLNVAL)))
                {
                    removeClient(i);
                    continue;
                }

                if (i < static_cast<int>(clients.size()) &&
                    (pollFds[i + 1].revents & POLLOUT))
                {
                    int fdBefore = clients[i]->getFd();
                    sendData(i);
                    if (i >= static_cast<int>(clients.size()) ||
                        clients[i]->getFd() != fdBefore)
                        continue;
                }
                ++i;
            }
        }
        catch (const std::bad_alloc &)
        {
            std::size_t i = 0;
            while (i < clients.size())
            {
                delete clients[i];
                ++i;
            }
            clients.clear();
            pollFds.resize(1);
            pollFds[0].revents = 0;
            handler.resetSessions();
            std::cerr << "Error: allocation failed; client sessions cleared" << std::endl;
        }
    }

    std::size_t i = 0;
    while (i < clients.size())
    {
        delete clients[i];
        ++i;
    }
    clients.clear();
    pollFds.clear();
    if (serverFd != -1)
    {
        close(serverFd);
        serverFd = -1;
    }
}
