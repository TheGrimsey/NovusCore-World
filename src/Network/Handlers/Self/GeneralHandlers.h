#pragma once
#include <memory>

class NetPacketHandler;
class NetClient;
struct NetPacket;
namespace InternalSocket
{
    class GeneralHandlers
    {
    public:
        static void Setup(NetPacketHandler*);
        static bool HandleConnected(std::shared_ptr<NetClient>, std::shared_ptr<NetPacket>);
        static bool HandleSendAddress(std::shared_ptr<NetClient>, std::shared_ptr<NetPacket>);
    };
}