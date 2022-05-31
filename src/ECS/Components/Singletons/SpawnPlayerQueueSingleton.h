#pragma once
#include <NovusTypes.h>
#include <Utils/ConcurrentQueue.h>

class NetClient;

struct SpawnPlayerRequest
{
    std::shared_ptr<NetClient> client;
};

struct SpawnPlayerQueueSingleton
{
    moodycamel::ConcurrentQueue<SpawnPlayerRequest> spawnPlayerRequests;
};