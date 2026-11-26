#pragma once

#include "gpu_model.h"

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>

namespace menu::render
{
    struct lighting
    {
        float exposure = 1.0f;
        float ambient = 1.0f;
        bool neutral = false;
        float key_front = 0.80f;
    };

    struct camera
    {
        float yaw = 0.0f;
        float pitch = 0.35f;
        float zoom = 1.0f;
    };

    class scene
    {
    public:
        bool create(ID3D11Device* device);
        ID3D11ShaderResourceView* render(ID3D11DeviceContext* context, const gpu_model& model, const DirectX::XMFLOAT3& rotation_degrees, const camera& view, const lighting& light, std::uint32_t width, std::uint32_t height);
        void pick(ID3D11DeviceContext* context, const gpu_model& model, float x, float y);
        int picked(ID3D11DeviceContext* context);
        bool project(const DirectX::XMFLOAT3& point, DirectX::XMFLOAT2& uv) const;

    private:
        bool resize(std::uint32_t width, std::uint32_t height);

        Microsoft::WRL::ComPtr<ID3D11Device> device_;
        Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader_;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel_shader_;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> id_shader_;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> id_texture_;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> id_target_;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> id_depth_;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> id_staging_;
        bool pick_pending_ = false;
        int last_pick_ = -1;
        DirectX::XMFLOAT4X4 last_view_projection_{};
        Microsoft::WRL::ComPtr<ID3D11InputLayout> input_layout_;
        Microsoft::WRL::ComPtr<ID3D11Buffer> frame_buffer_;
        Microsoft::WRL::ComPtr<ID3D11Buffer> shadow_frame_buffer_;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> shadow_target_;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shadow_view_;
        Microsoft::WRL::ComPtr<ID3D11SamplerState> shadow_sampler_;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> shadow_raster_;
        Microsoft::WRL::ComPtr<ID3D11Buffer> opaque_pass_;
        Microsoft::WRL::ComPtr<ID3D11Buffer> blend_pass_;
        Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> cull_back_;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> cull_front_;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> cull_none_;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth_write_;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth_read_;
        Microsoft::WRL::ComPtr<ID3D11BlendState> opaque_;
        Microsoft::WRL::ComPtr<ID3D11BlendState> alpha_;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> color_;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> color_target_;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depth_target_;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> resolved_;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> resolved_view_;

        std::uint32_t width_ = 0;
        std::uint32_t height_ = 0;
        UINT samples_ = 1;
    };
}
