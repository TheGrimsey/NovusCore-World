#include "UpdateEntityPositionSystem.h"
#include <entt.hpp>
#include <tracy/Tracy.hpp>

#include <Utils/DebugHandler.h>
#include "../../Utils/ServiceLocator.h"
#include "../Components/Singletons/MapSingleton.h"
#include "../Components/Network/ConnectionComponent.h"

#include <Gameplay/Network/PacketWriter.h>
#include <Gameplay/ECS/Components/Transform.h>
#include <Gameplay/ECS/Components/GameEntity.h>
#include <Gameplay/ECS/Components/GameEntityPlayerFlag.h>

void UpdateEntityPositionSystem::Update(entt::registry& registry)
{
    auto playerView = registry.view<Transform, GameEntity, GameEntityPlayerFlag>();
    if (playerView.size_hint() == 0)
        return;


    MapSingleton& mapSingleton = registry.ctx<MapSingleton>();
    playerView.each([&](const auto entity, Transform& transform, GameEntity& gameEntity)
    {
        std::vector<Point2D> entitiesWithinDistance;
        Tree2D& entityTree = mapSingleton.GetEntityTree();
        if (!entityTree.GetWithinDistance({transform.position.x, transform.position.y}, SyncDistance, entity, entitiesWithinDistance))
            return;

        std::vector<entt::entity>& seenEntities = gameEntity.seenEntities;
        if (seenEntities.size() == 0 && entitiesWithinDistance.size() == 0)
            return;

        std::vector<entt::entity> newlySeenEntities(entitiesWithinDistance.size());
        for (u32 i = 0; i < entitiesWithinDistance.size(); i++)
        {
            newlySeenEntities[i] = entitiesWithinDistance[i].GetPayload();
        }

        ConnectionComponent& connection = registry.get<ConnectionComponent>(entity);

        // Send Delete Updates to no longer seen entites
        {
            for (auto it = seenEntities.begin(); it != seenEntities.end();)
            {
                entt::entity seenEntity = *it;

                auto itr = std::find(newlySeenEntities.begin(), newlySeenEntities.end(), seenEntity);
                if (itr != newlySeenEntities.end())
                {
                    newlySeenEntities.erase(itr);

                    it++;
                    continue;
                }

                // Remove SeenEntity
                {
                    it = seenEntities.erase(it);

                    std::shared_ptr<Bytebuffer> packetBuffer = nullptr;
                    if (PacketWriter::SMSG_DELETE_ENTITY(packetBuffer, seenEntity))
                    {
                        connection.AddPacket(packetBuffer, PacketPriority::IMMEDIATE);
                    }
                    else
                    {
                        DebugHandler::PrintError("Failed to build SMSG_DELETE_ENTITY");
                    }
                }
            }
        }

        // Send our Movement Updates to other players.
        if (seenEntities.size() > 0 && registry.all_of<TransformIsDirty>(entity))
        {
            std::shared_ptr<Bytebuffer> packetBuffer = nullptr;
            if (PacketWriter::SMSG_UPDATE_ENTITY(packetBuffer, entity, transform))
            {
                for (u32 i = 0; i < seenEntities.size(); i++)
                {
                    if (!registry.valid(seenEntities[i]) || !registry.all_of<GameEntityPlayerFlag>(seenEntities[i]))
                        continue;

                    ConnectionComponent& seenConnection = registry.get<ConnectionComponent>(seenEntities[i]);
                    seenConnection.AddPacket(packetBuffer, PacketPriority::IMMEDIATE);
                }
            }
        }

        // Check if there are any new entities
        if (newlySeenEntities.size() == 0)
            return;

        // List of new entities within range

        if (newlySeenEntities.size() > 0)
        {
            //DebugHandler::PrintSuccess("Preparing Data for Player (%u)", entt::to_integral(entity));
            for (u32 i = 0; i < newlySeenEntities.size(); i++)
            {
                entt::entity newEntity = newlySeenEntities[i];

                if (!registry.valid(newEntity))
                    continue;

                const Transform& newTransform = registry.get<Transform>(newEntity);
                const GameEntity& newGameEntity = registry.get<GameEntity>(newEntity);

                std::shared_ptr<Bytebuffer> packetBuffer = nullptr;
                
                if (PacketWriter::SMSG_CREATE_ENTITY(packetBuffer, newEntity, newGameEntity, newTransform))
                {
                    connection.AddPacket(packetBuffer, PacketPriority::IMMEDIATE);
                }

                seenEntities.push_back(newEntity);
            }
            //DebugHandler::PrintSuccess("Finished Preparing Data for Player (%u)", entt::to_integral(entity));
        }
    });

    auto entityView = registry.view<Transform, GameEntity, TransformIsDirty>(entt::exclude_t<GameEntityPlayerFlag>());
    if (entityView.size_hint() == 0)
        return;

    entityView.each([&](const auto entity, Transform& transform, GameEntity& gameEntity)
    {
        std::vector<Point2D> playersWithinDistance;
        Tree2D& playerTree = mapSingleton.GetPlayerTree();
        if (!playerTree.GetWithinDistance({ transform.position.x, transform.position.y }, SyncDistance, entity, playersWithinDistance))
            return;

        // TODO: We should not be sending these to newlySeenEntities as they just got the create packet.
        std::vector<entt::entity>& seenEntities = gameEntity.seenEntities;
        if (seenEntities.size() == 0 && playersWithinDistance.size() == 0)
            return;
        seenEntities.resize(playersWithinDistance.size());

        for (u32 i = 0; i < playersWithinDistance.size(); i++)
        {
            seenEntities[i] = playersWithinDistance[i].GetPayload();
        }

        std::shared_ptr<Bytebuffer> packetBuffer = nullptr;
        if (PacketWriter::SMSG_UPDATE_ENTITY(packetBuffer, entity, transform))
        {
            for (entt::entity seenEntity : seenEntities)
            {
                if (!registry.valid(seenEntity) || !registry.all_of<GameEntityPlayerFlag>(seenEntity))
                    continue;

                ConnectionComponent& seenConnection = registry.get<ConnectionComponent>(seenEntity);
                seenConnection.AddPacket(packetBuffer, PacketPriority::IMMEDIATE);
            }
        }
    });

    // Testing
    registry.clear<TransformIsDirty>();
}