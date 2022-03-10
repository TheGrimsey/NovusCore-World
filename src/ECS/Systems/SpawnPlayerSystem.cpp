#include "SpawnPlayerSystem.h"
#include <entt.hpp>
#include <Networking/NetClient.h>
#include <Networking/NetServer.h>
#include <Networking/NetPacketHandler.h>
#include <Utils/SafeVector.h>
#include <tracy/Tracy.hpp>

#include "../../Utils/ServiceLocator.h"

#include "../Components/Singletons/MapSingleton.h"
#include "../Components/Singletons/SpawnPlayerQueueSingleton.h"
#include "../Components/Network/ConnectionComponent.h"

#include <Gameplay/Network/PacketWriter.h>
#include <Gameplay/ECS/Components/Transform.h>
#include <Gameplay/ECS/Components/GameEntity.h>
#include <Gameplay/ECS/Components/GameEntityPlayerFlag.h>
#include <Gameplay/ECS/Components/EntityResources.h>

void SpawnPlayerSystem::Update(entt::registry& registry)
{
    ZoneScopedNC("SpawnPlayerSystem::Update", tracy::Color::Blue);

    MapSingleton& mapSingleton = registry.ctx<MapSingleton>();
    Terrain::Map& currentMap = mapSingleton.GetCurrentMap();

    SpawnPlayerQueueSingleton& spawnPlayerQueueSingleton = registry.ctx_or_set<SpawnPlayerQueueSingleton>();

    SpawnPlayerRequest request;
    while (spawnPlayerQueueSingleton.spawnPlayerRequests.try_dequeue(request))
    {
        const std::shared_ptr<NetClient> client = request.client;

        // Make sure we discard the request if the client disconnected before we could get to it
        if (!client->IsConnected())
            continue;

        // Create Player & Notify
        {
            const entt::entity entityID = client->GetEntity();
            ConnectionComponent& connection = registry.get<ConnectionComponent>(entityID);

            Transform& transform = registry.emplace<Transform>(entityID);
            GameEntity& gameEntity = registry.emplace<GameEntity>(entityID, GameEntity::Type::Player, 29344);
            registry.emplace<TransformIsDirty>(entityID);
            registry.emplace<GameEntityPlayerFlag>(entityID);
            EntityResources& resources = registry.emplace<EntityResources>(entityID);
            resources.resources[static_cast<u8>(EntityResourceType::HEALTH)] = 123456.f;

            std::shared_ptr<Bytebuffer> packetBuffer = nullptr;
            if (PacketWriter::SMSG_CREATE_PLAYER(packetBuffer, entityID, gameEntity, transform))
            {
                connection.AddPacket(packetBuffer, PacketPriority::IMMEDIATE);
            }

            // TODO Testing
            if (PacketWriter::SMSG_ENTITY_RESOURCES_UPDATE(packetBuffer, entityID, resources))
            {
                connection.AddPacket(packetBuffer, PacketPriority::IMMEDIATE);
            }
        }
    }
}