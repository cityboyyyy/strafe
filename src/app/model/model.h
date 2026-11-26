#pragma once

#include <DirectXMath.h>
#include <cstdint>
#include <string>
#include <vector>

namespace menu::model
{
    struct vertex
    {
        DirectX::XMFLOAT3 position{};
        DirectX::XMFLOAT3 normal{};
        DirectX::XMFLOAT2 uv[3]{};
    };

    struct image
    {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        bool srgb = false;
        std::vector<std::uint8_t> pixels;
    };

    enum class alpha_mode : std::uint8_t
    {
        opaque,
        mask,
        blend
    };

    struct texture_ref
    {
        int image = -1;
        int uv = 0;
    };

    struct material
    {
        DirectX::XMFLOAT4 base_color{ 1.0f, 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT3 emissive{ 0.0f, 0.0f, 0.0f };
        float metallic = 1.0f;
        float roughness = 1.0f;
        float alpha_cutoff = 0.5f;
        float normal_scale = 1.0f;
        float occlusion_strength = 1.0f;
        alpha_mode alpha = alpha_mode::opaque;
        bool double_sided = false;
        bool split_alpha = false;
        float glass_opacity = 1.0f;
        float transmission = 0.0f;
        texture_ref base_texture;
        texture_ref metal_rough_texture;
        texture_ref normal_texture;
        texture_ref occlusion_texture;
        texture_ref emissive_texture;
    };

    struct primitive
    {
        std::string name;
        std::uint32_t first_index = 0;
        std::uint32_t index_count = 0;
        int material = -1;
    };

    struct model
    {
        std::vector<vertex> vertices;
        std::vector<std::uint32_t> indices;
        std::vector<primitive> primitives;
        std::vector<material> materials;
        std::vector<image> images;
        DirectX::XMFLOAT3 bounds_min{};
        DirectX::XMFLOAT3 bounds_max{};
    };
}
