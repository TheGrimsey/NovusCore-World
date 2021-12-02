#include "ConnectionSystems.h"
#include <entt.hpp>
#include <Networking/NetClient.h>
#include <Networking/NetServer.h>
#include <Networking/NetPacketHandler.h>
#include <Utils/DebugHandler.h>
#include "../../../Utils/ServiceLocator.h"
#include "../../Components/Singletons/TimeSingleton.h"
#include "../../Components/Network/ConnectionSingleton.h"
#include "../../Components/Network/AuthenticationSingleton.h"
#include "../../Components/Network/ConnectionComponent.h"
#include "../../Components/Network/ConnectionDeferredSingleton.h"
#include "../../Components/Network/Authentication.h"
#include "../../Components/Singletons/MapSingleton.h"
#include "../../../Gameplay/Map/Map.h"
#include <Gameplay/ECS/Components/Transform.h>

#include <tracy/Tracy.hpp>

void ConnectionUpdateSystem::Update(entt::registry& registry)
{
    ZoneScopedNC("ConnectionUpdateSystem::Update", tracy::Color::Blue);

    ConnectionSingleton& connectionSingleton = registry.ctx<ConnectionSingleton>();
    if (connectionSingleton.netClient)
    {
        if (connectionSingleton.netClient->Read())
        {
            Self_HandleRead(connectionSingleton.netClient);
        }

        if (!connectionSingleton.netClient->IsConnected())
        {
            if (!connectionSingleton.didHandleDisconnect)
            {
                connectionSingleton.didHandleDisconnect = true;

                Self_HandleDisconnect(connectionSingleton.netClient);
            }
        }
        else
        {
            NetPacketHandler* selfNetPacketHandler = ServiceLocator::GetSelfNetPacketHandler();

            std::shared_ptr<NetPacket> packet = nullptr;
            while (connectionSingleton.packetQueue.try_dequeue(packet))
            {
#ifdef NC_Debug
                DebugHandler::PrintSuccess("[Network/ClientSocket]: CMD: %u, Size: %u", packet->header.opcode, packet->header.size);
#endif // NC_Debug

                if (!selfNetPacketHandler->CallHandler(connectionSingleton.netClient, packet))
                {
                    connectionSingleton.netClient->Close();
                    return;
                }
            }
        }
    }

    TimeSingleton& timeSingleton = registry.ctx<TimeSingleton>();
    f32 deltaTime = timeSingleton.deltaTime;

    NetPacketHandler* clientNetPacketHandler = ServiceLocator::GetClientNetPacketHandler();

    auto view = registry.view<ConnectionComponent>();
    view.each([&registry, &clientNetPacketHandler, &deltaTime](const auto, ConnectionComponent& connection)
    {
        if (connection.netClient->Read())
        {
            Client_HandleRead(connection.netClient);
        }

        if (!connection.netClient->IsConnected())
        {
            Client_HandleDisconnect(connection.netClient);
            return;
        }

        std::shared_ptr<NetPacket> packet = nullptr;
        while (connection.packetQueue.try_dequeue(packet))
        {
#ifdef NC_Debug
           DebugHandler::PrintSuccess("[Network/ServerSocket]: CMD: %u, Size: %u", packet->header.opcode, packet->header.size);
#endif // NC_Debug

            if (!clientNetPacketHandler->CallHandler(connection.netClient, packet))
            {
                connection.netClient->Close();
                return;
            }

            if (connection.lowPriorityBuffer->writtenData)
            {
                connection.lowPriorityTimer += deltaTime;
                if (connection.lowPriorityTimer >= LOW_PRIORITY_TIME)
                {
                    connection.lowPriorityTimer = 0;
                    connection.netClient->Send(connection.lowPriorityBuffer);
                    connection.lowPriorityBuffer->Reset();
                }
            }

            if (connection.mediumPriorityBuffer->writtenData)
            {
                connection.mediumPriorityTimer += deltaTime;
                if (connection.mediumPriorityTimer >= MEDIUM_PRIORITY_TIME)
                {
                    connection.mediumPriorityTimer = 0;
                    connection.netClient->Send(connection.mediumPriorityBuffer);
                    connection.mediumPriorityBuffer->Reset();
                }
            }

            if (connection.highPriorityBuffer->writtenData)
            {
                connection.netClient->Send(connection.highPriorityBuffer);
                connection.highPriorityBuffer->Reset();
            }
        }
    });
}

bool ConnectionUpdateSystem::Server_HandleConnect(std::shared_ptr<NetClient> netClient)
{
#ifdef NC_Debug
    const NetSocket::ConnectionInfo& connectionInfo = netClient->GetSocket()->GetConnectionInfo();
    DebugHandler::PrintSuccess("[Network/Socket]: Client connected from (%s, %u)", connectionInfo.ipAddrStr.c_str(), connectionInfo.port);
#endif // NC_Debug

    std::shared_ptr<NetSocket> socket = netClient->GetSocket();
    socket->SetBlockingState(false);
    socket->SetSendBufferSize(8192);
    socket->SetReceiveBufferSize(8192);
    socket->SetNoDelayState(true);

    entt::registry* registry = ServiceLocator::GetRegistry();
    auto& connectionDeferredSingleton = registry->ctx<ConnectionDeferredSingleton>();
    connectionDeferredSingleton.newConnectionQueue.enqueue(netClient);

    return true;
}

void ConnectionUpdateSystem::Client_HandleRead(std::shared_ptr<NetClient> netClient)
{
    std::shared_ptr<Bytebuffer> buffer = netClient->GetReadBuffer();
    entt::registry* registry = ServiceLocator::GetRegistry();

    const entt::entity& entity = netClient->GetEntity();
    ConnectionComponent& connectionComponent = registry->get<ConnectionComponent>(entity);

    while (size_t activeSize = buffer->GetActiveSize())
    {
        // We have received a partial header and need to read more
        if (activeSize < sizeof(PacketHeader))
        {
            buffer->Normalize();
            break;
        }

        PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer->GetReadPointer());

        if (header->opcode == Opcode::INVALID || header->opcode > Opcode::MAX_COUNT)
        {
#ifdef NC_Debug
            DebugHandler::PrintError("Received Invalid Opcode (%u) from network stream", static_cast<u16>(header->opcode));
#endif // NC_Debug
            break;
        }

        if (header->size > 8192)
        {
#ifdef NC_Debug
            DebugHandler::PrintError("Received Invalid Opcode Size (%u) from network stream", header->size);
#endif // NC_Debug
            break;
        }
        size_t sizeWithoutHeader = activeSize - sizeof(PacketHeader);

        // We have received a valid header, but we have yet to receive the entire payload
        if (sizeWithoutHeader < header->size)
        {
            buffer->Normalize();
            break;
        }

        // Skip Header
        buffer->SkipRead(sizeof(PacketHeader));

        std::shared_ptr<NetPacket> packet = NetPacket::Borrow();
        {
            // Header
            {
                packet->header = *header;
            }

            // Payload
            {
                if (packet->header.size)
                {
                    packet->payload = Bytebuffer::Borrow<8192/*NETWORK_BUFFER_SIZE*/ >();
                    packet->payload->size = packet->header.size;
                    packet->payload->writtenData = packet->header.size;
                    std::memcpy(packet->payload->GetDataPointer(), buffer->GetReadPointer(), packet->header.size);

                    // Skip Payload
                    buffer->SkipRead(header->size);
                }
            }

            connectionComponent.packetQueue.enqueue(packet);
        }
    }

    // Only reset if we read everything that was written
    if (buffer->GetActiveSize() == 0)
    {
        buffer->Reset();
    }
}

void ConnectionUpdateSystem::Client_HandleDisconnect(std::shared_ptr<NetClient> netClient)
{
#ifdef NC_Debug
    const NetSocket::ConnectionInfo& connectionInfo = netClient->GetSocket()->GetConnectionInfo();
    DebugHandler::PrintWarning("[Network/Socket]: Client disconnected from (%s, %u)", connectionInfo.ipAddrStr.c_str(), connectionInfo.port);
#endif // NC_Debug

    entt::registry* registry = ServiceLocator::GetRegistry();
    auto& connectionDeferredSingleton = registry->ctx<ConnectionDeferredSingleton>();

    entt::entity entity = netClient->GetEntity();
    connectionDeferredSingleton.droppedConnectionQueue.enqueue(entity);
}

void ConnectionUpdateSystem::Self_HandleConnect(std::shared_ptr<NetClient> netClient, bool connected)
{
    if (connected)
    {
#ifdef NC_Debug
        const NetSocket::ConnectionInfo& connectionInfo = netClient->GetSocket()->GetConnectionInfo();
        DebugHandler::PrintSuccess("[Network/Socket]: Successfully connected to (%s, %u)", connectionInfo.ipAddrStr.c_str(), connectionInfo.port);
#endif // NC_Debug

        entt::registry* registry = ServiceLocator::GetRegistry();
        AuthenticationSingleton& authentication = registry->ctx<AuthenticationSingleton>();
        ConnectionSingleton& connectionSingleton = registry->ctx<ConnectionSingleton>();

        /* Send Initial Packet */
        std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<512>();

        authentication.srp.username = "world";
        authentication.srp.password = "password";
        connectionSingleton.didHandleDisconnect = false;

        // If StartAuthentication fails, it means A failed to generate and thus we cannot connect
        if (!authentication.srp.StartAuthentication())
            return;

        buffer->Put(Opcode::CMSG_LOGON_CHALLENGE);
        buffer->SkipWrite(sizeof(u16));

        u16 size = static_cast<u16>(buffer->writtenData);
        buffer->PutString(authentication.srp.username);
        buffer->PutBytes(authentication.srp.aBuffer->GetDataPointer(), authentication.srp.aBuffer->size);

        u16 writtenData = static_cast<u16>(buffer->writtenData) - size;

        buffer->Put<u16>(writtenData, 2);
        netClient->Send(buffer);

        netClient->SetConnectionStatus(ConnectionStatus::AUTH_CHALLENGE);
    }
    else
    {
#ifdef NC_Debug
        const NetSocket::ConnectionInfo& connectionInfo = netClient->GetSocket()->GetConnectionInfo();
        DebugHandler::PrintWarning("[Network/Socket]: Failed to connect to (%s, %u)", connectionInfo.ipAddrStr.c_str(), connectionInfo.port);
#endif // NC_Debug
    }
}

void ConnectionUpdateSystem::Self_HandleRead(std::shared_ptr<NetClient> netClient)
{
    entt::registry* registry = ServiceLocator::GetRegistry();
    ConnectionSingleton& connectionSingleton = registry->ctx<ConnectionSingleton>();

    std::shared_ptr<Bytebuffer> buffer = netClient->GetReadBuffer();

    while (size_t activeSize = buffer->GetActiveSize())
    {
        // We have received a partial header and need to read more
        if (activeSize < sizeof(PacketHeader))
        {
            buffer->Normalize();
            break;
        }

        PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer->GetReadPointer());

        if (header->opcode == Opcode::INVALID || header->opcode > Opcode::MAX_COUNT)
        {
#ifdef NC_Debug
            DebugHandler::PrintError("Received Invalid Opcode (%u) from network stream", static_cast<u16>(header->opcode));
#endif // NC_Debug
            break;
        }

        if (header->size > 8192)
        {
#ifdef NC_Debug
            DebugHandler::PrintError("Received Invalid Opcode Size (%u) from network stream", header->size);
#endif // NC_Debug
            break;
        }

        size_t sizeWithoutHeader = activeSize - sizeof(PacketHeader);

        // We have received a valid header, but we have yet to receive the entire payload
        if (sizeWithoutHeader < header->size)
        {
            buffer->Normalize();
            break;
        }

        // Skip Header
        buffer->SkipRead(sizeof(PacketHeader));

        std::shared_ptr<NetPacket> packet = NetPacket::Borrow();
        {
            // Header
            {
                packet->header = *header;
            }

            // Payload
            {
                if (packet->header.size)
                {
                    packet->payload = Bytebuffer::Borrow<8192/*NETWORK_BUFFER_SIZE*/ >();
                    packet->payload->size = packet->header.size;
                    packet->payload->writtenData = packet->header.size;
                    std::memcpy(packet->payload->GetDataPointer(), buffer->GetReadPointer(), packet->header.size);

                    // Skip Payload
                    buffer->SkipRead(header->size);
                }
            }

            connectionSingleton.packetQueue.enqueue(packet);
        }
    }

    // Only reset if we read everything that was written
    if (buffer->GetActiveSize() == 0)
    {
        buffer->Reset();
    }
}

void ConnectionUpdateSystem::Self_HandleDisconnect(std::shared_ptr<NetClient> netClient)
{
#ifdef NC_Debug
    const NetSocket::ConnectionInfo& connectionInfo = netClient->GetSocket()->GetConnectionInfo();
    DebugHandler::PrintWarning("[Network/Socket]: Disconnected from (%s, %u)", connectionInfo.ipAddrStr.c_str(), connectionInfo.port);
#endif // NC_Debug
}

void ConnectionDeferredSystem::Update(entt::registry& registry)
{
    ConnectionDeferredSingleton& connectionDeferredSingleton = registry.ctx<ConnectionDeferredSingleton>();

    if (connectionDeferredSingleton.newConnectionQueue.size_approx() > 0)
    {
        std::shared_ptr<NetClient> netClient;
        while (connectionDeferredSingleton.newConnectionQueue.try_dequeue(netClient))
        {
            entt::entity entity = registry.create();

            ConnectionComponent& connectionComponent = registry.emplace<ConnectionComponent>(entity);
            registry.emplace<Authentication>(entity);

            connectionComponent.netClient = netClient;

            connectionComponent.netClient->SetEntity(entity);
            connectionComponent.netClient->SetConnectionStatus(ConnectionStatus::AUTH_CHALLENGE);
        }
    }

    if (connectionDeferredSingleton.droppedConnectionQueue.size_approx() > 0)
    {
        entt::entity entity;
        while (connectionDeferredSingleton.droppedConnectionQueue.try_dequeue(entity))
        {
            registry.destroy(entity);
        }
    }
}