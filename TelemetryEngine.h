#pragma once
#include <windows.h>
#include <thread>
#include <atomic>
#include <WzdIoctl.h>
#include <WzdTelemetry.h>

// 1. The Observer Interface (Pure Virtual Class)
class ITelemetryObserver
{
public:
    virtual ~ITelemetryObserver() = default;
    virtual void OnProcessEvent(const WZD_PROCESS_EVENT &event) = 0;
    virtual void OnDroppedEvents(ULONG count) = 0;
};

// 2. The Engine Class (Manages the Ring Buffer & Thread via RAII)
class TelemetryEngine
{
private:
    HANDLE m_hDevice;
    PVOID m_MappedAddress;
    PWZD_RING_BUFFER m_RingBuffer;

    std::atomic<bool> m_IsRunning;
    std::thread m_ConsumerThread;
    ITelemetryObserver *m_Observer; // Pointer to whoever is listening

    void ConsumerLoop();

public:
    TelemetryEngine(HANDLE hDevice, ITelemetryObserver *observer);
    ~TelemetryEngine(); // RAII implementation for safe teardown

    bool Start();
    void Stop();
};