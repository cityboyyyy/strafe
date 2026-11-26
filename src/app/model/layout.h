#pragma once

#include "buttons.h"
#include "model.h"
#include "parts.h"

#include <DirectXMath.h>
#include <span>
#include <vector>

namespace menu::model
{
    struct part_rule
    {
        button target;
        const char* primitive;
        DirectX::XMFLOAT3 center;
    };

    struct part
    {
        button target;
        std::vector<std::uint32_t> triangles;
        DirectX::XMFLOAT3 center{};
    };

    std::span<const part_rule> layout_rules(const wchar_t* file);
    family layout_family(const wchar_t* file);
    std::vector<part> assign_parts(const model& source, std::span<const part_rule> rules, const DirectX::XMMATRIX& transform);
}
