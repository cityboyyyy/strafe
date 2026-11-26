#include "parts.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

using namespace DirectX;

namespace menu::model
{
    namespace
    {
        struct disjoint_set
        {
            std::vector<std::uint32_t> parent;

            explicit disjoint_set(std::size_t count)
                : parent(count)
            {
                for (std::size_t i = 0; i < count; ++i)
                    parent[i] = static_cast<std::uint32_t>(i);
            }

            std::uint32_t find(std::uint32_t value)
            {
                while (parent[value] != value)
                {
                    parent[value] = parent[parent[value]];
                    value = parent[value];
                }
                return value;
            }

            void join(std::uint32_t a, std::uint32_t b)
            {
                a = find(a);
                b = find(b);
                if (a != b)
                    parent[a] = b;
            }
        };

        std::uint64_t position_key(const XMFLOAT3& p, float cell)
        {
            const auto quantize = [cell](float v) { return static_cast<std::uint64_t>(static_cast<std::int64_t>(std::floor(v / cell)) & 0x1fffff); };
            return quantize(p.x) | (quantize(p.y) << 21) | (quantize(p.z) << 42);
        }
    }

    XMMATRIX display_transform(const model& source, const XMFLOAT3& rotation_degrees)
    {
        const XMVECTOR low = XMLoadFloat3(&source.bounds_min);
        const XMVECTOR high = XMLoadFloat3(&source.bounds_max);
        XMFLOAT3 center;
        XMStoreFloat3(&center, (low + high) * 0.5f);
        const float radius = (std::max)(XMVectorGetX(XMVector3Length(high - low)) * 0.5f, 1e-4f);
        const float inverse = 1.0f / radius;

        return XMMatrixTranslation(-center.x, -center.y, -center.z) *
            XMMatrixScaling(inverse, inverse, inverse) *
            XMMatrixRotationRollPitchYaw(XMConvertToRadians(rotation_degrees.x), XMConvertToRadians(rotation_degrees.y), XMConvertToRadians(rotation_degrees.z));
    }

    std::vector<island> find_islands(const model& source, std::uint32_t primitive, const XMMATRIX& transform)
    {
        const auto& range = source.primitives[primitive];
        std::unordered_map<std::uint32_t, std::uint32_t> local;
        std::vector<std::uint32_t> vertices;
        local.reserve(range.index_count);

        for (std::uint32_t i = 0; i < range.index_count; ++i)
        {
            const std::uint32_t index = source.indices[range.first_index + i];
            if (local.emplace(index, static_cast<std::uint32_t>(vertices.size())).second)
                vertices.push_back(index);
        }

        XMVECTOR low = XMVectorReplicate(FLT_MAX);
        XMVECTOR high = XMVectorReplicate(-FLT_MAX);
        for (const std::uint32_t index : vertices)
        {
            const XMVECTOR p = XMLoadFloat3(&source.vertices[index].position);
            low = XMVectorMin(low, p);
            high = XMVectorMax(high, p);
        }
        const float cell = (std::max)(XMVectorGetX(XMVector3Length(high - low)) * 1e-5f, 1e-7f);

        disjoint_set groups(vertices.size());
        std::unordered_map<std::uint64_t, std::uint32_t> welded;
        welded.reserve(vertices.size());
        for (std::uint32_t i = 0; i < vertices.size(); ++i)
        {
            const auto [slot, inserted] = welded.emplace(position_key(source.vertices[vertices[i]].position, cell), i);
            if (!inserted)
                groups.join(i, slot->second);
        }

        for (std::uint32_t t = 0; t + 2 < range.index_count; t += 3)
        {
            const std::uint32_t a = local[source.indices[range.first_index + t]];
            groups.join(a, local[source.indices[range.first_index + t + 1]]);
            groups.join(a, local[source.indices[range.first_index + t + 2]]);
        }

        std::unordered_map<std::uint32_t, std::size_t> slots;
        std::vector<island> result;
        for (std::uint32_t t = 0; t + 2 < range.index_count; t += 3)
        {
            const std::uint32_t root = groups.find(local[source.indices[range.first_index + t]]);
            auto [slot, inserted] = slots.emplace(root, result.size());
            if (inserted)
            {
                island part;
                part.primitive = primitive;
                result.push_back(std::move(part));
            }
            result[slot->second].triangles.push_back(range.first_index + t);
        }

        for (auto& part : result)
        {
            XMVECTOR part_low = XMVectorReplicate(FLT_MAX);
            XMVECTOR part_high = XMVectorReplicate(-FLT_MAX);
            for (const std::uint32_t first : part.triangles)
            {
                for (std::uint32_t corner = 0; corner < 3; ++corner)
                {
                    const XMVECTOR p = XMVector3TransformCoord(XMLoadFloat3(&source.vertices[source.indices[first + corner]].position), transform);
                    part_low = XMVectorMin(part_low, p);
                    part_high = XMVectorMax(part_high, p);
                }
            }
            XMStoreFloat3(&part.center, (part_low + part_high) * 0.5f);
            XMStoreFloat3(&part.size, part_high - part_low);
        }

        std::sort(result.begin(), result.end(), [](const island& a, const island& b) { return a.triangles.size() > b.triangles.size(); });
        return result;
    }
}
