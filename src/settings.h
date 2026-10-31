#pragma once

#include <string>

namespace settings {
std::wstring GetSettingsPath();
std::wstring LoadRememberedInputId();
void SaveRememberedInputId(const std::wstring& id);
}
