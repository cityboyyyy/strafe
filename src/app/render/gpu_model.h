#pragma once

#include "../model/buttons.h"
#include "../model/layout.h"
#include "../model/model.h"

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <array>
#include <span>
#include <vector>

namespace menu::render
{
    class gpu_model
    {
    public:
        struct raster_states
        {
            ID3D11RasterizerState* cull_back;
            ID3D11RasterizerState* cull_front;
            ID3D11RasterizerState* cull_none;
        };

        bool upload(ID3D11Device* device, ID3D11DeviceContext* context, const model::model& source, std::span<const model::part> parts);
        void set_part(ID3D11DeviceContext* context, std::size_t part, const DirectX::XMFLOAT4& highlight, const DirectX::XMFLOAT3& offset);

        void draw(ID3D11DeviceContext* context, bool blended, const raster_states& states) const;
        void draw_shadow_casters(ID3D11DeviceContext* context) const;
        void draw_ids(ID3D11DeviceContext* context) const;

        const DirectX::XMFLOAT3& center() const { return center_; }
        float radius() const { return radius_; }
        std::size_t part_count() const { return part_buttons_.size(); }
        model::button part_button(std::size_t part) const { return part_buttons_[part]; }
        const DirectX::XMFLOAT3& part_center(std::size_t part) const { return part_centers_[part]; }
        void set_display_bounds(const DirectX::XMFLOAT3& low, const DirectX::XMFLOAT3& high) { display_low_ = low; display_high_ = high; }
        const DirectX::XMFLOAT3& display_low() const { return display_low_; }
        const DirectX::XMFLOAT3& display_high() const { return display_high_; }

    private:
        struct material
        {
            Microsoft::WRL::ComPtr<ID3D11Buffer> constants;
            std::array<ID3D11ShaderResourceView*, 5> textures{};
            model::alpha_mode alpha = model::alpha_mode::opaque;
            bool double_sided = false;
            bool split_alpha = false;
        };

        struct range
        {
            std::uint32_t first = 0;
            std::uint32_t count = 0;
            int material = 0;
            int part = -1;
        };

        void bind_geometry(ID3D11DeviceContext* context) const;
        void bind_part(ID3D11DeviceContext* context, int part) const;

        Microsoft::WRL::ComPtr<ID3D11Buffer> vertex_buffer_;
        Microsoft::WRL::ComPtr<ID3D11Buffer> index_buffer_;
        Microsoft::WRL::ComPtr<ID3D11Buffer> base_part_buffer_;
        std::vector<Microsoft::WRL::ComPtr<ID3D11Buffer>> part_buffers_;
        std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> images_;
        std::vector<material> materials_;
        std::vector<range> ranges_;
        std::vector<model::button> part_buttons_;
        std::vector<DirectX::XMFLOAT3> part_centers_;
        DirectX::XMFLOAT3 center_{};
        float radius_ = 1.0f;
        DirectX::XMFLOAT3 display_low_{ -1.0f, -1.0f, -1.0f };
        DirectX::XMFLOAT3 display_high_{ 1.0f, 1.0f, 1.0f };
    };
}
