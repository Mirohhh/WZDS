#include <windows.h>
#include <iostream>
#include <sddl.h>
#include <string>
#include <WzdIoctl.h>
#include <WzdTelemetry.h>
#include "PathTranslator.h"
#include "TelemetryEngine.h"

std::wstring ParseSidAndAccount(const UCHAR *sidBuffer, ULONG sidLength)
{
    if (sidLength == 0 || sidBuffer == nullptr)
    {
        return L"<NULL>";
    }
    PSID pSid = (PSID)sidBuffer;
    if (!IsValidSid(pSid))
    {
        return L"<INVALID SID>";
    }
    LPWSTR stringSid = NULL;

    std::wstring userSid = L"";
    // Convert the binary SID to a string format (e.g., S-1-5-21-...)
    if (ConvertSidToStringSidW(pSid, &stringSid))
    {
        userSid = stringSid;
        LocalFree(stringSid);
    }
    else
        return L"<CONVERSION_FAILED_ERR>" + std::to_wstring(GetLastError()) + L"";

    // Resolve to human-readable account name (e.g., DOMAIN\User) for better clarity in logs
    WCHAR accountName[256];
    WCHAR domainName[256];
    DWORD accountNameSize = 256;
    DWORD domainNameSize = 256;
    SID_NAME_USE sidType;
    if (LookupAccountSidW(NULL, pSid, accountName, &accountNameSize, domainName, &domainNameSize, &sidType))
    {
        return L" (" + std::wstring(domainName) + L"\\" + std::wstring(accountName) + L")";
    }
    return userSid; // Fallback to string SID if resolution fails
}

// Concrete implementation of the Observer
class ConsoleLogger : public ITelemetryObserver
{
private:
    PathTranslator m_Translator; // Composition

public:
    void OnProcessEvent(const WZD_PROCESS_EVENT &event) override
    {
        const wchar_t *action = (event.EventType == (UINT32)WZDEventProcessCreate) ? L"[+] CREATED   " : L"[-] TERMINATED";
        const wchar_t *arch = (event.Is32Bit == 1) ? L"x86" : L"x64";
        std::wstring cleanImagePath = m_Translator.Translate(event.ImageFileName);

        // LPWSTR stringSid = NULL;
        // std::wstring userSid = L"<NULL>";
        // if (event.SidLength > 0 && ConvertSidToStringSidW((PSID)event.Sid, &stringSid)) {
        //     userSid = stringSid;
        //     LocalFree(stringSid);
        // }
        std::wstring userSid = ParseSidAndAccount(event.Sid, event.SidLength);

        wchar_t pidBuffer[64];
        swprintf_s(pidBuffer, 64, L"0x%04X:%-5u", event.ProcessId, event.ProcessId);

        std::wcout << action << L" | PID: " << pidBuffer
                   << L" | Session: " << event.SessionId
                   << L" | SID: " << userSid << L"\n"
                   << L"    Arch : " << arch << L"\n"
                   << L"    Image: " << cleanImagePath << L"\n";

        if (event.EventType == WZDEventProcessCreate)
        {
            std::wcout << L"    Cmd  : " << event.CommandLine << L"\n";
        }
        std::wcout << L"---------------------------------------------------\n";

        if (std::wcout.fail())
            std::wcout.clear();
    }

    void OnDroppedEvents(ULONG count) override
    {
        std::wcout << L"\n[!] WARNING: KERNEL DROPPED " << count << L" EVENTS (DOS ATTEMPT DETECTED!)\n";
    }
};

int main()
{
    // [NEW] OFFSET DIAGNOSTICS: Prints the exact memory location of variables
    std::wcout << L"[DIAGNOSTICS] Offset of SessionId: " << offsetof(WZD_PROCESS_EVENT, SessionId) << L"\n";
    std::wcout << L"[DIAGNOSTICS] Offset of SidLength: " << offsetof(WZD_PROCESS_EVENT, SidLength) << L"\n";
    std::wcout << L"[DIAGNOSTICS] Offset of Is32Bit: " << offsetof(WZD_PROCESS_EVENT, Is32Bit) << L"\n";

    std::wcout << L"[WZDC] WatchZork Detection Client Initializing...\n";

    // 1. Open Kernel Handle
    HANDLE hDevice = CreateFileW(WZD_USER_DEVICE_NAME, GENERIC_READ | GENERIC_WRITE,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                                 FILE_ATTRIBUTE_NORMAL, NULL);

    if (hDevice == INVALID_HANDLE_VALUE)
    {
        std::wcerr << L"[WZDC] ERROR: Failed to open driver! Ensure WZDK is running and you are Admin.\n";
        return 1;
    }
    std::wcout << L"[WZDC] Connected to WZDK.\n";

    // 2. Instantiate the Logger and Engine
    ConsoleLogger logger;
    TelemetryEngine engine(hDevice, &logger);

    // 3. Start Engine
    if (!engine.Start())
    {
        std::wcerr << L"[WZDC] ERROR: Failed to start Telemetry Engine.\n";
        CloseHandle(hDevice);
        return 1;
    }

    // 4. Wait for exit
    std::wcout << L"\n====================================================\n";
    std::wcout << L"[WZDC] LIVE TELEMETRY ACTIVE. Press ENTER to stop.\n";
    std::wcout << L"====================================================\n\n";
    std::wcin.get();

    // 5. Teardown (Engine goes out of scope and calls Stop() automatically via RAII)
    std::wcout << L"[WZDC] Shutting down...\n";
    engine.Stop();
    CloseHandle(hDevice);

    std::wcout << L"[WZDC] Graceful exit complete.\n";
    return 0;
}