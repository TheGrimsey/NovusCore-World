#include "AuthHandlers.h"
#include <entt.hpp>
#include <Networking/NetStructures.h>
#include <Networking/NetPacket.h>
#include <Networking/NetClient.h>
#include <Networking/NetPacketHandler.h>
#include <Utils/ByteBuffer.h>
#include "../../../../Utils/ServiceLocator.h"
#include "../../../../ECS/Components/Network/AuthenticationSingleton.h"
#include "../../../../ECS/Components/Network/ConnectionDeferredSingleton.h"

// @TODO: Remove Temporary Includes when they're no longer needed
#include <Utils/DebugHandler.h>

namespace InternalSocket
{
    void AuthHandlers::Setup(NetPacketHandler* netPacketHandler)
    {
        netPacketHandler->SetMessageHandler(Opcode::SMSG_LOGON_CHALLENGE, { ConnectionStatus::AUTH_CHALLENGE, sizeof(ServerLogonChallenge), AuthHandlers::HandshakeHandler });
        netPacketHandler->SetMessageHandler(Opcode::SMSG_LOGON_HANDSHAKE, { ConnectionStatus::AUTH_HANDSHAKE, sizeof(ServerLogonHandshake), AuthHandlers::HandshakeResponseHandler });
    }
    bool AuthHandlers::HandshakeHandler(std::shared_ptr<NetClient> netClient, std::shared_ptr<NetPacket> packet)
    {
        ServerLogonChallenge logonChallenge;
        logonChallenge.Deserialize(packet->payload);

        entt::registry* registry = ServiceLocator::GetRegistry();
        AuthenticationSingleton& authenticationSingleton = registry->ctx<AuthenticationSingleton>();

        // If "ProcessChallenge" fails, we have either hit a bad memory allocation or a SRP-6a safety check, thus we should close the connection
        if (!authenticationSingleton.srp.ProcessChallenge(logonChallenge.s, logonChallenge.B))
        {
            netClient->Close();
            return true;
        }

        std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<36>();
        ClientLogonHandshake clientResponse;

        std::memcpy(clientResponse.M1, authenticationSingleton.srp.M, 32);

        buffer->Put(Opcode::CMSG_LOGON_HANDSHAKE);
        buffer->PutU16(0);

        u16 payloadSize = clientResponse.Serialize(buffer);
        buffer->Put<u16>(payloadSize, 2);
        netClient->Send(buffer);

        netClient->SetConnectionStatus(ConnectionStatus::AUTH_HANDSHAKE);
        return true;
    }
    bool AuthHandlers::HandshakeResponseHandler(std::shared_ptr<NetClient> netClient, std::shared_ptr<NetPacket> packet)
    {
        // Handle handshake response
        ServerLogonHandshake logonResponse;
        logonResponse.Deserialize(packet->payload);

        entt::registry* registry = ServiceLocator::GetRegistry();
        AuthenticationSingleton& authenticationSingleton = registry->ctx<AuthenticationSingleton>();
        ConnectionDeferredSingleton& connectionDeferredSingleton = registry->ctx<ConnectionDeferredSingleton>();

        if (!authenticationSingleton.srp.VerifySession(logonResponse.HAMK))
        {
            DebugHandler::PrintWarning("Unsuccessful Login");
            netClient->Close();
            return true;
        }
        else
        {
            DebugHandler::PrintSuccess("Successful Login");
        }

        std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<128>();
        buffer->Put(Opcode::CMSG_CONNECTED);
        buffer->PutU16(8);
        buffer->Put(AddressType::REGION);
        buffer->PutU8(0);

        const NetSocket::ConnectionInfo& connectionInfo = connectionDeferredSingleton.netServer->GetSocket()->GetConnectionInfo();
        buffer->PutU32(connectionInfo.ipAddr);
        buffer->PutU16(connectionInfo.port);

        netClient->Send(buffer);

        netClient->SetConnectionStatus(ConnectionStatus::AUTH_SUCCESS);
        return true;
    }
}