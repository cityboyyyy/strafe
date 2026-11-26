#include "text.h"

namespace engine::util
{
    std::string to_utf8(std::wstring_view text)
    {
        if (text.empty())
            return {};
        const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0,
            nullptr, nullptr);
        if (size <= 0)
            return {};
        std::string result(static_cast<std::size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
        return result;
    }

    std::wstring from_utf8(std::string_view text)
    {
        if (text.empty())
            return {};
        const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (size <= 0)
            return {};
        std::wstring result(static_cast<std::size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
        return result;
    }

    std::string win32_message(DWORD code)
    {
        wchar_t* buffer = nullptr;
        const DWORD length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
        if (length == 0 || buffer == nullptr)
            return "Windows error " + std::to_string(code) + ".";
        std::wstring text(buffer, length);
        LocalFree(buffer);
        while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n' || text.back() == L' '))
            text.pop_back();
        return to_utf8(text);
    }
}
