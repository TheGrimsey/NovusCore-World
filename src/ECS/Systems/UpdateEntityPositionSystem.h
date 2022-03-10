#pragma once
#include <NovusTypes.h>
#include <entity/fwd.hpp>

class UpdateEntityPositionSystem
{
public:
    static void Update(entt::registry& registry);
    
    static constexpr f32 SyncDistance = 500.f;
};