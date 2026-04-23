#pragma once
#include <string>
#include <unordered_map>

// Encapsulates NT to DOS path translation logic
class PathTranslator
{
private:
    std::unordered_map<std::wstring, std::wstring> m_DeviceMap;

public:
    PathTranslator();

    // Translates "\Device\HarddiskVolume3\..." to "C:\..."
    std::wstring Translate(const wchar_t *ntPath) const;
};