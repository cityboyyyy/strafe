#pragma once

#include <DirectXMath.h>
#include <cstdint>
#include <filesystem>
#include <span>

namespace menu::model
{
    struct catalog_entry
    {
        const char* label;
        const char* subtitle;
        DirectX::XMFLOAT3 accent;
        const wchar_t* file;
        DirectX::XMFLOAT3 rotation;
        std::uint32_t max_texture_size;
        float glass_opacity;
        float exposure;
        float ambient;
        bool neutral_tone;
        float key_front;
    };

    std::span<const catalog_entry> catalog();
    std::filesystem::path asset_path(const std::filesystem::path& relative);
    std::filesystem::path model_path(const catalog_entry& entry);
    std::filesystem::path model_asset(const catalog_entry& entry);
}
