#include "TelemetryEngine.h"
#include <iostream>

TelemetryEngine::TelemetryEngine(HANDLE hDevice, ITelemetryObserver *observer)
    : m_hDevice(hDevice), m_Observer(observer), m_MappedAddress(NULL), m_RingBuffer(NULL), m_IsRunning(false)
{
}

TelemetryEngine::~TelemetryEngine()
{
    Stop(); // Ensure thread is killed if object goes out of scope
}

bool TelemetryEngine::Start()
{
    DWORD bytesReturned = 0;
    BOOL mapSuccess = DeviceIoControl(
        m_hDevice, IOCTL_WZD_MAP_MEMORY,
        NULL, 0, &m_MappedAddress, sizeof(PVOID),
        &bytesReturned, NULL);

    if (!mapSuccess || m_MappedAddress == NULL)
    {
        return false;
    }

    m_RingBuffer = static_cast<PWZD_RING_BUFFER>(m_MappedAddress);
    m_IsRunning = true;
    m_ConsumerThread = std::thread(&TelemetryEngine::ConsumerLoop, this);

    return true;
}

void TelemetryEngine::Stop()
{
    if (m_IsRunning)
    {
        m_IsRunning = false;
        if (m_ConsumerThread.joinable())
        {
            m_ConsumerThread.join();
        }
    }
}

void TelemetryEngine::ConsumerLoop()
{
    while (m_IsRunning)
    {
        LONG currentTail = m_RingBuffer->Tail;
        LONG state = _InterlockedCompareExchange(&m_RingBuffer->SlotStates[currentTail], WZDRB_SlotFree, WZDRB_SlotFree);

        if (state == WZDRB_SlotReady)
        {
            // Deep copy the event payload
            // WZD_PROCESS_EVENT localEvent = m_RingBuffer->Events[currentTail]; not safe due to potential padding bytes
            WZD_PROCESS_EVENT localEvent = {}; // Zero-initialize to prevent uninitialized data issues
            memcpy(&localEvent, (void *)&m_RingBuffer->Events[currentTail], sizeof(WZD_PROCESS_EVENT));

            // Free the slot immediately for the kernel
            _InterlockedExchange(&m_RingBuffer->SlotStates[currentTail], WZDRB_SlotFree);
            _InterlockedExchange(&m_RingBuffer->Tail, (currentTail + 1) & WZD_EVENT_MASK);

            // Notify the Observer! (No printing here)
            if (m_Observer)
            {
                m_Observer->OnProcessEvent(localEvent);
            }
        }
        else
        {
            Sleep(1);
        }

        // Check for dropped events
        LONG dropped = _InterlockedCompareExchange(&m_RingBuffer->DroppedEventsCount, 0, 0);
        if (dropped > 0)
        {
            _InterlockedExchange(&m_RingBuffer->DroppedEventsCount, 0);
            if (m_Observer)
            {
                m_Observer->OnDroppedEvents(dropped);
            }
        }
    }
}