#include "CreatureMovementSystem.h"
#include <entt.hpp>
#include <tracy/Tracy.hpp>

#include <Utils/DebugHandler.h>
#include "../../Utils/ServiceLocator.h"
#include "../Components/Singletons/MapSingleton.h"
#include "../Components/Network/ConnectionComponent.h"

#include <Gameplay/Network/PacketWriter.h>
#include <Gameplay/ECS/Components/Transform.h>
#include <Gameplay/ECS/Components/Transform.h>
#include <Gameplay/ECS/Components/GameEntity.h>
#include <Gameplay/ECS/Components/GameEntityPlayerFlag.h>

/*
*   This whole thing exists just to test.
*/
void CreatureMovementSystem::Update(entt::registry& registry)
{
    auto entityView = registry.view<Transform, GameEntity>(entt::exclude_t<GameEntityPlayerFlag>());
    if (entityView.size_hint() == 0)
        return;

    //entityView.each([&](const auto entity, Transform& transform, GameEntity& gameEntity)
    //{
    //    transform.position.z += 0.05f; // This is not right.
    //});
    //registry.insert<TransformIsDirty>(entityView.begin(), entityView.end());
}