#include "paint.h"

#include "../render/device.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace menu::ui::paint
{
    ImU32 rgba(float r, float g, float b, float a)
    {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, std::clamp(a, 0.0f, 1.0f)));
    }

    ImVec4 mix(const ImVec4& from, const ImVec4& to, float t)
    {
        return ImVec4(std::lerp(from.x, to.x, t), std::lerp(from.y, to.y, t), std::lerp(from.z, to.z, t), std::lerp(from.w, to.w, t));
    }

    ImU32 to_u32(const ImVec4& color, float alpha_scale)
    {
        return rgba(color.x, color.y, color.z, color.w * alpha_scale);
    }

    void soft_ellipse(ImDrawList* draw, ImVec2 center, ImVec2 radius, const ImVec4& color, int rings, int segments)
    {
        if (color.w <= 0.0f || radius.x <= 0.0f || radius.y <= 0.0f)
            return;

        const ImVec2 uv = ImGui::GetFontTexUvWhitePixel();
        const int vertex_count = 1 + rings * segments;
        const int index_count = segments * 3 + (rings - 1) * segments * 6;
        draw->PrimReserve(index_count, vertex_count);

        const auto base = static_cast<ImDrawIdx>(draw->_VtxCurrentIdx);
        draw->PrimWriteVtx(center, uv, to_u32(color));

        for (int ring = 1; ring <= rings; ++ring)
        {
            const float t = static_cast<float>(ring) / static_cast<float>(rings);
            const float falloff = (1.0f - t * t) * (1.0f - t * t);
            const ImU32 ring_color = to_u32(color, falloff);
            for (int segment = 0; segment < segments; ++segment)
            {
                const float angle = static_cast<float>(segment) / static_cast<float>(segments) * 2.0f * std::numbers::pi_v<float>;
                draw->PrimWriteVtx(ImVec2(center.x + std::cos(angle) * radius.x * t, center.y + std::sin(angle) * radius.y * t), uv, ring_color);
            }
        }

        const auto vertex = [&](int ring, int segment)
        {
            return static_cast<ImDrawIdx>(base + 1 + (ring - 1) * segments + (segment % segments));
        };

        for (int segment = 0; segment < segments; ++segment)
        {
            draw->PrimWriteIdx(base);
            draw->PrimWriteIdx(vertex(1, segment));
            draw->PrimWriteIdx(vertex(1, segment + 1));
        }

        for (int ring = 1; ring < rings; ++ring)
        {
            for (int segment = 0; segment < segments; ++segment)
            {
                draw->PrimWriteIdx(vertex(ring, segment));
                draw->PrimWriteIdx(vertex(ring + 1, segment));
                draw->PrimWriteIdx(vertex(ring + 1, segment + 1));
                draw->PrimWriteIdx(vertex(ring, segment));
                draw->PrimWriteIdx(vertex(ring + 1, segment + 1));
                draw->PrimWriteIdx(vertex(ring, segment + 1));
            }
        }
    }

    void vertical_gradient(ImDrawList* draw, ImVec2 min, ImVec2 max, ImU32 top, ImU32 bottom)
    {
        draw->AddRectFilledMultiColor(min, max, top, top, bottom, bottom);
    }

    ImVec2 spaced_text_size(const char* text, float spacing)
    {
        float width = 0.0f;
        int count = 0;
        for (const char* c = text; *c; ++c, ++count)
            width += ImGui::CalcTextSize(c, c + 1).x;
        return ImVec2(width + spacing * static_cast<float>(std::max(count - 1, 0)), ImGui::GetFontSize());
    }

    void spaced_text(ImDrawList* draw, ImVec2 position, float spacing, ImU32 color, const char* text)
    {
        for (const char* c = text; *c; ++c)
        {
            draw->AddText(position, color, c, c + 1);
            position.x += ImGui::CalcTextSize(c, c + 1).x + spacing;
        }
    }

    void rounded_path(ImDrawList* draw, ImVec2 min, ImVec2 max, float top_left, float top_right, float bottom_right, float bottom_left)
    {
        constexpr float pi = std::numbers::pi_v<float>;
        const float limit = std::min(max.x - min.x, max.y - min.y) * 0.5f;
        top_left = std::min(top_left, limit);
        top_right = std::min(top_right, limit);
        bottom_right = std::min(bottom_right, limit);
        bottom_left = std::min(bottom_left, limit);

        const auto corner = [&](ImVec2 center, float radius, float from, float to)
        {
            if (radius <= 0.5f)
                draw->PathLineTo(center);
            else
                draw->PathArcTo(center, radius, from, to, std::max(6, static_cast<int>(radius * 0.6f)));
        };

        corner(ImVec2(min.x + top_left, min.y + top_left), top_left, pi, pi * 1.5f);
        corner(ImVec2(max.x - top_right, min.y + top_right), top_right, pi * 1.5f, pi * 2.0f);
        corner(ImVec2(max.x - bottom_right, max.y - bottom_right), bottom_right, 0.0f, pi * 0.5f);
        corner(ImVec2(min.x + bottom_left, max.y - bottom_left), bottom_left, pi * 0.5f, pi);
    }

    void round_polyline(ImDrawList* draw, const ImVec2* points, int count, ImU32 color, float thickness)
    {
        draw->AddPolyline(points, count, color, ImDrawFlags_None, thickness);
        for (int i = 0; i < count; ++i)
            draw->AddCircleFilled(points[i], thickness * 0.5f, color, 12);
    }

    void chevron(ImDrawList* draw, ImVec2 center, float size, int direction, ImU32 color, float thickness)
    {
        const float reach = static_cast<float>(direction) * size * 0.5f;
        draw->PathLineTo(ImVec2(center.x - reach, center.y - size));
        draw->PathLineTo(ImVec2(center.x + reach, center.y));
        draw->PathLineTo(ImVec2(center.x - reach, center.y + size));
        draw->PathStroke(color, 0, thickness);
    }

    void dashed_polyline(ImDrawList* draw, const ImVec2* points, int count, ImU32 color, float thickness, float dash, float gap)
    {
        float phase = 0.0f;
        for (int i = 0; i + 1 < count; ++i)
        {
            const ImVec2 a = points[i];
            const ImVec2 b = points[i + 1];
            const float length = std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
            if (length < 0.001f)
                continue;
            const ImVec2 dir{ (b.x - a.x) / length, (b.y - a.y) / length };
            float t = 0.0f;
            while (t < length)
            {
                const float period = dash + gap;
                const float offset = std::fmod(phase, period);
                if (offset < dash)
                {
                    const float run = std::min(dash - offset, length - t);
                    draw->AddLine(ImVec2(a.x + dir.x * t, a.y + dir.y * t), ImVec2(a.x + dir.x * (t + run), a.y + dir.y * (t + run)), color, thickness);
                    t += run;
                    phase += run;
                }
                else
                {
                    const float run = std::min(period - offset, length - t);
                    t += run;
                    phase += run;
                }
            }
        }
    }

    namespace
    {
        void use_mip_sampler(const ImDrawList*, const ImDrawCmd*)
        {
            ID3D11SamplerState* sampler = render::mip_sampler();
            render::context()->PSSetSamplers(0, 1, &sampler);
        }
    }

    void render_image(ImDrawList* draw, void* texture, ImVec2 min, ImVec2 max, ImU32 tint)
    {
        draw->AddCallback(use_mip_sampler, nullptr);
        draw->AddImage(static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(texture)), min, max, ImVec2(0, 0), ImVec2(1, 1), tint);
        draw->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
    }
}
