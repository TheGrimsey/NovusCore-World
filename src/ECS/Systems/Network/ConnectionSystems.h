#pragma once
#include <entity/fwd.hpp>

class NetClient;
namespace moddycamel
{
    class ConcurrentQueue;
}
class ConnectionUpdateSystem
{
public:
    static void Update(entt::registry& registry);

    // Handlers for Network Server
    static bool Server_HandleConnect(std::shared_ptr<NetClient> netClient);

    // Handlers for Network Client
    static void Client_HandleRead(std::shared_ptr<NetClient> netClient);
    static void Client_HandleDisconnect(std::shared_ptr<NetClient> netClient);
    static void Self_HandleConnect(std::shared_ptr<NetClient> netClient, bool connected);
    static void Self_HandleRead(std::shared_ptr<NetClient> netClient);
    static void Self_HandleDisconnect(std::shared_ptr<NetClient> netClient);
};

class ConnectionDeferredSystem
{
public:
    static void Update(entt::registry& registry);
};