#include "background.h"

#include <Windows.h>
#include <background_vs.h>
#include <background_smoke_ps.h>

#include <wrl/client.h>
#include <cstring>

using Microsoft::WRL::ComPtr;

namespace menu::render::background
{
    namespace
    {
        struct alignas(16) constants
        {
            float resolution[2];
            float time;
            float scale;
            float tint[4];
            float origin[2];
            float corner_radius;
            float padding;
            float mask_origin[2];
            float mask_size[2];
        };

        ComPtr<ID3D11VertexShader> g_vertex_shader;
        ComPtr<ID3D11PixelShader> g_pixel_shader;
        ComPtr<ID3D11Buffer> g_constants;
        float g_tint[4]{ 0.0f, 0.0f, 0.0f, 0.0f };
        float g_scale = 1.0f;
    }

    bool create(ID3D11Device* device)
    {
        if (FAILED(device->CreateVertexShader(g_background_vs, sizeof(g_background_vs), nullptr, &g_vertex_shader)))
            return false;
        if (FAILED(device->CreatePixelShader(g_background_smoke_ps, sizeof(g_background_smoke_ps), nullptr, &g_pixel_shader)))
            return false;

        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = sizeof(constants);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        return SUCCEEDED(device->CreateBuffer(&desc, nullptr, &g_constants));
    }

    void destroy()
    {
        g_constants.Reset();
        g_pixel_shader.Reset();
        g_vertex_shader.Reset();
    }

    void set_scale(float value)
    {
        g_scale = value;
    }

    void set_tint(float r, float g, float b, float strength)
    {
        g_tint[0] = r;
        g_tint[1] = g;
        g_tint[2] = b;
        g_tint[3] = strength;
    }

    void draw(ID3D11DeviceContext* context, float time, const area& field, const area& mask, float corner_radius)
    {
        ID3D11PixelShader* pixel_shader = g_pixel_shader.Get();
        if (!pixel_shader || field.width < 1.0f || field.height < 1.0f || mask.width < 1.0f || mask.height < 1.0f)
            return;

        constants data{};
        data.resolution[0] = field.width;
        data.resolution[1] = field.height;
        data.origin[0] = field.x;
        data.origin[1] = field.y;
        data.corner_radius = corner_radius;
        data.mask_origin[0] = mask.x;
        data.mask_origin[1] = mask.y;
        data.mask_size[0] = mask.width;
        data.mask_size[1] = mask.height;
        data.time = time;
        data.scale = g_scale;
        std::memcpy(data.tint, g_tint, sizeof(g_tint));

        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(context->Map(g_constants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            std::memcpy(mapped.pData, &data, sizeof(data));
            context->Unmap(g_constants.Get(), 0);
        }

        const D3D11_VIEWPORT viewport{ mask.x, mask.y, mask.width, mask.height, 0.0f, 1.0f };
        ID3D11Buffer* buffer = g_constants.Get();
        context->RSSetViewports(1, &viewport);
        context->RSSetState(nullptr);
        context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
        context->OMSetDepthStencilState(nullptr, 0);
        context->IASetInputLayout(nullptr);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(g_vertex_shader.Get(), nullptr, 0);
        context->PSSetShader(pixel_shader, nullptr, 0);
        context->PSSetConstantBuffers(0, 1, &buffer);
        context->GSSetShader(nullptr, nullptr, 0);
        context->Draw(3, 0);
    }
}
