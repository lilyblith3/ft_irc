#include "Client.hpp"
#include <unistd.h>

Client::Client(int socketFd) : fd(socketFd), closing(false)
{
}

Client::~Client()
{
    if (fd != -1)
        close(fd);
}

int Client::getFd() const
{
    return fd;
}

void Client::appendBuffer(const char *data, int size)
{
    inBuffer.append(data, size);
}

std::string &Client::getBuffer()
{
    return inBuffer;
}

void Client::addOutput(const std::string &data)
{
    outBuffer += data;
}

std::string &Client::getOutput()
{
    return outBuffer;
}

bool Client::hasOutput() const
{
    return !outBuffer.empty();
}

void Client::closeAfterFlush()
{
    closing = true;
}

bool Client::isClosing() const
{
    return closing;
}
