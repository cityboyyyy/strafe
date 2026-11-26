#include "gpu_model.h"
#include "constants.h"

#include <cstring>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace menu::render
{
    namespace
    {
        constexpr std::uint32_t no_texture = 0xffffffffu;

        ComPtr<ID3D11ShaderResourceView> create_texture(ID3D11Device* device, ID3D11DeviceContext* context, const model::image& source)
        {
            if (source.pixels.empty())
                return nullptr;

            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = source.width;
            desc.Height = source.height;
            desc.MipLevels = 0;
            desc.ArraySize = 1;
            desc.Format = source.srgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
            desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

            ComPtr<ID3D11Texture2D> texture;
            if (FAILED(device->CreateTexture2D(&desc, nullptr, &texture)))
                return nullptr;

            context->UpdateSubresource(texture.Get(), 0, nullptr, source.pixels.data(), source.width * 4, 0);

            ComPtr<ID3D11ShaderResourceView> view;
            if (FAILED(device->CreateShaderResourceView(texture.Get(), nullptr, &view)))
                return nullptr;

            context->GenerateMips(view.Get());
            return view;
        }

        std::uint32_t uv_of(const model::texture_ref& ref, const std::vector<ComPtr<ID3D11ShaderResourceView>>& images)
        {
            if (ref.image < 0 || ref.image >= static_cast<int>(images.size()) || !images[ref.image])
                return no_texture;
            return static_cast<std::uint32_t>(ref.uv);
        }

        ID3D11ShaderResourceView* view_of(const model::texture_ref& ref, const std::vector<ComPtr<ID3D11ShaderResourceView>>& images)
        {
            if (ref.image < 0 || ref.image >= static_cast<int>(images.size()))
                return nullptr;
            return images[ref.image].Get();
        }

        ComPtr<ID3D11Buffer> create_buffer(ID3D11Device* device, UINT bind, const void* data, std::size_t size)
        {
            D3D11_BUFFER_DESC desc{};
            desc.ByteWidth = static_cast<UINT>(size);
            desc.Usage = D3D11_USAGE_IMMUTABLE;
            desc.BindFlags = bind;

            D3D11_SUBRESOURCE_DATA init{};
            init.pSysMem = data;

            ComPtr<ID3D11Buffer> buffer;
            device->CreateBuffer(&desc, &init, &buffer);
            return buffer;
        }
    }

    bool gpu_model::upload(ID3D11Device* device, ID3D11DeviceContext* context, const model::model& source, std::span<const model::part> parts)
    {
        vertex_buffer_ = create_buffer(device, D3D11_BIND_VERTEX_BUFFER, source.vertices.data(), source.vertices.size() * sizeof(model::vertex));
        if (!vertex_buffer_)
            return false;

        images_.clear();
        images_.reserve(source.images.size());
        for (const auto& image : source.images)
            images_.push_back(create_texture(device, context, image));

        std::vector<model::material> materials = source.materials;
        const int fallback = static_cast<int>(materials.size());
        materials.push_back(model::material{ .metallic = 0.0f, .roughness = 0.6f });

        materials_.clear();
        materials_.reserve(materials.size());
        for (const auto& m : materials)
        {
            material_constants constants{};
            constants.base_color = m.base_color;
            constants.base_color.w *= 1.0f - 0.85f * m.transmission;
            constants.emissive = m.emissive;
            constants.metallic = m.metallic;
            constants.roughness = m.roughness;
            constants.alpha_cutoff = m.alpha_cutoff;
            constants.normal_scale = m.normal_scale;
            constants.occlusion_strength = m.occlusion_strength;
            constants.base_uv = uv_of(m.base_texture, images_);
            constants.metal_rough_uv = uv_of(m.metal_rough_texture, images_);
            constants.normal_uv = uv_of(m.normal_texture, images_);
            constants.occlusion_uv = uv_of(m.occlusion_texture, images_);
            constants.emissive_uv = uv_of(m.emissive_texture, images_);
            constants.alpha_mode = static_cast<std::uint32_t>(m.alpha);
            constants.split_alpha = m.split_alpha ? 1u : 0u;
            constants.glass_opacity = m.glass_opacity;

            material out;
            out.constants = create_buffer(device, D3D11_BIND_CONSTANT_BUFFER, &constants, sizeof(constants));
            out.textures = {
                view_of(m.base_texture, images_),
                view_of(m.metal_rough_texture, images_),
                view_of(m.normal_texture, images_),
                view_of(m.occlusion_texture, images_),
                view_of(m.emissive_texture, images_),
            };
            out.alpha = m.alpha;
            out.double_sided = m.double_sided;
            out.split_alpha = m.split_alpha;
            materials_.push_back(out);
        }

        std::vector<int> owner(source.indices.size() / 3, -1);
        for (std::size_t p = 0; p < parts.size(); ++p)
        {
            for (const std::uint32_t first : parts[p].triangles)
                owner[first / 3] = static_cast<int>(p);
        }

        std::vector<std::uint32_t> indices;
        indices.reserve(source.indices.size());
        ranges_.clear();
        for (const auto& primitive : source.primitives)
        {
            const int material_index = primitive.material < 0 || primitive.material >= fallback ? fallback : primitive.material;
            for (int part = -1; part < static_cast<int>(parts.size()); ++part)
            {
                const auto start = static_cast<std::uint32_t>(indices.size());
                for (std::uint32_t t = primitive.first_index; t + 2 < primitive.first_index + primitive.index_count; t += 3)
                {
                    if (owner[t / 3] != part)
                        continue;
                    indices.push_back(source.indices[t]);
                    indices.push_back(source.indices[t + 1]);
                    indices.push_back(source.indices[t + 2]);
                }
                const auto count = static_cast<std::uint32_t>(indices.size()) - start;
                if (count > 0)
                    ranges_.push_back(range{ start, count, material_index, part });
            }
        }

        index_buffer_ = create_buffer(device, D3D11_BIND_INDEX_BUFFER, indices.data(), indices.size() * sizeof(std::uint32_t));
        if (!index_buffer_)
            return false;

        part_constants none{};
        base_part_buffer_ = create_buffer(device, D3D11_BIND_CONSTANT_BUFFER, &none, sizeof(none));

        part_buffers_.clear();
        part_buttons_.clear();
        part_centers_.clear();
        for (std::size_t p = 0; p < parts.size(); ++p)
        {
            D3D11_BUFFER_DESC desc{};
            desc.ByteWidth = sizeof(part_constants);
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            part_constants initial{};
            initial.id = static_cast<std::uint32_t>(p + 1);
            D3D11_SUBRESOURCE_DATA init{ &initial, 0, 0 };
            ComPtr<ID3D11Buffer> buffer;
            device->CreateBuffer(&desc, &init, &buffer);
            part_buffers_.push_back(buffer);
            part_buttons_.push_back(parts[p].target);
            part_centers_.push_back(parts[p].center);
        }

        const XMVECTOR low = XMLoadFloat3(&source.bounds_min);
        const XMVECTOR high = XMLoadFloat3(&source.bounds_max);
        XMStoreFloat3(&center_, (low + high) * 0.5f);
        radius_ = (std::max)(XMVectorGetX(XMVector3Length(high - low)) * 0.5f, 1e-4f);
        return true;
    }

    void gpu_model::set_part(ID3D11DeviceContext* context, std::size_t part, const XMFLOAT4& highlight, const XMFLOAT3& offset)
    {
        if (part >= part_buffers_.size() || !part_buffers_[part])
            return;

        part_constants data{};
        data.highlight = highlight;
        data.offset = offset;
        data.id = static_cast<std::uint32_t>(part + 1);

        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(context->Map(part_buffers_[part].Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            std::memcpy(mapped.pData, &data, sizeof(data));
            context->Unmap(part_buffers_[part].Get(), 0);
        }
    }

    void gpu_model::bind_geometry(ID3D11DeviceContext* context) const
    {
        const UINT stride = sizeof(model::vertex);
        const UINT offset = 0;
        ID3D11Buffer* vertex_buffer = vertex_buffer_.Get();
        context->IASetVertexBuffers(0, 1, &vertex_buffer, &stride, &offset);
        context->IASetIndexBuffer(index_buffer_.Get(), DXGI_FORMAT_R32_UINT, 0);
    }

    void gpu_model::bind_part(ID3D11DeviceContext* context, int part) const
    {
        ID3D11Buffer* buffer = part >= 0 ? part_buffers_[part].Get() : base_part_buffer_.Get();
        context->VSSetConstantBuffers(3, 1, &buffer);
        context->PSSetConstantBuffers(3, 1, &buffer);
    }

    void gpu_model::draw(ID3D11DeviceContext* context, bool blended, const raster_states& states) const
    {
        bind_geometry(context);
        for (const auto& r : ranges_)
        {
            const material& m = materials_[r.material];
            if (!m.split_alpha && (m.alpha == model::alpha_mode::blend) != blended)
                continue;

            ID3D11Buffer* constants = m.constants.Get();
            context->PSSetConstantBuffers(1, 1, &constants);
            context->PSSetShaderResources(0, static_cast<UINT>(m.textures.size()), m.textures.data());
            bind_part(context, r.part);
            if (blended && m.double_sided)
            {
                context->RSSetState(states.cull_front);
                context->DrawIndexed(r.count, r.first, 0);
                context->RSSetState(states.cull_back);
                context->DrawIndexed(r.count, r.first, 0);
                continue;
            }

            context->RSSetState(m.double_sided ? states.cull_none : states.cull_back);
            context->DrawIndexed(r.count, r.first, 0);
        }
    }

    void gpu_model::draw_shadow_casters(ID3D11DeviceContext* context) const
    {
        bind_geometry(context);
        for (const auto& r : ranges_)
        {
            const material& m = materials_[r.material];
            if (m.alpha == model::alpha_mode::blend || m.split_alpha)
                continue;
            bind_part(context, r.part);
            context->DrawIndexed(r.count, r.first, 0);
        }
    }

    void gpu_model::draw_ids(ID3D11DeviceContext* context) const
    {
        bind_geometry(context);
        for (const auto& r : ranges_)
        {
            bind_part(context, r.part);
            context->DrawIndexed(r.count, r.first, 0);
        }
    }
}
