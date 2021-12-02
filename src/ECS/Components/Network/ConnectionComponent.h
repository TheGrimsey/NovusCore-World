#pragma once
#include <NovusTypes.h>
#include <Utils/ConcurrentQueue.h>
#include <Networking/NetPacket.h>
#include <Networking/NetClient.h>

enum class PacketPriority
{
    LOW,
    MEDIUM,
    HIGH,
    IMMEDIATE
};

#define LOW_PRIORITY_TIME 1
#define MEDIUM_PRIORITY_TIME 0.5f

struct ConnectionComponent
{
    ConnectionComponent() : packetQueue(256)
    {
        lowPriorityBuffer = Bytebuffer::Borrow<8192>();
        mediumPriorityBuffer = Bytebuffer::Borrow<8192>();
        highPriorityBuffer = Bytebuffer::Borrow<8192>();
    }

    std::shared_ptr<NetClient> netClient;
    moodycamel::ConcurrentQueue<std::shared_ptr<NetPacket>> packetQueue;

    void AddPacket(std::shared_ptr<Bytebuffer> buffer, PacketPriority priority = PacketPriority::MEDIUM)
    {
        assert(buffer->writtenData <= 8192);

        std::shared_ptr<Bytebuffer> bufferToUse = nullptr;
        if (priority == PacketPriority::LOW)
        {
            bufferToUse = lowPriorityBuffer;
        }
        else if (priority == PacketPriority::MEDIUM)
        {
            bufferToUse = mediumPriorityBuffer;
        }
        else if (priority == PacketPriority::HIGH)
        {
            bufferToUse = highPriorityBuffer;
        }
        else if (priority == PacketPriority::IMMEDIATE)
        {
            netClient->Send(buffer);
            return;
        }

        // We send priority buffer's content before we add this due to size constraints
        if (bufferToUse->GetSpace() < buffer->writtenData)
        {
            size_t spaceLeft = buffer->writtenData - bufferToUse->GetSpace();
            if (spaceLeft > 0)
            {
                memset(bufferToUse->GetWritePointer(), 0, spaceLeft);
            }

            netClient->Send(bufferToUse);
            bufferToUse->Reset();

            if (bufferToUse == lowPriorityBuffer)
                lowPriorityTimer = 0;
            else if (bufferToUse == mediumPriorityBuffer)
                mediumPriorityTimer = 0;
        }

        memcpy(bufferToUse->GetWritePointer(), buffer->GetDataPointer(), buffer->writtenData);
        bufferToUse->writtenData += buffer->writtenData;
    }

    // The reason we are not using the Timer class is due to performance reasons (Timer creates a timepoint every call to GetLifetime())
    f32 lowPriorityTimer = 0;
    f32 mediumPriorityTimer = 0;
    std::shared_ptr<Bytebuffer> lowPriorityBuffer;
    std::shared_ptr<Bytebuffer> mediumPriorityBuffer;
    std::shared_ptr<Bytebuffer> highPriorityBuffer;
};