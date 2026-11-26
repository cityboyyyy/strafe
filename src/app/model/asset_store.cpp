#include "asset_store.h"
#include "catalog.h"

#include <Windows.h>

#include <fstream>
#include <iterator>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace menu::model
{
    namespace
    {
        std::wstring resource_name(const std::filesystem::path& relative)
        {
            std::wstring name = relative.generic_wstring();
            for (wchar_t& c : name)
            {
                if (c >= L'a' && c <= L'z')
                    c = static_cast<wchar_t>(c - L'a' + L'A');
                else if (!((c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9')))
                    c = L'_';
            }
            return name;
        }

        std::span<const std::uint8_t> embedded(const std::filesystem::path& relative)
        {
            const std::wstring name = resource_name(relative);
            HRSRC resource = FindResourceW(nullptr, name.c_str(), MAKEINTRESOURCEW(10));
            if (!resource)
                return {};
            HGLOBAL handle = LoadResource(nullptr, resource);
            const void* data = handle ? LockResource(handle) : nullptr;
            const DWORD size = SizeofResource(nullptr, resource);
            if (!data || size == 0)
                return {};
            return { static_cast<const std::uint8_t*>(data), size };
        }
    }

    std::span<const std::uint8_t> asset_bytes(const std::filesystem::path& relative)
    {
        static std::mutex lock;
        static std::map<std::wstring, std::vector<std::uint8_t>> cache;

        if (const auto inside = embedded(relative); !inside.empty())
            return inside;

        const std::wstring key = relative.generic_wstring();
        {
            std::lock_guard guard(lock);
            if (const auto found = cache.find(key); found != cache.end())
                return found->second;
        }

        std::ifstream file(asset_path(relative), std::ios::binary);
        if (file)
        {
            std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            if (!bytes.empty())
            {
                std::lock_guard guard(lock);
                auto& stored = cache[key];
                stored = std::move(bytes);
                return stored;
            }
        }

        return {};
    }
}
