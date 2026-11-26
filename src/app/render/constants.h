#pragma once

#include <DirectXMath.h>
#include <cstdint>

namespace menu::render
{
    struct alignas(16) frame_constants
    {
        DirectX::XMFLOAT4X4 view_projection;
        DirectX::XMFLOAT4X4 world;
        DirectX::XMFLOAT4X4 world_normal;
        DirectX::XMFLOAT3 camera_position;
        float exposure;
        DirectX::XMFLOAT4 light_direction[3];
        DirectX::XMFLOAT4 light_color[3];
        DirectX::XMFLOAT4 sky_color;
        DirectX::XMFLOAT4 ground_color;
        DirectX::XMFLOAT4X4 light_view_projection;
        DirectX::XMFLOAT4 shadow_params;
    };

    struct alignas(16) material_constants
    {
        DirectX::XMFLOAT4 base_color;
        DirectX::XMFLOAT3 emissive;
        float metallic;
        float roughness;
        float alpha_cutoff;
        float normal_scale;
        float occlusion_strength;
        std::uint32_t base_uv;
        std::uint32_t metal_rough_uv;
        std::uint32_t normal_uv;
        std::uint32_t occlusion_uv;
        std::uint32_t emissive_uv;
        std::uint32_t alpha_mode;
        std::uint32_t split_alpha;
        float glass_opacity;
    };

    struct alignas(16) part_constants
    {
        DirectX::XMFLOAT4 highlight;
        DirectX::XMFLOAT3 offset;
        std::uint32_t id;
    };

    struct alignas(16) pass_constants
    {
        std::uint32_t blend_pass;
        std::uint32_t padding[3];
    };

    static_assert(sizeof(frame_constants) == 416);
    static_assert(sizeof(material_constants) == 80);
    static_assert(sizeof(pass_constants) == 16);
    static_assert(sizeof(part_constants) == 32);
}
