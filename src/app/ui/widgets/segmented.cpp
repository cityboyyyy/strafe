#include "segmented.h"
#include "../fonts.h"
#include "../paint.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace menu::ui::widgets
{
    namespace
    {
        constexpr float height = 34.0f;
        constexpr float inset = 3.0f;
        constexpr float label_pad = 20.0f;
        constexpr float font_size = 13.5f;

        struct edge
        {
            float value = 0.0f;
            float velocity = 0.0f;
        };

        struct state
        {
            edge left;
            edge right;
            bool ready = false;
            float press = 0.0f;
            std::vector<float> hover;
        };

        std::unordered_map<ImGuiID, state> g_states;

        float px(float value)
        {
            return value * ImGui::GetStyle().FontScaleDpi;
        }

        void step(edge& e, float target, float stiffness, float damping, float dt)
        {
            constexpr int substeps = 4;
            const float h = std::min(dt, 1.0f / 30.0f) / substeps;
            for (int i = 0; i < substeps; ++i)
            {
                const float force = (target - e.value) * stiffness - e.velocity * damping;
                e.velocity += force * h;
                e.value += e.velocity * h;
            }
        }

        float label_width(const char* label)
        {
            ImGui::PushFont(fonts::get(fonts::role::strong), font_size);
            const float width = ImGui::CalcTextSize(label).x;
            ImGui::PopFont();
            return width + px(label_pad) * 2.0f;
        }
    }

    float segmented_width(const char* const* labels, int count)
    {
        float width = px(inset) * 2.0f;
        for (int i = 0; i < count; ++i)
            width += label_width(labels[i]);
        return width;
    }

    bool segmented(const char* id, const char* const* labels, int count, int& index, ImVec2 center)
    {
        ImGui::PushID(id);
        state& s = g_states[ImGui::GetID("##segmented")];
        s.hover.resize(static_cast<std::size_t>(count), 0.0f);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float dt = ImGui::GetIO().DeltaTime;

        const float total = segmented_width(labels, count);
        const float h = px(height);
        const ImVec2 min{ std::round(center.x - total * 0.5f), std::round(center.y - h * 0.5f) };
        const ImVec2 max{ min.x + total, min.y + h };

        std::vector<float> starts(static_cast<std::size_t>(count));
        std::vector<float> widths(static_cast<std::size_t>(count));
        float x = min.x + px(inset);
        for (int i = 0; i < count; ++i)
        {
            starts[static_cast<std::size_t>(i)] = x;
            widths[static_cast<std::size_t>(i)] = label_width(labels[i]);
            x += widths[static_cast<std::size_t>(i)];
        }

        bool changed = false;
        bool any_held = false;
        int hovered = -1;
        for (int i = 0; i < count; ++i)
        {
            ImGui::PushID(i);
            ImGui::SetCursorScreenPos(ImVec2(starts[static_cast<std::size_t>(i)], min.y));
            if (ImGui::InvisibleButton("##segment", ImVec2(widths[static_cast<std::size_t>(i)], h)) && index != i)
            {
                index = i;
                changed = true;
            }
            if (ImGui::IsItemHovered())
            {
                hovered = i;
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }
            any_held = any_held || ImGui::IsItemActive();
            ImGui::PopID();
        }

        const std::size_t active = static_cast<std::size_t>(std::clamp(index, 0, count - 1));
        const float target_left = starts[active];
        const float target_right = starts[active] + widths[active];
        if (!s.ready)
        {
            s.left.value = target_left;
            s.right.value = target_right;
            s.ready = true;
        }
        const bool moving_right = target_left > s.left.value + 0.5f;
        const bool moving_left = target_left < s.left.value - 0.5f;
        step(s.left, target_left, moving_left ? 520.0f : 230.0f, moving_left ? 40.0f : 27.0f, dt);
        step(s.right, target_right, moving_right ? 520.0f : 230.0f, moving_right ? 40.0f : 27.0f, dt);
        s.press += ((any_held ? 1.0f : 0.0f) - s.press) * (1.0f - std::exp(-18.0f * dt));

        const float radius = h * 0.5f;
        draw->AddRectFilled(min, max, paint::rgba(1.0f, 1.0f, 1.0f, 0.045f), radius);
        draw->AddRect(min, max, paint::rgba(1.0f, 1.0f, 1.0f, 0.06f), radius, 0, 1.0f);

        const float squeeze = px(1.6f) * s.press;
        const ImVec2 thumb_min{ s.left.value + squeeze, min.y + px(inset) + squeeze };
        const ImVec2 thumb_max{ s.right.value - squeeze, max.y - px(inset) - squeeze };
        const float thumb_radius = (thumb_max.y - thumb_min.y) * 0.5f;
        paint::soft_ellipse(draw, ImVec2((thumb_min.x + thumb_max.x) * 0.5f, (thumb_min.y + thumb_max.y) * 0.5f + px(2.0f)),
            ImVec2((thumb_max.x - thumb_min.x) * 0.6f, (thumb_max.y - thumb_min.y) * 0.75f), ImVec4(0.0f, 0.0f, 0.0f, 0.35f), 6, 32);
        draw->AddRectFilled(thumb_min, thumb_max, paint::rgba(0.2f, 0.2f, 0.215f, 1.0f), thumb_radius);
        draw->PushClipRect(thumb_min, ImVec2(thumb_max.x, (thumb_min.y + thumb_max.y) * 0.5f), true);
        draw->AddRectFilled(thumb_min, thumb_max, paint::rgba(1.0f, 1.0f, 1.0f, 0.035f), thumb_radius);
        draw->PopClipRect();
        draw->AddRect(thumb_min, thumb_max, paint::rgba(1.0f, 1.0f, 1.0f, 0.09f), thumb_radius, 0, 1.0f);

        ImGui::PushFont(fonts::get(fonts::role::strong), font_size);
        for (int i = 0; i < count; ++i)
        {
            const std::size_t at = static_cast<std::size_t>(i);
            s.hover[at] += ((hovered == i ? 1.0f : 0.0f) - s.hover[at]) * (1.0f - std::exp(-16.0f * dt));
            const float segment_center = starts[at] + widths[at] * 0.5f;
            const float cover = std::clamp(1.0f - std::abs(segment_center - (s.left.value + s.right.value) * 0.5f) / (widths[at] * 0.6f), 0.0f, 1.0f);
            const float brightness = 0.5f + 0.18f * s.hover[at] + (0.97f - 0.5f) * cover;
            const ImVec2 size = ImGui::CalcTextSize(labels[i]);
            draw->AddText(ImVec2(std::round(segment_center - size.x * 0.5f), std::round(center.y - size.y * 0.5f)),
                paint::rgba(brightness, brightness, brightness + 0.01f, 1.0f), labels[i]);
        }
        ImGui::PopFont();
        ImGui::PopID();
        return changed;
    }
}
