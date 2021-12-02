#pragma once
#include <NovusTypes.h>
#include <Utils/ConcurrentQueue.h>
#include <Networking/NetPacket.h>
#include <Networking/NetClient.h>

struct ConnectionSingleton
{
    ConnectionSingleton() : packetQueue(256) { }

    std::shared_ptr<NetClient> netClient;
    bool didHandleDisconnect = false;
    moodycamel::ConcurrentQueue<std::shared_ptr<NetPacket>> packetQueue;
};