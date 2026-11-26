#include "install_button.h"
#include "../fonts.h"
#include "../paint.h"
#include "../../render/goo.h"

#include <imgui_internal.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <numbers>
#include <string>
#include <unordered_map>

namespace menu::ui::widgets
{
    namespace
    {
        constexpr float height = 34.0f;
        constexpr float icon_size = 22.0f;
        constexpr float icon_gap = 8.0f;
        constexpr float pad = 16.0f;
        constexpr float pad_icon = 6.0f;
        constexpr float font_size = 14.5f;
        constexpr float bubble_gap = 3.0f;
        constexpr float goo_size = 9.0f;
        constexpr float char_stagger = 0.055f;
        constexpr float char_in = 0.26f;
        constexpr float char_out = 0.16f;

        struct spring_state
        {
            float value = 0.0f;
            float velocity = 0.0f;
        };

        struct morph_state
        {
            std::string from;
            std::string to;
            float time = 10.0f;
        };

        std::unordered_map<ImGuiID, float> g_values;
        std::unordered_map<ImGuiID, spring_state> g_springs;
        std::unordered_map<ImGuiID, morph_state> g_morphs;
        std::unordered_map<ImGuiID, float> g_shown;

        float px(float value)
        {
            return value * ImGui::GetStyle().FontScaleDpi;
        }

        float approach(ImGuiID id, float target, float speed)
        {
            auto [it, fresh] = g_values.try_emplace(id, target);
            float& value = it->second;
            value += (target - value) * (1.0f - std::exp(-speed * ImGui::GetIO().DeltaTime));
            if (std::fabs(target - value) < 0.001f)
                value = target;
            return value;
        }

        float spring(ImGuiID id, float target, float stiffness, float damping)
        {
            auto [it, fresh] = g_springs.try_emplace(id, spring_state{ target, 0.0f });
            spring_state& s = it->second;
            constexpr int substeps = 4;
            const float dt = std::min(ImGui::GetIO().DeltaTime, 1.0f / 30.0f) / substeps;
            for (int i = 0; i < substeps; ++i)
            {
                const float force = (target - s.value) * stiffness - s.velocity * damping;
                s.velocity += force * dt;
                s.value += s.velocity * dt;
            }
            if (std::fabs(target - s.value) < 0.0005f && std::fabs(s.velocity) < 0.001f)
            {
                s.value = target;
                s.velocity = 0.0f;
            }
            return s.value;
        }

        ImU32 mix(ImU32 a, ImU32 b, float t)
        {
            return paint::to_u32(paint::mix(ImGui::ColorConvertU32ToFloat4(a), ImGui::ColorConvertU32ToFloat4(b), t));
        }

        ImU32 fade(ImU32 color, float alpha)
        {
            const ImU32 a = static_cast<ImU32>(((color >> IM_COL32_A_SHIFT) & 0xFF) * std::clamp(alpha, 0.0f, 1.0f));
            return (color & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
        }

        ImFont* font()
        {
            return fonts::get(fonts::role::strong);
        }

        float prefix_width(const std::string& text, std::size_t count)
        {
            return font()->CalcTextSizeA(px(font_size), FLT_MAX, 0.0f, text.c_str(), text.c_str() + count).x;
        }

        void retarget(morph_state& morph, const char* text)
        {
            if (morph.to == text)
                return;
            if (morph.to.empty())
            {
                morph.from = text;
                morph.to = text;
                morph.time = 10.0f;
                return;
            }
            morph.from = morph.to;
            morph.to = text;
            morph.time = 0.0f;
        }

        void draw_char(ImDrawList* draw, ImVec2 position, char c, float alpha, float blur, ImU32 color)
        {
            if (alpha <= 0.01f)
                return;
            const char text[2] = { c, 0 };
            if (blur > 0.2f)
            {
                const ImVec2 offsets[4] = { ImVec2(-blur, 0.0f), ImVec2(blur, 0.0f), ImVec2(0.0f, -blur), ImVec2(0.0f, blur) };
                for (const ImVec2& offset : offsets)
                    draw->AddText(font(), px(font_size), ImVec2(position.x + offset.x, position.y + offset.y), fade(color, alpha * 0.28f), text);
            }
            draw->AddText(font(), px(font_size), position, fade(color, alpha * (blur > 0.2f ? 0.6f : 1.0f)), text);
        }

        void draw_text(ImDrawList* draw, const morph_state& morph, ImVec2 origin, float visibility, ImU32 color)
        {
            const std::string& from = morph.from;
            const std::string& to = morph.to;
            std::size_t keep = 0;
            while (keep < from.size() && keep < to.size() && from[keep] == to[keep])
                ++keep;
            const float t = morph.time;
            const float blur_max = px(2.6f);
            const float drop = px(5.0f);

            for (std::size_t i = 0; i < keep; ++i)
                draw_char(draw, ImVec2(origin.x + prefix_width(to, i), origin.y), to[i], visibility, 0.0f, color);

            const bool swap = keep == 0;
            for (std::size_t i = keep; i < from.size(); ++i)
            {
                const float delay = swap ? 0.0f : static_cast<float>(i - keep) * char_stagger * 0.5f;
                const float q = ImSaturate((t - delay) / char_out);
                draw_char(draw, ImVec2(origin.x + prefix_width(from, i), origin.y - drop * q), from[i], (1.0f - q) * visibility, blur_max * q, color);
            }
            for (std::size_t i = keep; i < to.size(); ++i)
            {
                const float start = (swap ? char_out : 0.06f) + static_cast<float>(i - keep) * char_stagger;
                const float p = ImSaturate((t - start) / char_in);
                const float e = 1.0f - (1.0f - p) * (1.0f - p);
                draw_char(draw, ImVec2(origin.x + prefix_width(to, i), origin.y + drop * (1.0f - e)), to[i], e * visibility, blur_max * (1.0f - e), color);
            }
        }

        void draw_arrow(ImDrawList* draw, ImVec2 center, float radius, ImU32 color)
        {
            const float thickness = radius * 0.2f;
            const float top = center.y - radius * 0.5f;
            const float tip = center.y + radius * 0.48f;
            const ImVec2 shaft[] = { ImVec2(center.x, top), ImVec2(center.x, tip - thickness * 0.4f) };
            paint::round_polyline(draw, shaft, 2, color, thickness);
            const float wing = radius * 0.44f;
            const ImVec2 head[] = { ImVec2(center.x - wing, tip - wing), ImVec2(center.x, tip), ImVec2(center.x + wing, tip - wing) };
            paint::round_polyline(draw, head, 3, color, thickness);
        }

        void draw_check(ImDrawList* draw, ImVec2 center, float radius, ImU32 color)
        {
            const float thickness = radius * 0.2f;
            const ImVec2 points[] = { ImVec2(center.x - radius * 0.42f, center.y + radius * 0.02f), ImVec2(center.x - radius * 0.1f, center.y + radius * 0.34f),
                ImVec2(center.x + radius * 0.46f, center.y - radius * 0.32f) };
            paint::round_polyline(draw, points, 3, color, thickness);
        }
    }

    bool install_button(const char* id, ImVec2 right_center, float visibility, install_phase phase, float progress, const ImVec4& accent)
    {
        ImGui::PushID(id);
        const ImGuiID key = ImGui::GetID("##state");
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float dt = ImGui::GetIO().DeltaTime;
        const bool working = phase == install_phase::working;
        const bool done = phase == install_phase::done;

        const ImU32 background = IM_COL32(37, 37, 40, 255);
        const ImU32 background_hover = IM_COL32(45, 45, 49, 255);
        const ImU32 text_color = paint::to_u32(paint::mix(ImVec4(accent.x, accent.y, accent.z, 1.0f), ImVec4(1.0f, 1.0f, 1.0f, 1.0f), 0.18f));
        const ImU32 icon_background = paint::to_u32(ImVec4(accent.x, accent.y, accent.z, 1.0f));
        const ImU32 glow = paint::to_u32(ImVec4(accent.x, accent.y, accent.z, 0.55f));

        morph_state& morph = g_morphs[key];
        const char* label = working ? "Installing" : done ? "Installed" : phase == install_phase::failed ? "Retry install" : "Install driver";
        retarget(morph, label);
        morph.time += dt;

        float& shown = g_shown[key];
        const float target_progress = done ? 1.0f : working ? std::max(progress, 0.08f) : 0.0f;
        shown = target_progress < shown ? target_progress : std::min(target_progress, shown + dt / 1.1f);

        bool clicked = false;
        if (visibility <= 0.01f)
        {
            ImGui::PopID();
            return false;
        }

        const float h = px(height);
        const float r = h * 0.5f;
        const float lift = (1.0f - visibility) * px(10.0f);
        const float cy = std::round(right_center.y + lift);

        const float icon_t = spring(ImGui::GetID("##icon"), 1.0f, 260.0f, 24.0f);
        const float text_w = spring(ImGui::GetID("##text"), prefix_width(morph.to, morph.to.size()), 240.0f, 22.0f);
        const float pad_left = px(pad) + (px(pad_icon) - px(pad)) * icon_t;
        const float icon_span = (px(icon_size) + px(icon_gap)) * icon_t;
        const float pill_w = pad_left + icon_span + text_w + px(pad);

        const float split = spring(ImGui::GetID("##split"), working ? 1.0f : 0.0f, 210.0f, 16.0f);
        const float bubble_r = r * 0.94f * ImSaturate(split * 1.35f);
        const float bubble_d = split * (r + r * 0.94f + px(bubble_gap));
        const float extra = std::max(0.0f, bubble_d + bubble_r - r);
        const float group_w = pill_w + extra;

        const ImVec2 a(std::round(right_center.x - group_w), cy - r);
        const ImVec2 b(a.x + pill_w, cy + r);
        const ImVec2 end_center(b.x - r, cy);
        const ImVec2 bubble_center(end_center.x + bubble_d, cy);

        ImGui::SetCursorScreenPos(a);
        const bool pill_clicked = ImGui::InvisibleButton("##pill", ImVec2(pill_w, h));
        const bool interactive = visibility > 0.9f && !working && !done;
        const bool hovered = ImGui::IsItemHovered() && interactive;
        const bool held = ImGui::IsItemActive() && interactive;
        if (hovered)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        const float hover = approach(ImGui::GetID("##hover"), hovered ? 1.0f : 0.0f, 16.0f);
        ImU32 fill = mix(background, background_hover, hover);
        if (held)
            fill = mix(fill, IM_COL32(0, 0, 0, 255), 0.12f);
        const float glow_alpha = approach(ImGui::GetID("##glow"), working || done ? 1.0f : 0.0f, 8.0f);
        const float eased = 1.0f - (1.0f - shown) * (1.0f - shown);

        render::goo::shape g;
        g.pill_min = a;
        g.pill_max = b;
        g.pill_radius = r;
        g.bubble_center = bubble_center;
        g.bubble_radius = bubble_r;
        g.smoothness = px(goo_size);
        g.pill_color = fill;
        g.bubble_color = background;
        g.glow_color = glow;
        g.glow_start = a.x;
        g.glow_edge = a.x + pill_w * eased;
        g.glow_feather = px(34.0f);
        g.glow_alpha = glow_alpha;
        g.shadow_alpha = 0.32f;
        g.shadow_blur = px(7.0f);
        g.shadow_offset = px(2.0f);
        g.highlight = 0.05f;
        g.alpha = visibility;
        render::goo::draw(draw, g);

        if (icon_t > 0.02f)
        {
            const float radius = px(icon_size) * 0.5f * ImSaturate(icon_t);
            const ImVec2 center(a.x + pad_left + px(icon_size) * 0.5f * icon_t, cy);
            const float alpha = ImSaturate(icon_t) * visibility;
            draw->AddCircleFilled(center, radius, fade(icon_background, alpha), 32);
            const float check = approach(ImGui::GetID("##check"), done ? 1.0f : 0.0f, 14.0f);
            if (check < 0.99f)
                draw_arrow(draw, center, radius * (1.0f - check * 0.4f), fade(IM_COL32_WHITE, alpha * (1.0f - check)));
            if (check > 0.01f)
                draw_check(draw, center, radius * (0.7f + 0.3f * check), fade(IM_COL32_WHITE, alpha * check));
        }

        const float line_h = font()->CalcTextSizeA(px(font_size), FLT_MAX, 0.0f, "Ag").y;
        draw_text(draw, morph, ImVec2(std::round(a.x + pad_left + icon_span), std::round(cy - line_h * 0.5f)), visibility, text_color);

        if (bubble_r > 0.5f)
        {
            const float alpha = ImSaturate(split * 1.5f - 0.3f) * visibility;
            const float spin = static_cast<float>(ImGui::GetTime()) * 5.5f;
            const float arc = px(5.2f) * ImSaturate(split);
            draw->PathArcTo(bubble_center, arc, spin, spin + std::numbers::pi_v<float> * 1.35f, 18);
            draw->PathStroke(fade(IM_COL32(232, 232, 236, 255), alpha), 0, px(2.0f));
        }

        if (pill_clicked && interactive)
            clicked = true;
        ImGui::PopID();
        return clicked;
    }
}
