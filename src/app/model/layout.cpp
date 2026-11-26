#include "layout.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <cwchar>

using namespace DirectX;

namespace menu::model
{
    namespace
    {
        constexpr float match_distance = 0.03f;

        constexpr std::array xbox_elite{
            part_rule{ button::south, "glass", { 0.432f, 0.160f, 0.179f } },
            part_rule{ button::south, "Circle.009", { 0.432f, 0.158f, 0.176f } },
            part_rule{ button::south, "Text.003", { 0.433f, 0.164f, 0.187f } },
            part_rule{ button::east, "glass", { 0.547f, 0.252f, 0.109f } },
            part_rule{ button::east, "Circle.009", { 0.549f, 0.253f, 0.106f } },
            part_rule{ button::east, "Text.002", { 0.551f, 0.261f, 0.120f } },
            part_rule{ button::west, "glass", { 0.318f, 0.266f, 0.140f } },
            part_rule{ button::west, "Circle.009", { 0.317f, 0.265f, 0.138f } },
            part_rule{ button::west, "Text_Material", { 0.316f, 0.271f, 0.147f } },
            part_rule{ button::north, "glass", { 0.430f, 0.364f, 0.086f } },
            part_rule{ button::north, "Circle.009", { 0.431f, 0.364f, 0.085f } },
            part_rule{ button::north, "Text.001", { 0.433f, 0.369f, 0.092f } },
            part_rule{ button::left_stick, "Cube.002", { -0.407f, 0.288f, 0.184f } },
            part_rule{ button::left_stick, "Circle.010", { -0.407f, 0.283f, 0.156f } },
            part_rule{ button::right_stick, "Cube.005", { 0.205f, 0.047f, 0.284f } },
            part_rule{ button::right_stick, "Circle.010", { 0.205f, 0.046f, 0.247f } },
            part_rule{ button::dpad, "Cylinder", { -0.208f, 0.023f, 0.245f } },
            part_rule{ button::view, "Circle.003", { -0.118f, 0.270f, 0.147f } },
            part_rule{ button::menu, "Circle.003", { 0.115f, 0.270f, 0.147f } },
            part_rule{ button::guide, "Circle.004", { 0.0f, 0.399f, 0.048f } },
            part_rule{ button::left_bumper, "Cube.003", { -0.386f, 0.445f, -0.073f } },
            part_rule{ button::right_bumper, "Cube.003", { 0.386f, 0.444f, -0.072f } },
            part_rule{ button::left_trigger, "Cube.003", { -0.446f, 0.350f, -0.221f } },
            part_rule{ button::right_trigger, "Cube.003", { 0.446f, 0.350f, -0.220f } },
            part_rule{ button::paddle_1, "Plane.001", { 0.217f, -0.068f, 0.038f } },
            part_rule{ button::paddle_2, "Plane.001", { 0.238f, -0.190f, 0.099f } },
            part_rule{ button::paddle_3, "Plane.001", { -0.218f, -0.068f, 0.038f } },
            part_rule{ button::paddle_4, "Plane.001", { -0.239f, -0.190f, 0.099f } },
        };

        constexpr std::array dualsense{
            part_rule{ button::south, "Buttons", { 0.492f, 0.143f, 0.144f } },
            part_rule{ button::south, "Transparent", { 0.492f, 0.143f, 0.152f } },
            part_rule{ button::east, "Buttons", { 0.600f, 0.254f, 0.104f } },
            part_rule{ button::east, "Transparent", { 0.601f, 0.254f, 0.111f } },
            part_rule{ button::west, "Buttons", { 0.397f, 0.257f, 0.141f } },
            part_rule{ button::west, "Transparent", { 0.397f, 0.257f, 0.148f } },
            part_rule{ button::north, "Buttons", { 0.490f, 0.377f, 0.107f } },
            part_rule{ button::north, "Transparent", { 0.490f, 0.378f, 0.115f } },
            part_rule{ button::dpad_up, "Buttons", { -0.489f, 0.327f, 0.113f } },
            part_rule{ button::dpad_up, "Transparent", { -0.491f, 0.328f, 0.125f } },
            part_rule{ button::dpad_down, "Buttons", { -0.485f, 0.171f, 0.140f } },
            part_rule{ button::dpad_down, "Transparent", { -0.486f, 0.172f, 0.151f } },
            part_rule{ button::dpad_left, "Buttons", { -0.568f, 0.254f, 0.109f } },
            part_rule{ button::dpad_left, "Transparent", { -0.571f, 0.255f, 0.121f } },
            part_rule{ button::dpad_right, "Buttons", { -0.412f, 0.249f, 0.142f } },
            part_rule{ button::dpad_right, "Transparent", { -0.414f, 0.251f, 0.153f } },
            part_rule{ button::left_stick, "Thumb_Top", { -0.259f, -0.012f, 0.255f } },
            part_rule{ button::right_stick, "Thumb_Top", { 0.262f, -0.012f, 0.255f } },
            part_rule{ button::left_bumper, "Trigger", { -0.518f, 0.422f, -0.019f } },
            part_rule{ button::right_bumper, "Trigger", { 0.522f, 0.422f, -0.019f } },
            part_rule{ button::left_trigger, "Trigger", { -0.512f, 0.398f, -0.197f } },
            part_rule{ button::right_trigger, "Trigger", { 0.516f, 0.398f, -0.197f } },
            part_rule{ button::view, "SPCB", { -0.373f, 0.414f, 0.117f } },
            part_rule{ button::menu, "SPCB", { 0.372f, 0.413f, 0.117f } },
            part_rule{ button::guide, "Logo", { -0.003f, 0.049f, 0.162f } },
            part_rule{ button::guide, "Logo", { 0.015f, 0.027f, 0.159f } },
            part_rule{ button::guide, "Logo", { -0.040f, 0.031f, 0.160f } },
            part_rule{ button::mute, "FBCB", { -0.005f, -0.038f, 0.153f } },
            part_rule{ button::touchpad, "Front_Panel1", { 0.007f, 0.357f, 0.104f } },
            part_rule{ button::touchpad, "Front_Panel_Box1", { 0.002f, 0.347f, 0.096f } },
        };

        float distance(const XMFLOAT3& a, const XMFLOAT3& b)
        {
            return XMVectorGetX(XMVector3Length(XMLoadFloat3(&a) - XMLoadFloat3(&b)));
        }
    }

    std::span<const part_rule> layout_rules(const wchar_t* file)
    {
        if (std::wcscmp(file, L"xbox/xbox.glb") == 0)
            return xbox_elite;
        if (std::wcscmp(file, L"ps5/ps5.glb") == 0)
            return dualsense;
        return {};
    }

    family layout_family(const wchar_t* file)
    {
        return std::wcsncmp(file, L"ps", 2) == 0 ? family::playstation : family::xbox;
    }

    std::vector<part> assign_parts(const model& source, std::span<const part_rule> rules, const XMMATRIX& transform)
    {
        std::vector<part> parts;
        if (rules.empty())
            return parts;

        for (std::uint32_t primitive = 0; primitive < source.primitives.size(); ++primitive)
        {
            const std::string& name = source.primitives[primitive].name;
            const bool relevant = std::any_of(rules.begin(), rules.end(), [&](const part_rule& rule) { return name.find(rule.primitive) != std::string::npos; });
            if (!relevant)
                continue;

            for (auto& island : find_islands(source, primitive, transform))
            {
                for (const auto& rule : rules)
                {
                    if (name.find(rule.primitive) == std::string::npos || distance(rule.center, island.center) > match_distance)
                        continue;

                    auto existing = std::find_if(parts.begin(), parts.end(), [&](const part& p) { return p.target == rule.target; });
                    if (existing == parts.end())
                    {
                        parts.push_back(part{ rule.target, {}, island.center });
                        existing = parts.end() - 1;
                    }
                    existing->triangles.insert(existing->triangles.end(), island.triangles.begin(), island.triangles.end());
                    break;
                }
            }
        }
        return parts;
    }
}
