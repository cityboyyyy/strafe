#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <cstdint>

namespace menu::render
{
    bool create(HWND hwnd);
    void destroy();
    void resize(std::uint32_t width, std::uint32_t height);
    bool occluded();
    void begin(const float clear[4]);
    void present();
    ID3D11Device* device();
    ID3D11DeviceContext* context();
    ID3D11SamplerState* mip_sampler();
}
