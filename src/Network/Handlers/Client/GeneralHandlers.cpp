#include "GeneralHandlers.h"
#include <Networking/NetStructures.h>
#include <Networking/NetPacket.h>
#include <Networking/NetClient.h>
#include <Networking/NetPacketHandler.h>
#include <Networking/PacketUtils.h>
#include <Utils/StringUtils.h>

#include <Gameplay/ECS/Components/Transform.h>
#include "../../../ECS/Components/Network/ConnectionSingleton.h"
#include "../../../ECS/Components/Singletons/DBSingleton.h"
#include "../../../ECS/Components/Singletons/MapSingleton.h"
#include "../../../ECS/Components/Singletons/SpawnPlayerQueueSingleton.h"
#include "../../../ECS/Components/Singletons/TeleportSingleton.h"

#include "../../../Gameplay/Map/Map.h"
#include "../../../Utils/ServiceLocator.h"
#include "../../../ECS/Components/Network/ConnectionComponent.h"
#include <Gameplay/Network/PacketWriter.h>

namespace Client
{
    void GeneralHandlers::Setup(NetPacketHandler* netPacketHandler)
    {
        netPacketHandler->SetMessageHandler(Opcode::CMSG_CONNECTED, { ConnectionStatus::AUTH_SUCCESS, 0, GeneralHandlers::HandleConnected });
        netPacketHandler->SetMessageHandler(Opcode::MSG_MOVE_ENTITY, { ConnectionStatus::CONNECTED, sizeof(vec3) * 3, GeneralHandlers::HandleMoveEntity });

        netPacketHandler->SetMessageHandler(Opcode::CMSG_STORELOC, { ConnectionStatus::CONNECTED, 1, 257, GeneralHandlers::HandleStoreLoc });
        netPacketHandler->SetMessageHandler(Opcode::CMSG_GOTO, { ConnectionStatus::CONNECTED, 1, 257, GeneralHandlers::HandleGoto });
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

    bool GeneralHandlers::HandleStoreLoc(std::shared_ptr<NetClient> netClient, std::shared_ptr<NetPacket> packet)
    {
        std::string name = "";
        if (!packet->payload->GetString(name) || name.length() == 0 || name.length() > 256)
            return false;

        entt::registry* registry = ServiceLocator::GetRegistry();
        DBSingleton& dbSingleton = registry->ctx<DBSingleton>();
        TeleportSingleton& teleportSingleton = registry->ctx<TeleportSingleton>();

        name = dbSingleton.auth.EscapeSQL(name);

        std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<512>();
        buffer->Put(Opcode::SMSG_STORELOC);

        u32 nameHash = StringUtils::fnv1a_32(name.c_str(), name.length());

        auto itr = teleportSingleton.nameHashToLocation.find(nameHash);
        if (itr == teleportSingleton.nameHashToLocation.end())
        {
            // Success
            MapSingleton& mapSingleton = registry->ctx<MapSingleton>();

            Transform& transform = registry->get<Transform>(netClient->GetEntity());
            TeleportLocation teleportLocation;
            {
                teleportLocation.name = name;
                teleportLocation.mapId = mapSingleton.GetCurrentMap().id;
                teleportLocation.position = transform.position;
                teleportLocation.orientation = glm::radians(transform.rotation.z);
            }

            size_t size = sizeof(u8) + (name.length() + 1u) + sizeof(vec3) + sizeof(f32);
            buffer->PutU16(static_cast<u16>(size));
            buffer->PutU8(1);
            buffer->PutString(teleportLocation.name);

            buffer->Put(teleportLocation.position);
            buffer->PutF32(teleportLocation.orientation);

            teleportSingleton.nameHashToLocation[nameHash] = teleportLocation;

            std::stringstream ss;
            ss << "INSERT INTO `teleportlocations` (`name`, `mapId`, `positionX`, `positionY`, `positionZ`, `orientation`) VALUES  ('" << name << "', " << teleportLocation.mapId << ", " << teleportLocation.position.x << ", " << teleportLocation.position.y << ", " << teleportLocation.position.z << ", " << teleportLocation.orientation << ");";
            
            dbSingleton.auth.Execute(ss.str());
        }
        else
        {
            // Error

            size_t size = sizeof(u8) + (name.length() + 1u);
            buffer->PutU16(static_cast<u16>(size));
            buffer->PutU8(0);
            buffer->PutString(name);
        }

        netClient->Send(buffer);
        return true;
    }
    bool GeneralHandlers::HandleGoto(std::shared_ptr<NetClient> netClient, std::shared_ptr<NetPacket> packet)
    {
        std::string name = "";
        if (!packet->payload->GetString(name) || name.length() == 0 || name.length() > 256)
            return false;

        entt::registry* registry = ServiceLocator::GetRegistry();
        DBSingleton& dbSingleton = registry->ctx<DBSingleton>();
        TeleportSingleton& teleportSingleton = registry->ctx<TeleportSingleton>();

        name = dbSingleton.auth.EscapeSQL(name);
        u32 nameHash = StringUtils::fnv1a_32(name.c_str(), name.length());

        auto itr = teleportSingleton.nameHashToLocation.find(nameHash);
        if (itr != teleportSingleton.nameHashToLocation.end())
        {
            const TeleportLocation& teleportLocation = itr->second;

            entt::entity entity = netClient->GetEntity();
            Transform& transform = registry->get<Transform>(entity);
            transform.position = teleportLocation.position;
            transform.rotation.z = glm::degrees(teleportLocation.orientation);

            registry->emplace_or_replace<TransformIsDirty>(entity);

            std::shared_ptr<Bytebuffer> packetBuffer = nullptr;
            if (PacketWriter::SMSG_UPDATE_ENTITY(packetBuffer, entity, transform))
            {
                netClient->Send(packetBuffer);
            }
        }

        return true;
    }
}