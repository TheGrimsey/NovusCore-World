#include "AuthHandlers.h"
#include <entt.hpp>
#include <sstream>
#include <Database/DBConnection.h>
#include <Database/mysql/QueryResult.h>
#include <Networking/NetStructures.h>
#include <Networking/NetPacket.h>
#include <Networking/NetClient.h>
#include <Networking/NetPacketHandler.h>
#include <Networking/PacketUtils.h>
#include <Utils/ByteBuffer.h>
#include <Utils/StringUtils.h>

#include "../../../../Utils/ServiceLocator.h"
#include "../../../../ECS/Components/Singletons/DBSingleton.h"
#include "../../../../ECS/Components/Network/AuthenticationSingleton.h"
#include "../../../../ECS/Components/Network/ConnectionDeferredSingleton.h"
#include "../../../../ECS/Components/Network/Authentication.h"

// @TODO: Remove Temporary Includes when they're no longer needed
#include <Utils/DebugHandler.h>

namespace Client
{
    void AuthHandlers::Setup(NetPacketHandler* netPacketHandler)
    {
        u16 authChallengeMinSize = static_cast<u16>(sizeof(u8) * 4 + sizeof(u16) + 4 + 256);
        i16 authChallengeMaxSize = authChallengeMinSize + 33;
        netPacketHandler->SetMessageHandler(Opcode::CMSG_LOGON_CHALLENGE, { ConnectionStatus::AUTH_CHALLENGE, authChallengeMinSize, authChallengeMaxSize, AuthHandlers::HandshakeHandler });
        netPacketHandler->SetMessageHandler(Opcode::CMSG_LOGON_HANDSHAKE, { ConnectionStatus::AUTH_HANDSHAKE, sizeof(ClientLogonHandshake), AuthHandlers::HandshakeResponseHandler });
    }

    bool AuthHandlers::HandshakeHandler(std::shared_ptr<NetClient> netClient, std::shared_ptr<NetPacket> packet)
    {
        ClientLogonChallenge logonChallenge;
        logonChallenge.Deserialize(packet->payload);

        entt::registry* registry = ServiceLocator::GetRegistry();
        Authentication& authentication = registry->get<Authentication>(netClient->GetEntity());
        authentication.username = logonChallenge.username;

        std::shared_ptr<Bytebuffer> sBuffer = Bytebuffer::Borrow<4>();
        std::shared_ptr<Bytebuffer> vBuffer = Bytebuffer::Borrow<256>();

        DBSingleton& dbSingleton = registry->ctx<DBSingleton>();

        std::stringstream ss;
        ss << "SELECT salt, verifier FROM accounts WHERE username='" << authentication.username << "';";

        std::shared_ptr<QueryResult> result = dbSingleton.auth.Query(ss.str());

        // If we found no account with the provided username we "temporarily" close the connection
        // TODO: Generate Random Salt & Verifier (Shorter length?) and "fake" logon challenge to not give away if an account exists or not
        if (result->GetAffectedRows() == 0)
        {
            DebugHandler::PrintWarning("Unsuccessful Login for: %s", authentication.username.c_str());
            netClient->Close();
            return true;
        }

        result->GetNextRow();
        {
            const Field& saltField = result->GetField(0);
            const Field& verifierField = result->GetField(1);

            std::string salt = saltField.GetString();
            StringUtils::HexStrToBytes(salt.c_str(), sBuffer->GetDataPointer());

            std::string verifier = verifierField.GetString();
            StringUtils::HexStrToBytes(verifier.c_str(), vBuffer->GetDataPointer());
        }

        authentication.srp.saltBuffer = sBuffer;
        authentication.srp.verifierBuffer = vBuffer;

        // If "StartVerification" fails, we have either hit a bad memory allocation or a SRP-6a safety check, thus we should close the connection
        if (!authentication.srp.StartVerification(authentication.username, logonChallenge.A))
        {
            DebugHandler::PrintWarning("Unsuccessful Login for: %s", authentication.username.c_str());
            netClient->Close();
            return true;
        }

        std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<512>();
        ServerLogonChallenge serverChallenge;
        serverChallenge.status = 0;

        std::memcpy(serverChallenge.s, authentication.srp.saltBuffer->GetDataPointer(), authentication.srp.saltBuffer->size);
        std::memcpy(serverChallenge.B, authentication.srp.bBuffer->GetDataPointer(), authentication.srp.bBuffer->size);

        buffer->Put(Opcode::SMSG_LOGON_CHALLENGE);
        buffer->PutU16(0);

        u16 payloadSize = serverChallenge.Serialize(buffer);
        buffer->Put<u16>(payloadSize, 2);
        netClient->Send(buffer);

        netClient->SetConnectionStatus(ConnectionStatus::AUTH_HANDSHAKE);
        return true;
    }
    bool AuthHandlers::HandshakeResponseHandler(std::shared_ptr<NetClient> netClient, std::shared_ptr<NetPacket> packet)
    {
        ClientLogonHandshake clientHandshake;
        clientHandshake.Deserialize(packet->payload);

        entt::registry* registry = ServiceLocator::GetRegistry();
        Authentication& authentication = registry->get<Authentication>(netClient->GetEntity());

        if (!authentication.srp.VerifySession(clientHandshake.M1))
        {
            DebugHandler::PrintWarning("Unsuccessful Login for: %s", authentication.username.c_str());
            netClient->Close();
            return true;
        }
        else
        {
            DebugHandler::PrintSuccess("Successful Login for: %s", authentication.username.c_str());
        }

        ServerLogonHandshake serverHandsake;
        std::memcpy(serverHandsake.HAMK, authentication.srp.HAMK, sizeof(authentication.srp.HAMK));

        std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<128>();
        buffer->Put(Opcode::SMSG_LOGON_HANDSHAKE);
        buffer->PutU16(0);

        u16 payloadSize = serverHandsake.Serialize(buffer);
        buffer->Put<u16>(payloadSize, 2);
        netClient->Send(buffer);

        netClient->SetConnectionStatus(ConnectionStatus::AUTH_SUCCESS);
        return true;
    }
}