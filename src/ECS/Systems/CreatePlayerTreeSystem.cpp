#include "CreatePlayerTreeSystem.h"
#include <entt.hpp>
#include <tracy/Tracy.hpp>

#include "../../Utils/ServiceLocator.h"
#include "../Components/Singletons/MapSingleton.h"

#include "../Components/Network/ConnectionComponent.h"
#include <Gameplay/ECS/Components/Transform.h>
#include <Gameplay/ECS/Components/GameEntity.h>

void CreatePlayerTreeSystem::Update(entt::registry& registry)
{
    auto modelView = registry.view<Transform, GameEntity>();
    size_t numEntitiesInView = modelView.size_hint();

    if (numEntitiesInView == 0)
        return;

    MapSingleton& mapSingleton = registry.ctx<MapSingleton>();

    std::vector<Point2D> points;
    points.reserve(numEntitiesInView);

    modelView.each([&](const auto entity, Transform& transform, GameEntity& gameEntity)
    {
        points.push_back(Point2D({ transform.position.x, transform.position.y }, entity));
    });

    Tree2D& playerTree = mapSingleton.GetPlayerTree();
    playerTree = Tree2D(points.begin(), points.end());
}