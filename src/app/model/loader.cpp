#include "loader.h"
#include "image_decoder.h"

#include <cgltf.h>

#include <Windows.h>
#include <objbase.h>
#include <algorithm>
#include <atomic>
#include <thread>

using namespace DirectX;

namespace menu::model
{
    namespace
    {
        struct gltf_handle
        {
            cgltf_data* data = nullptr;
            ~gltf_handle()
            {
                if (data)
                    cgltf_free(data);
            }
        };

        texture_ref to_ref(const cgltf_data* data, const cgltf_texture_view& view, std::vector<image>& images, bool srgb)
        {
            texture_ref ref;
            if (!view.texture || !view.texture->image)
                return ref;

            ref.image = static_cast<int>(cgltf_image_index(data, view.texture->image));
            ref.uv = (std::min)(static_cast<int>(view.texcoord), 2);
            if (srgb)
                images[ref.image].srgb = true;
            return ref;
        }

        material to_material(const cgltf_data* data, const cgltf_material& source, std::vector<image>& images)
        {
            material out;
            if (source.has_pbr_metallic_roughness)
            {
                const auto& pbr = source.pbr_metallic_roughness;
                out.base_color = { pbr.base_color_factor[0], pbr.base_color_factor[1], pbr.base_color_factor[2], pbr.base_color_factor[3] };
                out.metallic = pbr.metallic_factor;
                out.roughness = pbr.roughness_factor;
                out.base_texture = to_ref(data, pbr.base_color_texture, images, true);
                out.metal_rough_texture = to_ref(data, pbr.metallic_roughness_texture, images, false);
            }

            const float strength = source.has_emissive_strength ? source.emissive_strength.emissive_strength : 1.0f;
            out.emissive = { source.emissive_factor[0] * strength, source.emissive_factor[1] * strength, source.emissive_factor[2] * strength };
            out.emissive_texture = to_ref(data, source.emissive_texture, images, true);
            out.normal_texture = to_ref(data, source.normal_texture, images, false);
            out.normal_scale = source.normal_texture.texture ? source.normal_texture.scale : 1.0f;
            out.occlusion_texture = to_ref(data, source.occlusion_texture, images, false);
            out.occlusion_strength = source.occlusion_texture.texture ? source.occlusion_texture.scale : 1.0f;
            out.alpha_cutoff = source.alpha_cutoff;
            out.double_sided = source.double_sided;
            out.transmission = source.has_transmission ? source.transmission.transmission_factor : 0.0f;

            switch (source.alpha_mode)
            {
            case cgltf_alpha_mode_mask:
                out.alpha = alpha_mode::mask;
                break;
            case cgltf_alpha_mode_blend:
                out.alpha = alpha_mode::blend;
                break;
            default:
                out.alpha = alpha_mode::opaque;
                break;
            }
            return out;
        }

        void add_primitive(const cgltf_data* data, const cgltf_node& node, const cgltf_primitive& source, const XMMATRIX& world, model& out)
        {
            if (source.type != cgltf_primitive_type_triangles)
                return;

            const cgltf_accessor* positions = nullptr;
            const cgltf_accessor* normals = nullptr;
            const cgltf_accessor* uvs[3]{};

            for (cgltf_size i = 0; i < source.attributes_count; ++i)
            {
                const cgltf_attribute& attribute = source.attributes[i];
                if (attribute.type == cgltf_attribute_type_position)
                    positions = attribute.data;
                else if (attribute.type == cgltf_attribute_type_normal)
                    normals = attribute.data;
                else if (attribute.type == cgltf_attribute_type_texcoord && attribute.index >= 0 && attribute.index < 3)
                    uvs[attribute.index] = attribute.data;
            }

            if (!positions || positions->count == 0)
                return;

            const XMMATRIX normal_matrix = XMMatrixTranspose(XMMatrixInverse(nullptr, world));
            const bool flipped = XMVectorGetX(XMMatrixDeterminant(world)) < 0.0f;
            const auto base = static_cast<std::uint32_t>(out.vertices.size());
            const auto count = static_cast<std::uint32_t>(positions->count);

            out.vertices.resize(base + count);
            for (std::uint32_t i = 0; i < count; ++i)
            {
                vertex& v = out.vertices[base + i];

                float p[3]{};
                cgltf_accessor_read_float(positions, i, p, 3);
                XMStoreFloat3(&v.position, XMVector3TransformCoord(XMVectorSet(p[0], p[1], p[2], 1.0f), world));

                if (normals)
                {
                    float n[3]{};
                    cgltf_accessor_read_float(normals, i, n, 3);
                    XMStoreFloat3(&v.normal, XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(n[0], n[1], n[2], 0.0f), normal_matrix)));
                }

                for (int set = 0; set < 3; ++set)
                {
                    if (uvs[set])
                        cgltf_accessor_read_float(uvs[set], i, &v.uv[set].x, 2);
                }
            }

            const auto first = static_cast<std::uint32_t>(out.indices.size());
            const auto index_count = source.indices ? static_cast<std::uint32_t>(source.indices->count) : count;
            out.indices.resize(first + index_count);
            for (std::uint32_t i = 0; i < index_count; ++i)
            {
                const auto index = source.indices ? static_cast<std::uint32_t>(cgltf_accessor_read_index(source.indices, i)) : i;
                out.indices[first + i] = base + index;
            }

            if (flipped)
            {
                for (std::uint32_t i = first; i + 2 < first + index_count; i += 3)
                    std::swap(out.indices[i + 1], out.indices[i + 2]);
            }

            if (!normals)
            {
                for (std::uint32_t i = first; i + 2 < first + index_count; i += 3)
                {
                    vertex& a = out.vertices[out.indices[i]];
                    vertex& b = out.vertices[out.indices[i + 1]];
                    vertex& c = out.vertices[out.indices[i + 2]];
                    const XMVECTOR pa = XMLoadFloat3(&a.position);
                    const XMVECTOR face = XMVector3Cross(XMLoadFloat3(&b.position) - pa, XMLoadFloat3(&c.position) - pa);
                    for (vertex* v : { &a, &b, &c })
                        XMStoreFloat3(&v->normal, XMLoadFloat3(&v->normal) + face);
                }
                for (std::uint32_t i = base; i < base + count; ++i)
                    XMStoreFloat3(&out.vertices[i].normal, XMVector3Normalize(XMLoadFloat3(&out.vertices[i].normal)));
            }

            primitive prim;
            prim.name = node.name ? node.name : (node.mesh && node.mesh->name ? node.mesh->name : "");
            prim.first_index = first;
            prim.index_count = index_count;
            prim.material = source.material ? static_cast<int>(cgltf_material_index(data, source.material)) : -1;
            out.primitives.push_back(std::move(prim));
        }

        void add_node(const cgltf_data* data, const cgltf_node& node, model& out)
        {
            if (node.mesh)
            {
                float matrix[16];
                cgltf_node_transform_world(&node, matrix);
                const XMMATRIX world = XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(matrix));
                for (cgltf_size i = 0; i < node.mesh->primitives_count; ++i)
                    add_primitive(data, node, node.mesh->primitives[i], world, out);
            }

            for (cgltf_size i = 0; i < node.children_count; ++i)
                add_node(data, *node.children[i], out);
        }

        bool decode_images(const cgltf_data* data, std::uint32_t max_size, std::vector<image>& images)
        {
            std::atomic<std::size_t> next = 0;
            std::atomic<bool> ok = true;

            auto work = [&]
            {
                const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
                for (std::size_t i = next++; i < images.size(); i = next++)
                {
                    const cgltf_image& source = data->images[i];
                    if (!source.buffer_view)
                        continue;

                    const std::uint8_t* bytes = cgltf_buffer_view_data(source.buffer_view);
                    if (!bytes || !decode_image(bytes, source.buffer_view->size, max_size, images[i]))
                        ok = false;
                }
                if (SUCCEEDED(com))
                    CoUninitialize();
            };

            const std::size_t thread_count = (std::min<std::size_t>)(images.size(), (std::max)(1u, std::thread::hardware_concurrency()));
            std::vector<std::thread> threads;
            threads.reserve(thread_count);
            for (std::size_t i = 0; i < thread_count; ++i)
                threads.emplace_back(work);
            for (auto& thread : threads)
                thread.join();

            return ok;
        }

        void compute_bounds(model& out)
        {
            XMVECTOR low = XMVectorReplicate(FLT_MAX);
            XMVECTOR high = XMVectorReplicate(-FLT_MAX);
            for (const auto& v : out.vertices)
            {
                const XMVECTOR p = XMLoadFloat3(&v.position);
                low = XMVectorMin(low, p);
                high = XMVectorMax(high, p);
            }
            XMStoreFloat3(&out.bounds_min, low);
            XMStoreFloat3(&out.bounds_max, high);
        }
    }

    bool load(std::span<const std::uint8_t> bytes, std::uint32_t max_texture_size, model& out, std::string& error)
    {
        if (bytes.empty())
        {
            error = "model file is missing";
            return false;
        }

        cgltf_options options{};
        gltf_handle gltf;
        if (cgltf_parse(&options, bytes.data(), bytes.size(), &gltf.data) != cgltf_result_success)
        {
            error = "invalid glTF file";
            return false;
        }

        if (cgltf_load_buffers(&options, gltf.data, "") != cgltf_result_success)
        {
            error = "cannot read model buffers";
            return false;
        }

        const cgltf_data* data = gltf.data;
        out = {};
        out.images.resize(data->images_count);

        out.materials.reserve(data->materials_count);
        for (cgltf_size i = 0; i < data->materials_count; ++i)
            out.materials.push_back(to_material(data, data->materials[i], out.images));

        const cgltf_scene* scene = data->scene ? data->scene : (data->scenes_count ? &data->scenes[0] : nullptr);
        if (scene)
        {
            for (cgltf_size i = 0; i < scene->nodes_count; ++i)
                add_node(data, *scene->nodes[i], out);
        }
        else
        {
            for (cgltf_size i = 0; i < data->nodes_count; ++i)
            {
                if (!data->nodes[i].parent)
                    add_node(data, data->nodes[i], out);
            }
        }

        if (out.primitives.empty())
        {
            error = "model has no meshes";
            return false;
        }

        if (!decode_images(data, max_texture_size, out.images))
        {
            error = "cannot decode model textures";
            return false;
        }

        compute_bounds(out);
        return true;
    }
}
