#pragma once
#include <NovusTypes.h>
#include <Utils/ConcurrentQueue.h>
#include <Networking/NetClient.h>
#include <Networking/NetServer.h>

struct ConnectionDeferredSingleton
{
    ConnectionDeferredSingleton() : newConnectionQueue(64), droppedConnectionQueue(32) { }

    std::shared_ptr<NetServer> netServer;
    moodycamel::ConcurrentQueue<std::shared_ptr<NetClient>> newConnectionQueue;
    moodycamel::ConcurrentQueue<entt::entity> droppedConnectionQueue;
};