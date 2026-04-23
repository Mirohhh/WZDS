#include "PathTranslator.h"
#include <windows.h>

PathTranslator::PathTranslator()
{
    wchar_t driveStrings[512];
    if (GetLogicalDriveStringsW(512, driveStrings))
    {
        wchar_t *currentDrive = driveStrings;
        while (*currentDrive)
        {
            std::wstring drive(currentDrive, 2); // Get "C:"
            wchar_t devicePath[MAX_PATH];
            if (QueryDosDeviceW(drive.c_str(), devicePath, MAX_PATH))
            {
                m_DeviceMap[devicePath] = drive + L"\\";
            }
            currentDrive += wcslen(currentDrive) + 1;
        }
    }
}

std::wstring PathTranslator::Translate(const wchar_t *ntPath) const
{
    std::wstring path(ntPath);
    if (path.rfind(L"\\??\\", 0) == 0)
    {
        return path.substr(4);
    }
    for (const auto &pair : m_DeviceMap)
    {
        if (path.rfind(pair.first, 0) == 0)
        {
            return pair.second + path.substr(pair.first.length() + 1);
        }
    }
    return path; // Return original if translation fails
}