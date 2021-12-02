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


void UpdateEntityPositionSystem::Update(entt::registry& registry)
{
    auto modelView = registry.view<Transform, GameEntity>();
    if (modelView.size_hint() == 0)
        return;

    MapSingleton& mapSingleton = registry.ctx<MapSingleton>();
    modelView.each([&](const auto entity, Transform& transform, GameEntity& gameEntity)
    {
        if (gameEntity.type != GameEntity::Type::Player)
            return;

        std::vector<Point2D> playersWithinDistance;
        Tree2D& entityTress = mapSingleton.GetPlayerTree();
        if (!entityTress.GetWithinDistance({transform.position.x, transform.position.y}, 30.f, entity, playersWithinDistance))
            return;

        std::vector<entt::entity>& seenEntities = gameEntity.seenEntities;
        if (seenEntities.size() == 0 && playersWithinDistance.size() == 0)
            return;

        std::vector<entt::entity> newlySeenEntities(playersWithinDistance.size());
        for (u32 i = 0; i < playersWithinDistance.size(); i++)
        {
            newlySeenEntities[i] = playersWithinDistance[i].GetPayload();
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

        // Send Movement Updates to seenEntities
        if (seenEntities.size() > 0 && registry.all_of<TransformIsDirty>(entity))
        {
            std::shared_ptr<Bytebuffer> packetBuffer = nullptr;
            if (PacketWriter::SMSG_UPDATE_ENTITY(packetBuffer, entity, transform))
            {
                for (u32 i = 0; i < seenEntities.size(); i++)
                {
                    entt::entity& seenEntity = seenEntities[i];

                    if (!registry.valid(seenEntity))
                        continue;

                    const GameEntity& seenGameEntity = registry.get<GameEntity>(seenEntity);
                    if (seenGameEntity.type != GameEntity::Type::Player)
                        continue;

                    ConnectionComponent& seenConnection = registry.get<ConnectionComponent>(seenEntity);
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
}