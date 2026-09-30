#include "Bot.hpp"
#include <sstream>

#include <fstream>
#include <cstdint>

Bot::Bot(int port, const std::string &ip, const std::string &password)
    : fd(-1), port(port), ip(ip), password(password), nickname(BOT_NICK), channel(BOT_CHANNEL)
{
}

Bot::~Bot()
{
  if (fd != -1)
  {
    close(fd);
  }
}

void Bot::connectTooServer()
{
  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof(hints));

  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  std::stringstream ss;
  ss << port;
  std::string portStr = ss.str();

  if (getaddrinfo(ip.c_str(), portStr.c_str(), &hints, &res) != 0)
  {
    std::cerr << "I could NOT figure out how to reach that IP + port" << std::endl;
    exit(1);
  }

  fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

  if (fd == -1)
  {
    std::cerr << "Error creating socket" << std::endl;
    freeaddrinfo(res);
    exit(1);
  }

  if (connect(fd, res->ai_addr, res->ai_addrlen) == -1)
  {
    std::cerr << "Error connecting to server" << std::endl;
    freeaddrinfo(res);
    close(fd);
    exit(1);
  }
  freeaddrinfo(res);
  std::cout << "Connected to server" << ip << ":" << port << std::endl;
}

void Bot::sendMessage(const std::string &msg)
{
  std::string full = msg + "\r\n";
  if (send(fd, full.c_str(), full.size(), 0) == -1)
  {
    std::cerr << "Error : could not send message" << std::endl;
  }
}

void Bot::authenticate()
{
  sendMessage("PASS " + password);
  sendMessage("NICK " + nickname);
  sendMessage("USER " + nickname + " 0 * :IRC Bot");
}

void Bot::joinChannel()
{
  sendMessage("JOIN " + channel);
  std::cout << "Joined channel " << channel << std::endl;
}

std::string Bot::extractNickname(const std::string &msg)
{
  if (msg.empty() || msg[0] != ':')
    return "";
  size_t exclamation = msg.find("!");
  if (exclamation == std::string::npos)
    return "";
  return msg.substr(1, exclamation - 1);
}

void Bot::handleMessages(const std::string &msg)
{
  std::cout << "Server: " << msg << std::endl;

  if (msg.find("PING") != std::string::npos)
  {
    std::string pong = "PONG " + msg.substr(msg.find("PING") + 5);
    sendMessage(pong);
    std::cout << "Sent PONG" << std::endl;
    return;
  }
  if (msg.find("PRIVMSG") != std::string::npos && msg.find(channel) != std::string::npos)
  {
    std::string nick = extractNickname(msg);
    size_t colonPos = msg.rfind(':');
    if (colonPos == std::string::npos)
      return;
    std::string text = msg.substr(colonPos + 1);

    if (text.find("!hello") != std::string::npos)
      sendMessage("PRIVMSG " + channel + " :Hello " + nick + "!");
    else if (text.find("!help") != std::string::npos)
      sendMessage("PRIVMSG " + channel + " :Commands: !hello, !help, !time");
    else if (text.find("!time") != std::string::npos)
    {
      time_t now = time(0);
      std::string t = ctime(&now);
      t.erase(t.size() - 1);
      sendMessage("PRIVMSG " + channel + " :Current time: " + t);
    }
    else
      sendMessage("PRIVMSG " + channel + " :Unknown command. Type !help for commands.");
  }
}

void Bot::run()
{
  char buffer[BUFFER_SIZE];

  connectTooServer();
  authenticate();
  joinChannel();

  while (true)
  {
    memset(buffer, 0, sizeof(buffer));
    ssize_t bytes = recv(fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0)
    {
      std::cout << "Disconnected from server" << std::endl;
      return;
    }
    std::string msg(buffer, bytes);
    handleMessages(msg);
  }
}
