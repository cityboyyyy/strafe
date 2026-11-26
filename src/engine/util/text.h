#pragma once

#include <windows.h>

#include <string>
#include <string_view>

namespace engine::util
{
    std::string to_utf8(std::wstring_view text);
    std::wstring from_utf8(std::string_view text);
    std::string win32_message(DWORD code);
}
