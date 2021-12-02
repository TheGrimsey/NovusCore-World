#pragma once
#include <memory>

class NetPacketHandler;
class NetClient;
struct NetPacket;
namespace Client
{
    class AuthHandlers
    {
    public:
        static void Setup(NetPacketHandler*);
        static bool HandshakeHandler(std::shared_ptr<NetClient>, std::shared_ptr<NetPacket>);
        static bool HandshakeResponseHandler(std::shared_ptr<NetClient>, std::shared_ptr<NetPacket>);
    };
}