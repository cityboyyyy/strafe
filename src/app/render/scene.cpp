#include "scene.h"
#include "constants.h"

#include <Windows.h>
#include <model_vs.h>
#include <model_ps.h>
#include <model_id_ps.h>

#include <algorithm>
#include <cmath>
#include <cstring>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace menu::render
{
    namespace
    {
        constexpr float field_of_view = XMConvertToRadians(30.0f);
        constexpr float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        constexpr UINT shadow_size = 2048;

        XMFLOAT4 direction(XMVECTOR v)
        {
            XMFLOAT4 out;
            XMStoreFloat4(&out, XMVector3Normalize(v));
            return out;
        }
    }

    bool scene::create(ID3D11Device* device)
    {
        device_ = device;

        if (FAILED(device->CreateVertexShader(g_model_vs, sizeof(g_model_vs), nullptr, &vertex_shader_)))
            return false;
        if (FAILED(device->CreatePixelShader(g_model_ps, sizeof(g_model_ps), nullptr, &pixel_shader_)))
            return false;
        if (FAILED(device->CreatePixelShader(g_model_id_ps, sizeof(g_model_id_ps), nullptr, &id_shader_)))
            return false;

        D3D11_TEXTURE2D_DESC id_desc{};
        id_desc.Width = 1;
        id_desc.Height = 1;
        id_desc.MipLevels = 1;
        id_desc.ArraySize = 1;
        id_desc.Format = DXGI_FORMAT_R32_UINT;
        id_desc.SampleDesc.Count = 1;
        id_desc.Usage = D3D11_USAGE_DEFAULT;
        id_desc.BindFlags = D3D11_BIND_RENDER_TARGET;
        if (FAILED(device->CreateTexture2D(&id_desc, nullptr, &id_texture_)) || FAILED(device->CreateRenderTargetView(id_texture_.Get(), nullptr, &id_target_)))
            return false;

        id_desc.Usage = D3D11_USAGE_STAGING;
        id_desc.BindFlags = 0;
        id_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        if (FAILED(device->CreateTexture2D(&id_desc, nullptr, &id_staging_)))
            return false;

        id_desc.Format = DXGI_FORMAT_D32_FLOAT;
        id_desc.Usage = D3D11_USAGE_DEFAULT;
        id_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        id_desc.CPUAccessFlags = 0;
        ComPtr<ID3D11Texture2D> id_depth;
        if (FAILED(device->CreateTexture2D(&id_desc, nullptr, &id_depth)) || FAILED(device->CreateDepthStencilView(id_depth.Get(), nullptr, &id_depth_)))
            return false;

        const D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        if (FAILED(device->CreateInputLayout(layout, ARRAYSIZE(layout), g_model_vs, sizeof(g_model_vs), &input_layout_)))
            return false;

        D3D11_BUFFER_DESC buffer{};
        buffer.ByteWidth = sizeof(frame_constants);
        buffer.Usage = D3D11_USAGE_DYNAMIC;
        buffer.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        buffer.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(device->CreateBuffer(&buffer, nullptr, &frame_buffer_)))
            return false;
        if (FAILED(device->CreateBuffer(&buffer, nullptr, &shadow_frame_buffer_)))
            return false;

        D3D11_TEXTURE2D_DESC shadow_desc{};
        shadow_desc.Width = shadow_size;
        shadow_desc.Height = shadow_size;
        shadow_desc.MipLevels = 1;
        shadow_desc.ArraySize = 1;
        shadow_desc.Format = DXGI_FORMAT_R32_TYPELESS;
        shadow_desc.SampleDesc.Count = 1;
        shadow_desc.Usage = D3D11_USAGE_DEFAULT;
        shadow_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
        ComPtr<ID3D11Texture2D> shadow_texture;
        if (FAILED(device->CreateTexture2D(&shadow_desc, nullptr, &shadow_texture)))
            return false;

        D3D11_DEPTH_STENCIL_VIEW_DESC shadow_dsv{};
        shadow_dsv.Format = DXGI_FORMAT_D32_FLOAT;
        shadow_dsv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        if (FAILED(device->CreateDepthStencilView(shadow_texture.Get(), &shadow_dsv, &shadow_target_)))
            return false;

        D3D11_SHADER_RESOURCE_VIEW_DESC shadow_srv{};
        shadow_srv.Format = DXGI_FORMAT_R32_FLOAT;
        shadow_srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        shadow_srv.Texture2D.MipLevels = 1;
        if (FAILED(device->CreateShaderResourceView(shadow_texture.Get(), &shadow_srv, &shadow_view_)))
            return false;

        D3D11_SAMPLER_DESC compare{};
        compare.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        compare.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
        compare.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
        compare.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
        compare.BorderColor[0] = 1.0f;
        compare.BorderColor[1] = 1.0f;
        compare.BorderColor[2] = 1.0f;
        compare.BorderColor[3] = 1.0f;
        compare.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
        compare.MaxLOD = D3D11_FLOAT32_MAX;
        if (FAILED(device->CreateSamplerState(&compare, &shadow_sampler_)))
            return false;

        D3D11_RASTERIZER_DESC shadow_raster{};
        shadow_raster.FillMode = D3D11_FILL_SOLID;
        shadow_raster.CullMode = D3D11_CULL_NONE;
        shadow_raster.DepthClipEnable = TRUE;
        shadow_raster.SlopeScaledDepthBias = 1.5f;
        shadow_raster.DepthBiasClamp = 0.01f;
        if (FAILED(device->CreateRasterizerState(&shadow_raster, &shadow_raster_)))
            return false;

        for (std::uint32_t pass : { 0u, 1u })
        {
            const pass_constants constants{ pass, {} };
            D3D11_BUFFER_DESC pass_desc{};
            pass_desc.ByteWidth = sizeof(pass_constants);
            pass_desc.Usage = D3D11_USAGE_IMMUTABLE;
            pass_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            D3D11_SUBRESOURCE_DATA init{ &constants, 0, 0 };
            if (FAILED(device->CreateBuffer(&pass_desc, &init, pass ? &blend_pass_ : &opaque_pass_)))
                return false;
        }

        D3D11_SAMPLER_DESC sampler{};
        sampler.Filter = D3D11_FILTER_ANISOTROPIC;
        sampler.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        sampler.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        sampler.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        sampler.MaxAnisotropy = 16;
        sampler.MaxLOD = D3D11_FLOAT32_MAX;
        if (FAILED(device->CreateSamplerState(&sampler, &sampler_)))
            return false;

        D3D11_RASTERIZER_DESC raster{};
        raster.FillMode = D3D11_FILL_SOLID;
        raster.CullMode = D3D11_CULL_BACK;
        raster.FrontCounterClockwise = TRUE;
        raster.DepthClipEnable = TRUE;
        raster.MultisampleEnable = TRUE;
        if (FAILED(device->CreateRasterizerState(&raster, &cull_back_)))
            return false;
        raster.CullMode = D3D11_CULL_FRONT;
        if (FAILED(device->CreateRasterizerState(&raster, &cull_front_)))
            return false;
        raster.CullMode = D3D11_CULL_NONE;
        if (FAILED(device->CreateRasterizerState(&raster, &cull_none_)))
            return false;

        D3D11_DEPTH_STENCIL_DESC depth{};
        depth.DepthEnable = TRUE;
        depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        depth.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        if (FAILED(device->CreateDepthStencilState(&depth, &depth_write_)))
            return false;
        depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        if (FAILED(device->CreateDepthStencilState(&depth, &depth_read_)))
            return false;

        D3D11_BLEND_DESC blend{};
        blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        if (FAILED(device->CreateBlendState(&blend, &opaque_)))
            return false;
        blend.RenderTarget[0].BlendEnable = TRUE;
        blend.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blend.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        blend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        if (FAILED(device->CreateBlendState(&blend, &alpha_)))
            return false;

        for (UINT count : { 4u, 2u })
        {
            UINT quality = 0;
            if (SUCCEEDED(device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, count, &quality)) && quality > 0)
            {
                samples_ = count;
                break;
            }
        }
        return true;
    }

    bool scene::resize(std::uint32_t width, std::uint32_t height)
    {
        if (width == width_ && height == height_ && resolved_view_)
            return true;

        color_target_.Reset();
        depth_target_.Reset();
        color_.Reset();
        resolved_view_.Reset();
        resolved_.Reset();
        width_ = width;
        height_ = height;

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
        desc.SampleDesc.Count = samples_;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET;
        if (FAILED(device_->CreateTexture2D(&desc, nullptr, &color_)))
            return false;

        D3D11_RENDER_TARGET_VIEW_DESC rtv{};
        rtv.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        rtv.ViewDimension = samples_ > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;
        if (FAILED(device_->CreateRenderTargetView(color_.Get(), &rtv, &color_target_)))
            return false;

        desc.Format = DXGI_FORMAT_D32_FLOAT;
        desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        ComPtr<ID3D11Texture2D> depth;
        if (FAILED(device_->CreateTexture2D(&desc, nullptr, &depth)))
            return false;
        if (FAILED(device_->CreateDepthStencilView(depth.Get(), nullptr, &depth_target_)))
            return false;

        desc.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
        desc.SampleDesc.Count = 1;
        desc.MipLevels = 0;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
        if (FAILED(device_->CreateTexture2D(&desc, nullptr, &resolved_)))
            return false;

        D3D11_SHADER_RESOURCE_VIEW_DESC srv{};
        srv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srv.Texture2D.MipLevels = static_cast<UINT>(-1);
        return SUCCEEDED(device_->CreateShaderResourceView(resolved_.Get(), &srv, &resolved_view_));
    }

    ID3D11ShaderResourceView* scene::render(ID3D11DeviceContext* context, const gpu_model& model, const XMFLOAT3& rotation_degrees, const camera& view, const lighting& light, std::uint32_t width, std::uint32_t height)
    {
        if (width == 0 || height == 0 || !resize(width, height))
            return nullptr;

        const float inverse_radius = 1.0f / model.radius();
        const XMMATRIX world =
            XMMatrixTranslation(-model.center().x, -model.center().y, -model.center().z) *
            XMMatrixScaling(inverse_radius, inverse_radius, inverse_radius) *
            XMMatrixRotationRollPitchYaw(XMConvertToRadians(rotation_degrees.x), XMConvertToRadians(rotation_degrees.y), XMConvertToRadians(rotation_degrees.z));

        const float distance = (1.0f / std::sin(field_of_view * 0.5f)) * 0.82f / view.zoom;
        const XMVECTOR eye = XMVectorSet(
            distance * std::cos(view.pitch) * std::sin(view.yaw),
            distance * std::sin(view.pitch),
            distance * std::cos(view.pitch) * std::cos(view.yaw),
            1.0f);
        const XMVECTOR target = XMVectorZero();
        const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

        D3D11_MAPPED_SUBRESOURCE mapped{};
        const float aspect = static_cast<float>(width) / static_cast<float>(height);
        const XMMATRIX view_matrix = XMMatrixLookAtRH(eye, target, up);
        const XMMATRIX projection = XMMatrixPerspectiveFovRH(field_of_view, aspect, 0.05f, distance + 4.0f);

        const XMVECTOR forward = XMVector3Normalize(target - eye);
        const XMVECTOR right = XMVector3Normalize(XMVector3Cross(forward, up));
        const XMVECTOR camera_up = XMVector3Cross(right, forward);

        frame_constants frame{};
        XMStoreFloat4x4(&frame.view_projection, XMMatrixTranspose(view_matrix * projection));
        XMStoreFloat4x4(&last_view_projection_, view_matrix * projection);
        XMStoreFloat4x4(&frame.world, XMMatrixTranspose(world));
        XMStoreFloat4x4(&frame.world_normal, XMMatrixInverse(nullptr, world));
        XMStoreFloat3(&frame.camera_position, eye);
        frame.exposure = light.exposure;
        frame.light_direction[0] = direction(-forward * light.key_front + camera_up * 1.10f - right * 1.05f);
        frame.light_direction[1] = direction(-forward * 0.78f + right * 0.85f + camera_up * 0.12f);
        frame.light_direction[2] = direction(forward * 1.20f + camera_up * 0.73f + right * 0.65f);
        frame.light_color[0] = { 3.5f, 3.3f, 3.10f, 0.0f };
        frame.light_color[1] = { 0.82f, 0.96f, 1.16f, 0.0f };
        frame.light_color[2] = { 1.75f, 2.25f, 3.30f, 0.0f };
        frame.sky_color = { 0.40f * light.ambient, 0.43f * light.ambient, 0.50f * light.ambient, 0.0f };
        frame.ground_color = { 0.11f * light.ambient, 0.13f * light.ambient, 0.17f * light.ambient, 0.0f };

        const XMVECTOR key = XMLoadFloat4(&frame.light_direction[0]);
        const XMVECTOR light_up = std::abs(XMVectorGetY(key)) > 0.95f ? XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f) : up;
        const XMMATRIX light_matrix = XMMatrixLookAtRH(key * 4.0f, target, light_up) * XMMatrixOrthographicRH(2.3f, 2.3f, 1.5f, 6.5f);
        XMStoreFloat4x4(&frame.light_view_projection, XMMatrixTranspose(light_matrix));
        frame.shadow_params = { 1.0f / static_cast<float>(shadow_size), 0.012f, 0.0008f, light.neutral ? 1.0f : 0.0f };

        frame_constants shadow_frame = frame;
        XMStoreFloat4x4(&shadow_frame.view_projection, XMMatrixTranspose(light_matrix));
        if (SUCCEEDED(context->Map(shadow_frame_buffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            std::memcpy(mapped.pData, &shadow_frame, sizeof(shadow_frame));
            context->Unmap(shadow_frame_buffer_.Get(), 0);
        }

        if (SUCCEEDED(context->Map(frame_buffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            std::memcpy(mapped.pData, &frame, sizeof(frame));
            context->Unmap(frame_buffer_.Get(), 0);
        }

        ID3D11ShaderResourceView* no_shadow = nullptr;
        context->PSSetShaderResources(5, 1, &no_shadow);
        context->OMSetRenderTargets(0, nullptr, shadow_target_.Get());
        context->ClearDepthStencilView(shadow_target_.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
        const D3D11_VIEWPORT shadow_viewport{ 0.0f, 0.0f, static_cast<float>(shadow_size), static_cast<float>(shadow_size), 0.0f, 1.0f };
        context->RSSetViewports(1, &shadow_viewport);
        context->RSSetState(shadow_raster_.Get());
        context->IASetInputLayout(input_layout_.Get());
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D11Buffer* shadow_buffer = shadow_frame_buffer_.Get();
        context->VSSetShader(vertex_shader_.Get(), nullptr, 0);
        context->VSSetConstantBuffers(0, 1, &shadow_buffer);
        context->PSSetShader(nullptr, nullptr, 0);
        context->GSSetShader(nullptr, nullptr, 0);
        context->OMSetDepthStencilState(depth_write_.Get(), 0);
        context->OMSetBlendState(opaque_.Get(), nullptr, 0xffffffff);
        model.draw_shadow_casters(context);

        ID3D11RenderTargetView* target_view = color_target_.Get();
        context->OMSetRenderTargets(1, &target_view, depth_target_.Get());
        context->ClearRenderTargetView(target_view, clear_color);
        context->ClearDepthStencilView(depth_target_.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

        D3D11_VIEWPORT viewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
        context->RSSetViewports(1, &viewport);

        ID3D11Buffer* frame_buffer = frame_buffer_.Get();
        ID3D11SamplerState* sampler = sampler_.Get();
        context->IASetInputLayout(input_layout_.Get());
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(vertex_shader_.Get(), nullptr, 0);
        context->VSSetConstantBuffers(0, 1, &frame_buffer);
        context->PSSetShader(pixel_shader_.Get(), nullptr, 0);
        context->PSSetConstantBuffers(0, 1, &frame_buffer);
        context->PSSetSamplers(0, 1, &sampler);
        ID3D11SamplerState* shadow_sampler = shadow_sampler_.Get();
        ID3D11ShaderResourceView* shadow_view = shadow_view_.Get();
        context->PSSetSamplers(1, 1, &shadow_sampler);
        context->PSSetShaderResources(5, 1, &shadow_view);
        context->GSSetShader(nullptr, nullptr, 0);
        const gpu_model::raster_states states{ cull_back_.Get(), cull_front_.Get(), cull_none_.Get() };

        ID3D11Buffer* opaque_pass = opaque_pass_.Get();
        context->PSSetConstantBuffers(2, 1, &opaque_pass);
        context->OMSetDepthStencilState(depth_write_.Get(), 0);
        context->OMSetBlendState(opaque_.Get(), nullptr, 0xffffffff);
        model.draw(context, false, states);

        ID3D11Buffer* blend_pass = blend_pass_.Get();
        context->PSSetConstantBuffers(2, 1, &blend_pass);
        context->OMSetDepthStencilState(depth_read_.Get(), 0);
        context->OMSetBlendState(alpha_.Get(), nullptr, 0xffffffff);
        model.draw(context, true, states);

        ID3D11ShaderResourceView* no_views[6]{};
        context->PSSetShaderResources(0, 6, no_views);
        context->OMSetRenderTargets(0, nullptr, nullptr);

        if (samples_ > 1)
            context->ResolveSubresource(resolved_.Get(), 0, color_.Get(), 0, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
        else
            context->CopySubresourceRegion(resolved_.Get(), 0, 0, 0, 0, color_.Get(), 0, nullptr);
        context->GenerateMips(resolved_view_.Get());

        return resolved_view_.Get();
    }

    void scene::pick(ID3D11DeviceContext* context, const gpu_model& model, float x, float y)
    {
        picked(context);
        if (pick_pending_)
            return;

        if (!resolved_view_ || x < 0.0f || y < 0.0f || x >= static_cast<float>(width_) || y >= static_cast<float>(height_))
        {
            pick_pending_ = false;
            last_pick_ = -1;
            return;
        }

        constexpr float empty[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        ID3D11RenderTargetView* target = id_target_.Get();
        context->OMSetRenderTargets(1, &target, id_depth_.Get());
        context->ClearRenderTargetView(target, empty);
        context->ClearDepthStencilView(id_depth_.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

        const D3D11_VIEWPORT viewport{ -std::floor(x), -std::floor(y), static_cast<float>(width_), static_cast<float>(height_), 0.0f, 1.0f };
        context->RSSetViewports(1, &viewport);
        context->RSSetState(cull_none_.Get());

        ID3D11Buffer* frame_buffer = frame_buffer_.Get();
        context->IASetInputLayout(input_layout_.Get());
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(vertex_shader_.Get(), nullptr, 0);
        context->VSSetConstantBuffers(0, 1, &frame_buffer);
        context->PSSetShader(id_shader_.Get(), nullptr, 0);
        context->OMSetDepthStencilState(depth_write_.Get(), 0);
        context->OMSetBlendState(opaque_.Get(), nullptr, 0xffffffff);
        model.draw_ids(context);
        context->OMSetRenderTargets(0, nullptr, nullptr);

        context->CopyResource(id_staging_.Get(), id_texture_.Get());
        pick_pending_ = true;
    }

    int scene::picked(ID3D11DeviceContext* context)
    {
        if (!pick_pending_)
            return last_pick_;

        D3D11_MAPPED_SUBRESOURCE mapped{};
        const HRESULT hr = context->Map(id_staging_.Get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped);
        if (hr == DXGI_ERROR_WAS_STILL_DRAWING || FAILED(hr))
            return last_pick_;

        const std::uint32_t value = *static_cast<const std::uint32_t*>(mapped.pData);
        context->Unmap(id_staging_.Get(), 0);
        pick_pending_ = false;
        last_pick_ = static_cast<int>(value) - 1;
        return last_pick_;
    }

    bool scene::project(const XMFLOAT3& point, XMFLOAT2& uv) const
    {
        const XMVECTOR clip = XMVector4Transform(XMVectorSet(point.x, point.y, point.z, 1.0f), XMLoadFloat4x4(&last_view_projection_));
        const float w = XMVectorGetW(clip);
        if (w <= 1e-5f)
            return false;
        uv.x = XMVectorGetX(clip) / w * 0.5f + 0.5f;
        uv.y = 0.5f - XMVectorGetY(clip) / w * 0.5f;
        return true;
    }
}
