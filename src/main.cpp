#include <NovusTypes.h>
#include "defines.h"
#include <Utils/Message.h>
#include <Utils/StringUtils.h>

#include <future>

#include "EngineLoop.h"
#include "ConsoleCommands.h"

#ifdef _WIN32
#include <Windows.h>
#endif

i32 main()
{
#ifdef _WIN32 //Windows
    SetConsoleTitle(WINDOWNAME);
#endif

    EngineLoop engineLoop;
    engineLoop.Start();

    ConsoleCommandHandler consoleCommandHandler;
    std::future<std::string> future = std::async(std::launch::async, StringUtils::GetLineFromCin);
    while (true)
    {
        Message message;
        bool shouldExit = false;

        while (engineLoop.TryGetMessage(message))
        {
            if (message.code == MSG_OUT_EXIT_CONFIRM)
            {
                shouldExit = true;
                break;
            }
            else if (message.code == MSG_OUT_PRINT)
            {
                DebugHandler::Print(*message.message);
                delete message.message;
            }
        }

        if (shouldExit)
            break;

        if (future.wait_for(std::chrono::milliseconds(50)) == std::future_status::ready)
        {
            std::string command = future.get();
            std::transform(command.begin(), command.end(), command.begin(), ::tolower); // Convert command to lowercase

            consoleCommandHandler.HandleCommand(engineLoop, command);
            future = std::async(std::launch::async, StringUtils::GetLineFromCin);
        }
    }

    engineLoop.Stop();
    return 0;
}