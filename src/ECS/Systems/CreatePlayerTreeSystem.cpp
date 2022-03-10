#include "CreatePlayerTreeSystem.h"
#include <entt.hpp>
#include <tracy/Tracy.hpp>

#include "../../Utils/ServiceLocator.h"
#include "../Components/Singletons/MapSingleton.h"

#include "../Components/Network/ConnectionComponent.h"
#include <Gameplay/ECS/Components/Transform.h>
#include <Gameplay/ECS/Components/GameEntity.h>
#include <Gameplay/ECS/Components/GameEntityPlayerFlag.h>

void CreatePlayerTreeSystem::Update(entt::registry& registry)
{
    auto modelView = registry.view<Transform, GameEntity>();
    size_t numEntitiesInView = modelView.size_hint();

    if (numEntitiesInView == 0)
        return;

    MapSingleton& mapSingleton = registry.ctx<MapSingleton>();

    std::vector<Point2D> points;
    std::vector<Point2D> playerPoints;
    points.reserve(numEntitiesInView);
    playerPoints.reserve(registry.view<GameEntityPlayerFlag>().size());

    modelView.each([&](const auto entity, Transform& transform, GameEntity& gameEntity)
    {
        Point2D point = Point2D({ transform.position.x, transform.position.y }, entity);
        points.push_back(point);

        if (gameEntity.type == GameEntity::Type::Player)
            playerPoints.push_back(point);
    });

    Tree2D& entityTree = mapSingleton.GetEntityTree();
    entityTree = Tree2D(points.begin(), points.end());

    Tree2D& playerTree = mapSingleton.GetPlayerTree();
    playerTree = Tree2D(points.begin(), points.end());
}