#include "EngineLoop.h"
#include <thread>
#include <Utils/Timer.h>
#include "Utils/ServiceLocator.h"
#include <Networking/NetPacketHandler.h>
#include <tracy/Tracy.hpp>

// Component Singletons
#include "ECS/Components/Singletons/TimeSingleton.h"
#include "ECS/Components/Singletons/MapSingleton.h"
#include "ECS/Components/Singletons/DBSingleton.h"
#include "ECS/Components/Network/ConnectionSingleton.h"
#include "ECS/Components/Network/ConnectionDeferredSingleton.h"
#include "ECS/Components/Network/AuthenticationSingleton.h"

// Components

// Systems
#include "ECS/Systems/SpawnPlayerSystem.h"
#include "ECS/Systems/UpdateEntityPositionSystem.h"
#include "ECS/Systems/CreatePlayerTreeSystem.h"
#include "ECS/Systems/Network/ConnectionSystems.h"

// Handlers
#include "Network/Handlers/Self/Auth/AuthHandlers.h"
#include "Network/Handlers/Self/GeneralHandlers.h"
#include "Network/Handlers/Client/Auth/AuthHandlers.h"
#include "Network/Handlers/Client/GeneralHandlers.h"

#include <Containers/KDTree.h>
#include <Gameplay/ECS/Components/Transform.h>
#include <Gameplay/ECS/Components/GameEntity.h>

#ifdef WIN32
#include "Winsock.h"
#endif

EngineLoop::EngineLoop()
    : _isRunning(false), _inputQueue(256), _outputQueue(16)
{
#ifdef WIN32
    WSADATA data;
    i32 code = WSAStartup(MAKEWORD(2, 2), &data);
    if (code != 0)
    {
        DebugHandler::PrintFatal("[Network] Failed to initialize WinSock");
    }
#endif

    _network.client = std::make_shared<NetClient>();
    _network.client->Init(NetSocket::Mode::TCP);

    std::shared_ptr<NetSocket> clientSocket = _network.client->GetSocket();
    clientSocket->SetBlockingState(false);
    clientSocket->SetNoDelayState(true);
    clientSocket->SetSendBufferSize(8192);
    clientSocket->SetReceiveBufferSize(8192);

    _network.server = std::make_shared<NetServer>();
}

EngineLoop::~EngineLoop()
{
}

void EngineLoop::Start()
{
    if (_isRunning)
        return;

    std::thread threadRun = std::thread(&EngineLoop::Run, this);
    threadRun.detach();
}

void EngineLoop::Stop()
{
    if (!_isRunning)
        return;

    Message message;
    message.code = MSG_IN_EXIT;
    PassMessage(message);
}

void EngineLoop::PassMessage(Message& message)
{
    _inputQueue.enqueue(message);
}

bool EngineLoop::TryGetMessage(Message& message)
{
    return _outputQueue.try_dequeue(message);
}

void EngineLoop::Run()
{
    tracy::SetThreadName("EngineThread");

    _isRunning = true;

    SetupUpdateFramework();
    
    /*Point2D points[] = {{0, 10}, {10, 0}, {5, 5}, {10, 10}, {25, 25}, {75, 75}, {150, 150}, {250, 250}, {500, 500}};
    Tree2D tree(std::begin(points), std::end(points));

    Point2D closestPoint = { 0, 0 };
    if (tree.GetNearest(closestPoint, closestPoint))
    {
        volatile i32 test = 5;
    }*/

    DBSingleton& dbSingleton = _updateFramework.gameRegistry.set<DBSingleton>();
    dbSingleton.auth.Connect("localhost", 3306, "root", "ascent", "novuscore", 0);

    TimeSingleton& timeSingleton = _updateFramework.gameRegistry.set<TimeSingleton>();
    MapSingleton& mapSingleton = _updateFramework.gameRegistry.set<MapSingleton>();
    ConnectionSingleton& connectionSingleton = _updateFramework.gameRegistry.set<ConnectionSingleton>();
    ConnectionDeferredSingleton& connectionDeferredSingleton = _updateFramework.gameRegistry.set<ConnectionDeferredSingleton>();
    AuthenticationSingleton& authenticationSingleton = _updateFramework.gameRegistry.set<AuthenticationSingleton>();

    connectionSingleton.netClient = _network.client;
    bool didConnect = connectionSingleton.netClient->Connect("127.0.0.1", 8000);
    ConnectionUpdateSystem::Self_HandleConnect(connectionSingleton.netClient, didConnect);

    connectionDeferredSingleton.netServer = _network.server;
    connectionDeferredSingleton.netServer->SetOnConnectCallback(ConnectionUpdateSystem::Server_HandleConnect);
    if (!_network.server->Init(NetSocket::Mode::TCP, "127.0.0.1", 4500))
    {
        DebugHandler::PrintFatal("Network : Failed to initialize server (NovusCore - World)");
    }

    LoadBasicCreatureDataFromDB();

    Timer timer;
    f32 targetDelta = 1.0f / 30.0f;

    while (true)
    {
        f32 deltaTime = timer.GetDeltaTime();
        timer.Tick();

        timeSingleton.lifeTimeInS = timer.GetLifeTime();
        timeSingleton.lifeTimeInMS = timeSingleton.lifeTimeInS * 1000;
        timeSingleton.deltaTime = deltaTime;

        if (!Update())
            break;

        {
            ZoneScopedNC("WaitForTickRate", tracy::Color::AntiqueWhite1)

            // Wait for tick rate, this might be an overkill implementation but it has the even tickrate I've seen - MPursche
            {
                ZoneScopedNC("Sleep", tracy::Color::AntiqueWhite1) for (deltaTime = timer.GetDeltaTime(); deltaTime < targetDelta - 0.0025f; deltaTime = timer.GetDeltaTime())
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
            {
                ZoneScopedNC("Yield", tracy::Color::AntiqueWhite1) for (deltaTime = timer.GetDeltaTime(); deltaTime < targetDelta; deltaTime = timer.GetDeltaTime())
                {
                    std::this_thread::yield();
                }
            }
        }


        FrameMark
    }

    // Clean up stuff here

    Message exitMessage;
    exitMessage.code = MSG_OUT_EXIT_CONFIRM;
    _outputQueue.enqueue(exitMessage);
}

bool EngineLoop::Update()
{
    ZoneScopedNC("Update", tracy::Color::Blue2)
    {
        ZoneScopedNC("HandleMessages", tracy::Color::Green3)
            Message message;

        while (_inputQueue.try_dequeue(message))
        {
            if (message.code == -1)
                assert(false);

            if (message.code == MSG_IN_EXIT)
            {
                return false;
            }
            else if (message.code == MSG_IN_PING)
            {
                ZoneScopedNC("Ping", tracy::Color::Green3)
                    Message pongMessage;
                pongMessage.code = MSG_OUT_PRINT;
                pongMessage.message = new std::string("PONG!");
                _outputQueue.enqueue(pongMessage);
            }
        }
    }

    UpdateSystems();
    return true;
}

void EngineLoop::SetupUpdateFramework()
{
    tf::Framework& framework = _updateFramework.framework;
    entt::registry& registry = _updateFramework.gameRegistry;

    ServiceLocator::SetRegistry(&registry);
    SetMessageHandler();
    
    // SpawnPlayerSystem
    tf::Task spawnPlayerSystemTask = framework.emplace([&registry]()
    {
        ZoneScopedNC("SpawnPlayerSystem::Update", tracy::Color::Blue2);
        SpawnPlayerSystem::Update(registry);
    });

    // UpdateEntityPositionSystem
    tf::Task updateEntityPositionSystemTask = framework.emplace([&registry]()
    {
        ZoneScopedNC("UpdateEntityPositionSystem::Update", tracy::Color::Blue2);
        UpdateEntityPositionSystem::Update(registry);
    });
    updateEntityPositionSystemTask.gather(spawnPlayerSystemTask);

    // ConnectionUpdateSystem
    tf::Task connectionUpdateSystemTask = framework.emplace([&registry]()
    {
        ZoneScopedNC("ConnectionUpdateSystem::Update", tracy::Color::Blue2);
        ConnectionUpdateSystem::Update(registry);
    });
    connectionUpdateSystemTask.gather(updateEntityPositionSystemTask);

    // ConnectionDeferredSystem
    tf::Task connectionDeferredSystemTask = framework.emplace([&registry]()
    {
        ZoneScopedNC("ConnectionDeferredSystem::Update", tracy::Color::Blue2)
        ConnectionDeferredSystem::Update(registry);
    });
    connectionDeferredSystemTask.gather(connectionUpdateSystemTask);

    // CreatePlayerTreeSystem
    tf::Task createPlayerTreeSystemTask = framework.emplace([&registry]()
    {
        ZoneScopedNC("CreatePlayerTreeSystem::Update", tracy::Color::Blue2);
        CreatePlayerTreeSystem::Update(registry);
    });
    createPlayerTreeSystemTask.gather(connectionDeferredSystemTask);
}
void EngineLoop::SetMessageHandler()
{
    NetPacketHandler* selfNetPacketHandler = new NetPacketHandler();
    ServiceLocator::SetSelfNetPacketHandler(selfNetPacketHandler);
    InternalSocket::AuthHandlers::Setup(selfNetPacketHandler);
    InternalSocket::GeneralHandlers::Setup(selfNetPacketHandler);

    NetPacketHandler* clientNetPacketHandler = new NetPacketHandler();
    ServiceLocator::SetClientNetPacketHandler(clientNetPacketHandler);
    Client::AuthHandlers::Setup(clientNetPacketHandler);
    Client::GeneralHandlers::Setup(clientNetPacketHandler);
}
void EngineLoop::LoadBasicCreatureDataFromDB()
{
    DBSingleton& dbSingleton = _updateFramework.gameRegistry.ctx<DBSingleton>();

    std::string query = "SELECT * FROM creatures;"; 
    
    std::shared_ptr<QueryResult> result = dbSingleton.auth.Query(query);

    u64 numAffectedRows = result->GetAffectedRows();
    DebugHandler::PrintSuccess("Spawning creatures...");

    if (numAffectedRows != 0)
    {
        while (result->GetNextRow())
        {
            //const Field& idField = result->GetField(0);
            const Field& entryField = result->GetField(1);
            const Field& nameField = result->GetField(2);
            const Field& subNameField = result->GetField(3);
            const Field& displayIDField = result->GetField(4);
            const Field& scaleField = result->GetField(5);
            const Field& positionXField = result->GetField(6);
            const Field& positionYField = result->GetField(7);
            const Field& positionZField = result->GetField(8);
            const Field& orientationField = result->GetField(9);

            u32 entry = entryField.GetU32();
            std::string name = nameField.GetString();
            std::string subName = subNameField.GetString();
            u32 displayID = displayIDField.GetU32();
            f32 scale = scaleField.GetF32();
            vec3 position = vec3(positionXField.GetF32(), positionYField.GetF32(), positionZField.GetF32());
            f32 orientation = orientationField.GetF32();

            entt::entity entityID = _updateFramework.gameRegistry.create();
            Transform& transform = _updateFramework.gameRegistry.emplace<Transform>(entityID);

            transform.position = position;
            transform.scale *= scale;
            transform.rotation.z = glm::degrees(orientation);

            GameEntity& gameEntity = _updateFramework.gameRegistry.emplace<GameEntity>(entityID, GameEntity::Type::Creature, displayID);
            _updateFramework.gameRegistry.emplace<TransformIsDirty>(entityID);
        }
    }

    DebugHandler::PrintSuccess("Spawned %u creatures.", numAffectedRows);
}
void EngineLoop::UpdateSystems()
{
    ZoneScopedNC("UpdateSystems", tracy::Color::Blue2)
    {
        ZoneScopedNC("Taskflow::Run", tracy::Color::Blue2);
        _updateFramework.taskflow.run(_updateFramework.framework);
    }
    {
        ZoneScopedNC("Taskflow::WaitForAll", tracy::Color::Blue2);
        _updateFramework.taskflow.wait_for_all();
    }
}
