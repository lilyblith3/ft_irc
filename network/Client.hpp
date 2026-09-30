#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>

class Client
{
private:
    int         fd;
    std::string inBuffer;
    std::string outBuffer;
    bool        closing;

public:
    Client(int socketFd);
    ~Client();

    int getFd() const;

    void         appendBuffer(const char *data, int size);
    std::string &getBuffer();

    void         addOutput(const std::string &data);
    std::string &getOutput();
    bool         hasOutput() const;

    void closeAfterFlush();
    bool isClosing() const;
};

#endif
