#include "catalog.h"

#include <Windows.h>
#include <array>

namespace menu::model
{
    namespace
    {
        constexpr std::array entries{
            catalog_entry{ "Xbox Elite", "Microsoft  \xC2\xB7  XInput", { 0.30f, 0.82f, 0.48f }, L"xbox/xbox.glb", { 0.0f, 0.0f, 0.0f }, 2560, 0.0f, 1.0f, 1.0f, false, 0.80f },
            catalog_entry{ "DualSense", "Sony  \xC2\xB7  PlayStation 5", { 0.36f, 0.58f, 1.00f }, L"ps5/ps5.glb", { -90.0f, 180.0f, 0.0f }, 2560, 0.4f, 0.86f, 0.40f, true, 0.30f },
        };

        std::filesystem::path find_assets_root()
        {
            wchar_t buffer[MAX_PATH]{};
            GetModuleFileNameW(nullptr, buffer, MAX_PATH);

            std::filesystem::path directory = std::filesystem::path(buffer).parent_path();
            for (int depth = 0; depth < 5 && !directory.empty(); ++depth)
            {
                const auto candidate = directory / L"assets";
                std::error_code ec;
                if (std::filesystem::is_directory(candidate / L"models", ec))
                    return candidate;
                if (directory == directory.parent_path())
                    break;
                directory = directory.parent_path();
            }
            return std::filesystem::path(buffer).parent_path() / L"assets";
        }
    }

    std::span<const catalog_entry> catalog()
    {
        return entries;
    }

    std::filesystem::path asset_path(const std::filesystem::path& relative)
    {
        static const std::filesystem::path root = find_assets_root();
        return root / relative;
    }

    std::filesystem::path model_path(const catalog_entry& entry)
    {
        return asset_path(std::filesystem::path(L"models") / entry.file);
    }

    std::filesystem::path model_asset(const catalog_entry& entry)
    {
        return std::filesystem::path(L"models") / entry.file;
    }
}
