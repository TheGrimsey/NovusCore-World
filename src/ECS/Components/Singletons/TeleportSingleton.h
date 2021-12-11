#pragma once
#include <NovusTypes.h>
#include <vector>
#include <robin_hood.h>

#include "../../../Gameplay/Map/TeleportLocations.h"

struct TeleportSingleton
{
public:
	robin_hood::unordered_map<u32, TeleportLocation> nameHashToLocation;
};