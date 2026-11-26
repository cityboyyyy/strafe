#include "arrow_button.h"
#include "../paint.h"

#include <cmath>

namespace menu::ui::widgets
{
    namespace
    {
        constexpr float width_ratio = 1.36f;
        constexpr float exit_duration = 0.16f;
        constexpr float enter_duration = 0.34f;
        constexpr float hold_until = 1.1f;
        constexpr float fade_duration = 0.45f;

        const ImVec4 surface_edge{ 0.925f, 0.925f, 0.925f, 1.0f };
        const ImVec4 surface{ 0.972f, 0.972f, 0.972f, 1.0f };
        const ImVec4 ink{ 0.035f, 0.035f, 0.035f, 1.0f };

        void arrow(ImDrawList* draw, ImVec2 center, float height, float scale, int direction, ImU32 color)
        {
            const float reach = height * 0.085f * scale * static_cast<float>(direction);
            const float span = height * 0.165f * scale;
            const ImVec2 points[] = {
                ImVec2(center.x - reach, center.y - span),
                ImVec2(center.x + reach, center.y),
                ImVec2(center.x - reach, center.y + span),
            };
            paint::round_polyline(draw, points, 3, color, height * 0.078f * scale);
        }
    }

    float arrow_button::width(float height)
    {
        return height * width_ratio;
    }

    void arrow_button::trigger()
    {
        started_ = ImGui::GetTime();
    }

    bool arrow_button::draw(const char* id, ImVec2 center, float height, int direction, const ImVec4& accent)
    {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float s = ImGui::GetStyle().FontScaleDpi;
        const float full_width = width(height);

        ImGui::SetCursorScreenPos(ImVec2(center.x - full_width * 0.5f, center.y - height * 0.5f));
        const bool pressed = ImGui::InvisibleButton(id, ImVec2(full_width, height), ImGuiButtonFlags_EnableNav);
        const bool hovered = ImGui::IsItemHovered();
        const bool held = ImGui::IsItemActive();
        if (hovered)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (pressed)
            trigger();

        float elapsed = started_ < 0.0 ? -1.0f : static_cast<float>(ImGui::GetTime() - started_);
        if (elapsed > hold_until + fade_duration)
        {
            started_ = -1.0;
            elapsed = -1.0f;
        }

        const float press = held ? 0.95f : 1.0f;
        const ImVec2 half{ full_width * 0.5f * press, height * 0.5f * press };
        const ImVec2 a{ center.x - half.x, center.y - half.y };
        const ImVec2 b{ center.x + half.x, center.y + half.y };
        const float radius = half.y;
        const float inset = 1.5f * s;

        draw->AddRectFilled(a, b, paint::to_u32(surface_edge), radius);
        draw->AddRectFilled(ImVec2(a.x + inset, a.y + inset), ImVec2(b.x - inset, b.y - inset * 1.6f), paint::to_u32(surface), radius - inset);
        draw->AddRect(a, b, paint::rgba(0.0f, 0.0f, 0.0f, 0.10f), radius, 0, 1.0f);

        const float fade = motion::smooth(motion::window(elapsed, hold_until, fade_duration));
        draw->PushClipRect(ImVec2(a.x + inset, a.y + inset), ImVec2(b.x - inset, b.y - inset), true);

        if (elapsed >= 0.0f)
        {
            const float glow_in = motion::smooth(motion::window(elapsed, 0.0f, 0.12f));
            paint::soft_ellipse(draw, center, ImVec2(half.x * 0.95f, half.y * 1.05f), ImVec4(accent.x, accent.y, accent.z, 0.55f * glow_in * (1.0f - fade)));
        }

        const float nudge = hover_.update(hovered && elapsed < 0.0f ? 1.0f : 0.0f, ImGui::GetIO().DeltaTime, 320.0f, 24.0f);
        const float dir = static_cast<float>(direction);
        const float travel = full_width * 0.42f;
        float offset = nudge * 2.5f * s * dir;
        float alpha = 1.0f;
        ImVec4 color = ink;

        if (elapsed >= 0.0f)
        {
            const float lit = motion::smooth(motion::window(elapsed, 0.0f, 0.18f));
            color = paint::mix(paint::mix(ink, accent, lit), ink, fade);

            if (elapsed < exit_duration)
            {
                const float t = motion::out_cubic(elapsed / exit_duration);
                offset = dir * travel * t;
                alpha = 1.0f - t;
            }
            else
            {
                const float t = motion::window(elapsed, exit_duration, enter_duration);
                offset = -dir * travel * (1.0f - motion::out_back(t, 1.25f));
                alpha = motion::smooth(t * 1.6f);
            }
        }

        arrow(draw, ImVec2(center.x + offset, center.y), height, press, direction, paint::to_u32(color, alpha));
        draw->PopClipRect();
        return pressed;
    }
}
