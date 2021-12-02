#include "GeneralHandlers.h"
#include <Networking/NetStructures.h>
#include <Networking/NetPacket.h>
#include <Networking/NetClient.h>
#include <Networking/NetPacketHandler.h>
#include <Networking/PacketUtils.h>

#include <Gameplay/ECS/Components/Transform.h>
#include "../../../ECS/Components/Network/ConnectionSingleton.h"
#include "../../../ECS/Components/Singletons/MapSingleton.h"
#include "../../../ECS/Components/Singletons/SpawnPlayerQueueSingleton.h"

#include "../../../Gameplay/Map/Map.h"
#include "../../../Utils/ServiceLocator.h"
#include "../../../ECS/Components/Network/ConnectionComponent.h"

namespace Client
{
    void GeneralHandlers::Setup(NetPacketHandler* netPacketHandler)
    {
        netPacketHandler->SetMessageHandler(Opcode::CMSG_CONNECTED, { ConnectionStatus::AUTH_SUCCESS, 0, GeneralHandlers::HandleConnected });
        netPacketHandler->SetMessageHandler(Opcode::MSG_MOVE_ENTITY, { ConnectionStatus::CONNECTED, sizeof(vec3) * 3, GeneralHandlers::HandleMoveEntity });
    }

    bool GeneralHandlers::HandleConnected(std::shared_ptr<NetClient> netClient, std::shared_ptr<NetPacket> packet)
    {
        netClient->SetConnectionStatus(ConnectionStatus::CONNECTED);

        std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<128>();
        buffer->Put(Opcode::SMSG_CONNECTED);
        buffer->PutU16(0);
        netClient->Send(buffer);

        // Add Player Entity (Request to be handled later in this frame or early next frame)
        {
            entt::registry* registry = ServiceLocator::GetRegistry();

            SpawnPlayerQueueSingleton& spawnPlayerQueueSingleton = registry->ctx<SpawnPlayerQueueSingleton>();
            spawnPlayerQueueSingleton.spawnPlayerRequests.enqueue({ netClient });
        }

        return true;
    }

    bool GeneralHandlers::HandleMoveEntity(std::shared_ptr<NetClient> netClient, std::shared_ptr<NetPacket> packet)
    {
        entt::registry* registry = ServiceLocator::GetRegistry();
        const entt::entity& senderEntity = netClient->GetEntity();

        Transform& senderTransform = registry->get<Transform>(senderEntity);
        packet->payload->Deserialize(senderTransform);

        // Validate Input

        // Mark as dirty
        registry->emplace_or_replace<TransformIsDirty>(senderEntity);

        return true;
    }
}