#include "CommandHandler.hpp"

void CommandHandler::handleQUIT(int fd, const Command &cmd)
{
    User *user = getUser(fd);
    std::string reason;

    if (cmd.hasTrailing)
        reason = cmd.message;
    else if (!cmd.params.empty())
    {
        reason = cmd.params[0];
        for (std::size_t i = 1; i < cmd.params.size(); ++i)
            reason += " " + cmd.params[i];
    }

    // If user not found, just mark for disconnect
    if (user == NULL)
    {
        _pendingDisconnect.insert(fd);
        return;
    }

    std::string quitMsg = IRC::ResponseBuilder::clientMessage(
        user->getFullIdentifier(), "QUIT", "", reason);

    // Collect unique recipients across all channels user is in — walk
    // channel registry (single source of truth) instead of per-user set.
    std::set<int> recipients;
    for (std::map<std::string, Channel>::iterator it = channels.begin();
         it != channels.end(); ++it)
    {
        Channel &ch = it->second;
        if (!ch.hasMember(fd))
            continue;
        const std::set<int> &members = ch.getMembers();
        for (std::set<int>::const_iterator m = members.begin(); m != members.end(); ++m)
        {
            if (*m != fd)
                recipients.insert(*m);
        }
    }

    for (std::set<int>::const_iterator it = recipients.begin(); it != recipients.end(); ++it)
        sendToClient(*it, quitMsg);

    // Flush pending output (or at least ERROR) before closing: queue ERROR
    // to the quitter so drain will deliver it before disconnect. This also
    // ensures any earlier queued reply (e.g. PONG from a pipelined PING+QUIT)
    // is not discarded — drain now keeps replies for pendingDisconnect fds.
    {
        std::string closingReason = reason.empty() ? "Client Quit" : reason;
        sendToClient(fd, "ERROR :Closing Link: " + user->getNickname() + " (" + closingReason + ")\r\n");
    }

    // Single membership call — same path as unexpected disconnect.
    removeFromAllChannels(fd);
    _pendingDisconnect.insert(fd);
}
