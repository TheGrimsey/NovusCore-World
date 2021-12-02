#include "GeneralHandlers.h"
#include <entt.hpp>
#include <Networking/NetStructures.h>
#include <Networking/NetPacket.h>
#include <Networking/NetClient.h>
#include <Networking/NetPacketHandler.h>
#include <Networking/PacketUtils.h>
#include "../../../Utils/ServiceLocator.h"
#include "../../../ECS/Components/Network/ConnectionComponent.h"

namespace InternalSocket
{
    void GeneralHandlers::Setup(NetPacketHandler* netPacketHandler)
    {
        netPacketHandler->SetMessageHandler(Opcode::SMSG_CONNECTED, { ConnectionStatus::AUTH_SUCCESS, 0, GeneralHandlers::HandleConnected });
        netPacketHandler->SetMessageHandler(Opcode::SMSG_SEND_ADDRESS, { ConnectionStatus::CONNECTED, 1, sizeof(u8) + sizeof(u32) + sizeof(u16) + sizeof(entt::entity), GeneralHandlers::HandleSendAddress });
    }

    bool GeneralHandlers::HandleConnected(std::shared_ptr<NetClient> netClient, std::shared_ptr<NetPacket> packet)
    {
        netClient->SetConnectionStatus(ConnectionStatus::CONNECTED);
        return true;
    }
    bool GeneralHandlers::HandleSendAddress(std::shared_ptr<NetClient> netClient, std::shared_ptr<NetPacket> packet)
    {
        u8 status = 0;
        u32 address = 0;
        u16 port = 0;
        entt::entity entity = entt::null;

        if (!packet->payload->GetU8(status))
            return false;

        if (status > 0)
        {
            if (!packet->payload->GetU32(address))
                return false;

            if (!packet->payload->GetU16(port))
                return false;
        }

        if (!packet->payload->Get(entity))
            return false;

        std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<128>();
        if (!PacketUtils::Write_SMSG_SEND_ADDRESS(buffer, status, address, port))
            return false;

        entt::registry* registry = ServiceLocator::GetRegistry();
        auto& connectionComponent = registry->get<ConnectionComponent>(entity);
        connectionComponent.netClient->Send(buffer);
        return true;
    }
}