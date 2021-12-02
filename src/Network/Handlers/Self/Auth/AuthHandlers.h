#pragma once
#include <memory>

class NetPacketHandler;
struct NetPacket;
class NetClient;
namespace InternalSocket
{
    class AuthHandlers
    {
    public:
        static void Setup(NetPacketHandler*);
        static bool HandshakeHandler(std::shared_ptr<NetClient>, std::shared_ptr<NetPacket>);
        static bool HandshakeResponseHandler(std::shared_ptr<NetClient>, std::shared_ptr<NetPacket>);
    };
}