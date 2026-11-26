#include "button_badge.h"
#include "../fonts.h"
#include "../paint.h"

#include <algorithm>
#include <cmath>

namespace menu::ui::widgets
{
    namespace
    {
        using model::button;

        void label(ImDrawList* draw, ImVec2 center, float size, ImU32 color, const char* text)
        {
            const float s = ImGui::GetStyle().FontScaleDpi;
            ImGui::PushFont(fonts::get(fonts::role::strong), size / s);
            const ImVec2 extent = ImGui::CalcTextSize(text);
            draw->AddText(ImVec2(std::round(center.x - extent.x * 0.5f), std::round(center.y - extent.y * 0.5f)), color, text);
            ImGui::PopFont();
        }

        void line(ImDrawList* draw, ImVec2 a, ImVec2 b, ImU32 color, float thickness)
        {
            const ImVec2 points[] = { a, b };
            paint::round_polyline(draw, points, 2, color, thickness);
        }

        void arrow(ImDrawList* draw, ImVec2 c, float size, float dx, float dy, ImU32 color)
        {
            const ImVec2 tip{ c.x + dx * size * 0.45f, c.y + dy * size * 0.45f };
            const ImVec2 side{ -dy * size * 0.4f, dx * size * 0.4f };
            const ImVec2 base{ c.x - dx * size * 0.3f, c.y - dy * size * 0.3f };
            draw->AddTriangleFilled(tip, ImVec2(base.x + side.x, base.y + side.y), ImVec2(base.x - side.x, base.y - side.y), color);
        }
    }

    void button_glyph(ImDrawList* draw, ImVec2 c, float size, button which, model::family style, ImU32 color)
    {
        const bool playstation = style == model::family::playstation;
        const float stroke = std::max(1.4f, size * 0.12f);
        const float h = size * 0.5f;

        switch (which)
        {
        case button::south:
            if (!playstation)
                return label(draw, c, size, color, "A");
            line(draw, ImVec2(c.x - h * 0.7f, c.y - h * 0.7f), ImVec2(c.x + h * 0.7f, c.y + h * 0.7f), color, stroke);
            line(draw, ImVec2(c.x - h * 0.7f, c.y + h * 0.7f), ImVec2(c.x + h * 0.7f, c.y - h * 0.7f), color, stroke);
            return;
        case button::east:
            if (!playstation)
                return label(draw, c, size, color, "B");
            draw->AddCircle(c, h * 0.78f, color, 24, stroke);
            return;
        case button::west:
            if (!playstation)
                return label(draw, c, size, color, "X");
            draw->AddRect(ImVec2(c.x - h * 0.7f, c.y - h * 0.7f), ImVec2(c.x + h * 0.7f, c.y + h * 0.7f), color, 1.0f, 0, stroke);
            return;
        case button::north:
            if (!playstation)
                return label(draw, c, size, color, "Y");
            {
                const ImVec2 points[] = { ImVec2(c.x, c.y - h * 0.8f), ImVec2(c.x + h * 0.8f, c.y + h * 0.6f), ImVec2(c.x - h * 0.8f, c.y + h * 0.6f), ImVec2(c.x, c.y - h * 0.8f) };
                paint::round_polyline(draw, points, 4, color, stroke);
            }
            return;
        case button::left_bumper:
            return label(draw, c, size * 0.78f, color, playstation ? "L1" : "LB");
        case button::right_bumper:
            return label(draw, c, size * 0.78f, color, playstation ? "R1" : "RB");
        case button::left_trigger:
            return label(draw, c, size * 0.78f, color, playstation ? "L2" : "LT");
        case button::right_trigger:
            return label(draw, c, size * 0.78f, color, playstation ? "R2" : "RT");
        case button::left_stick:
            return label(draw, c, size, color, "L");
        case button::right_stick:
            return label(draw, c, size, color, "R");
        case button::dpad:
        {
            const float a = h * 0.32f;
            const float b = h * 0.9f;
            const ImVec2 points[] = {
                ImVec2(c.x - a, c.y - b), ImVec2(c.x + a, c.y - b), ImVec2(c.x + a, c.y - a), ImVec2(c.x + b, c.y - a),
                ImVec2(c.x + b, c.y + a), ImVec2(c.x + a, c.y + a), ImVec2(c.x + a, c.y + b), ImVec2(c.x - a, c.y + b),
                ImVec2(c.x - a, c.y + a), ImVec2(c.x - b, c.y + a), ImVec2(c.x - b, c.y - a), ImVec2(c.x - a, c.y - a), ImVec2(c.x - a, c.y - b),
            };
            draw->AddPolyline(points, 13, color, ImDrawFlags_None, stroke * 0.85f);
            return;
        }
        case button::dpad_up:
            return arrow(draw, c, size, 0.0f, -1.0f, color);
        case button::dpad_down:
            return arrow(draw, c, size, 0.0f, 1.0f, color);
        case button::dpad_left:
            return arrow(draw, c, size, -1.0f, 0.0f, color);
        case button::dpad_right:
            return arrow(draw, c, size, 1.0f, 0.0f, color);
        case button::view:
        {
            const float q = h * 0.46f;
            const float o = h * 0.2f;
            const ImVec2 back_points[] = { ImVec2(c.x - o - q, c.y + o - q * 0.2f), ImVec2(c.x - o - q, c.y - o - q), ImVec2(c.x + o + q * 0.2f, c.y - o - q) };
            paint::round_polyline(draw, back_points, 3, color, stroke * 0.8f);
            draw->AddRect(ImVec2(c.x - q + o, c.y - q + o), ImVec2(c.x + q + o, c.y + q + o), color, 1.0f, 0, stroke * 0.8f);
            return;
        }
        case button::menu:
            for (int i = -1; i <= 1; ++i)
                line(draw, ImVec2(c.x - h * 0.62f, c.y + i * h * 0.48f), ImVec2(c.x + h * 0.62f, c.y + i * h * 0.48f), color, stroke * 0.85f);
            return;
        case button::guide:
            if (playstation)
                return label(draw, c, size * 0.74f, color, "PS");
            draw->AddCircle(c, h * 0.82f, color, 24, stroke * 0.85f);
            line(draw, ImVec2(c.x - h * 0.38f, c.y - h * 0.38f), ImVec2(c.x + h * 0.38f, c.y + h * 0.38f), color, stroke * 0.8f);
            line(draw, ImVec2(c.x - h * 0.38f, c.y + h * 0.38f), ImVec2(c.x + h * 0.38f, c.y - h * 0.38f), color, stroke * 0.8f);
            return;
        case button::touchpad:
            draw->AddRect(ImVec2(c.x - h * 0.85f, c.y - h * 0.5f), ImVec2(c.x + h * 0.85f, c.y + h * 0.5f), color, h * 0.2f, 0, stroke * 0.85f);
            return;
        case button::mute:
            draw->AddRect(ImVec2(c.x - h * 0.28f, c.y - h * 0.8f), ImVec2(c.x + h * 0.28f, c.y + h * 0.15f), color, h * 0.28f, 0, stroke * 0.8f);
            draw->PathArcTo(ImVec2(c.x, c.y - h * 0.1f), h * 0.55f, 0.15f, 3.0f, 12);
            draw->PathStroke(color, ImDrawFlags_None, stroke * 0.8f);
            line(draw, ImVec2(c.x, c.y + h * 0.45f), ImVec2(c.x, c.y + h * 0.8f), color, stroke * 0.8f);
            return;
        case button::paddle_1:
            return label(draw, c, size * 0.8f, color, "P1");
        case button::paddle_2:
            return label(draw, c, size * 0.8f, color, "P2");
        case button::paddle_3:
            return label(draw, c, size * 0.8f, color, "P3");
        case button::paddle_4:
            return label(draw, c, size * 0.8f, color, "P4");
        default:
            return;
        }
    }

    badge_result button_badge(const char* id, ImVec2 center, float radius, button which, model::family style, float hover, float select, const ImVec4& accent)
    {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImGui::SetCursorScreenPos(ImVec2(center.x - radius, center.y - radius));
        badge_result result;
        result.clicked = ImGui::InvisibleButton(id, ImVec2(radius * 2.0f, radius * 2.0f));
        result.hovered = ImGui::IsItemHovered();
        if (result.hovered)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        const float lift = std::max(hover, select);
        const ImVec4 base{ 0.055f, 0.055f, 0.065f, 0.94f };
        const ImVec4 fill = paint::mix(base, ImVec4(accent.x, accent.y, accent.z, 1.0f), select * 0.92f);
        const ImVec4 edge = paint::mix(ImVec4(1.0f, 1.0f, 1.0f, 0.42f), ImVec4(accent.x, accent.y, accent.z, 1.0f), lift);
        const float grow = radius * (1.0f + 0.08f * lift);

        if (lift > 0.0f)
            paint::soft_ellipse(draw, center, ImVec2(grow * 1.9f, grow * 1.9f), ImVec4(accent.x, accent.y, accent.z, 0.22f * lift), 8, 40);
        draw->AddCircleFilled(center, grow, paint::to_u32(fill), 40);
        draw->AddCircle(center, grow - 0.5f, paint::to_u32(edge), 40, 1.3f);

        const ImVec4 ink = paint::mix(ImVec4(0.93f, 0.93f, 0.95f, 1.0f), ImVec4(0.04f, 0.05f, 0.07f, 1.0f), select);
        button_glyph(draw, center, radius * 0.92f, which, style, paint::to_u32(ink));
        return result;
    }
}
