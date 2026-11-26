#pragma once

#include "model.h"

#include <DirectXMath.h>
#include <cstdint>
#include <vector>

namespace menu::model
{
    struct island
    {
        std::uint32_t primitive = 0;
        std::vector<std::uint32_t> triangles;
        DirectX::XMFLOAT3 center{};
        DirectX::XMFLOAT3 size{};
    };

    DirectX::XMMATRIX display_transform(const model& source, const DirectX::XMFLOAT3& rotation_degrees);
    std::vector<island> find_islands(const model& source, std::uint32_t primitive, const DirectX::XMMATRIX& transform);
}
