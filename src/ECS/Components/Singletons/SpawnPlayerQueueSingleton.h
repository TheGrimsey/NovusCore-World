#pragma once
#include <NovusTypes.h>
#include <Networking/NetClient.h>
#include <Utils/ConcurrentQueue.h>

struct SpawnPlayerRequest
{
    std::shared_ptr<NetClient> client;
};

struct SpawnPlayerQueueSingleton
{
    moodycamel::ConcurrentQueue<SpawnPlayerRequest> spawnPlayerRequests;
};