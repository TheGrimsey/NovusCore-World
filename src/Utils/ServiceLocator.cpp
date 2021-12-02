#include "ServiceLocator.h"
#include <Networking/NetPacketHandler.h>

entt::registry* ServiceLocator::_gameRegistry = nullptr;
NetPacketHandler* ServiceLocator::_selfNetPacketHandler = nullptr;
NetPacketHandler* ServiceLocator::_clientNetPacketHandler = nullptr;

void ServiceLocator::SetRegistry(entt::registry* registry)
{
    assert(_gameRegistry == nullptr);
    _gameRegistry = registry;
}
void ServiceLocator::SetSelfNetPacketHandler(NetPacketHandler* netPacketHandler)
{
    assert(_selfNetPacketHandler == nullptr);
    _selfNetPacketHandler = netPacketHandler;
}
void ServiceLocator::SetClientNetPacketHandler(NetPacketHandler* netPacketHandler)
{
    assert(_clientNetPacketHandler == nullptr);
    _clientNetPacketHandler = netPacketHandler;
}