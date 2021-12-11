#pragma once
#include <memory>

class NetPacketHandler;
class NetClient;
struct NetPacket;
namespace Client
{
    class GeneralHandlers
    {
    public:
        static void Setup(NetPacketHandler*);
        static bool HandleConnected(std::shared_ptr<NetClient>, std::shared_ptr<NetPacket>);
        static bool HandleMoveEntity(std::shared_ptr<NetClient>, std::shared_ptr<NetPacket>);
        static bool HandleStoreLoc(std::shared_ptr<NetClient>, std::shared_ptr<NetPacket>);
        static bool HandleGoto(std::shared_ptr<NetClient>, std::shared_ptr<NetPacket>);
    };
}