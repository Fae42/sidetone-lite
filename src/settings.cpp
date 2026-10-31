#include "settings.h"

#include <windows.h>

namespace settings {
std::wstring GetSettingsPath() {
    wchar_t path[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return {};
    }
    const std::wstring executable(path, length);
    const size_t separator = executable.find_last_of(L"\\/");
    if (separator == std::wstring::npos) {
        return {};
    }
    return executable.substr(0, separator + 1) + L"Jianting.ini";
}

std::wstring LoadRememberedInputId() {
    const std::wstring path = GetSettingsPath();
    if (path.empty()) {
        return {};
    }
    wchar_t id[2048];
    const DWORD length = GetPrivateProfileStringW(L"Audio", L"LastInputId", L"", id, 2048, path.c_str());
    if (length == 0 || length >= 2047) {
        return {};
    }
    return std::wstring(id, length);
}

void SaveRememberedInputId(const std::wstring& id) {
    const std::wstring path = GetSettingsPath();
    if (!id.empty() && !path.empty()) {
        WritePrivateProfileStringW(L"Audio", L"LastInputId", id.c_str(), path.c_str());
    }
}
}
