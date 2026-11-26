#pragma once

#include <d3d11.h>
#include <cstdint>

namespace menu::render::background
{
    struct area
    {
        float x;
        float y;
        float width;
        float height;
    };

    bool create(ID3D11Device* device);
    void destroy();
    void set_scale(float value);
    void set_tint(float r, float g, float b, float strength);
    void draw(ID3D11DeviceContext* context, float time, const area& field, const area& mask, float corner_radius);
}
