#include "confirm_button.h"
#include "../fonts.h"
#include "../paint.h"

#include <cmath>
#include <numbers>

namespace menu::ui::widgets
{
    namespace
    {
        constexpr float working_duration = 1.82f;
        constexpr float done_duration = 1.64f;
        constexpr float reset_duration = 0.38f;
        constexpr float reset_at = working_duration + done_duration;

        constexpr const char* prefix = "Confirm";
        constexpr const char* working_suffix = "ing";
        constexpr const char* done_suffix = "ed";

        const ImVec4 idle_fill{ 218.0f / 255.0f, 218.0f / 255.0f, 218.0f / 255.0f, 1.0f };
        const ImVec4 busy_fill{ 34.0f / 255.0f, 34.0f / 255.0f, 34.0f / 255.0f, 1.0f };
        const ImVec4 idle_ink{ 0.03f, 0.03f, 0.03f, 1.0f };
        const ImVec4 busy_ink{ 1.0f, 1.0f, 1.0f, 1.0f };

        float font_size(float height)
        {
            return height * 0.32f / ImGui::GetStyle().FontScaleDpi;
        }

        float text_width(const char* text)
        {
            return ImGui::CalcTextSize(text).x;
        }

        void blurred_text(ImDrawList* draw, ImVec2 position, ImU32 color, float alpha, float blur, const char* text)
        {
            if (alpha <= 0.0f)
                return;
            const ImVec4 base = ImGui::ColorConvertU32ToFloat4(color);
            if (blur > 0.25f)
            {
                const ImU32 ghost = paint::to_u32(base, alpha * 0.28f);
                draw->AddText(ImVec2(position.x - blur, position.y), ghost, text);
                draw->AddText(ImVec2(position.x + blur, position.y), ghost, text);
                draw->AddText(ImVec2(position.x, position.y - blur * 0.6f), ghost, text);
                draw->AddText(ImVec2(position.x, position.y + blur * 0.6f), ghost, text);
            }
            draw->AddText(position, paint::to_u32(base, alpha * (1.0f - std::min(blur, 3.0f) * 0.15f)), text);
        }
    }

    float confirm_button::idle_width(float height) const
    {
        ImGui::PushFont(fonts::get(fonts::role::strong), font_size(height));
        const float width = text_width(prefix) + height * 1.34f;
        ImGui::PopFont();
        return width;
    }

    bool confirm_button::draw(const char* id, ImVec2 center, float height)
    {
        const double now = ImGui::GetTime();
        const float s = ImGui::GetStyle().FontScaleDpi;
        ImDrawList* draw = ImGui::GetWindowDrawList();

        ImGui::PushFont(fonts::get(fonts::role::strong), font_size(height));
        const float prefix_width = text_width(prefix);
        const float working_width = text_width(working_suffix);
        const float done_width = text_width(done_suffix);
        const float padding = height * 1.34f;

        float elapsed = started_ < 0.0 ? -1.0f : static_cast<float>(now - started_);
        if (elapsed >= reset_at + reset_duration)
        {
            started_ = -1.0;
            reported_ = false;
            elapsed = -1.0f;
        }

        float target = prefix_width + padding;
        if (elapsed >= 0.0f && elapsed < reset_at)
            target = prefix_width + (elapsed < working_duration ? working_width : done_width) + padding;
        const float width = width_.update(target, ImGui::GetIO().DeltaTime);

        const float hit_width = prefix_width + working_width + padding;
        ImGui::SetCursorScreenPos(ImVec2(center.x - hit_width * 0.5f, center.y - height * 0.5f));
        const bool clicked = ImGui::InvisibleButton(id, ImVec2(hit_width, height), ImGuiButtonFlags_EnableNav) ||
            ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false);
        const bool held = ImGui::IsItemActive();
        if (ImGui::IsItemHovered() && elapsed < 0.0f)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        if (clicked && elapsed < 0.0f)
        {
            started_ = now;
            elapsed = 0.0f;
        }

        const float reset = motion::smooth(motion::window(elapsed, reset_at, 0.18f));
        const float dark = motion::smooth(motion::window(elapsed, 0.0f, 0.22f)) * (1.0f - reset);
        const float press = held && elapsed < 0.0f ? 0.97f : 1.0f;

        const ImVec2 min{ center.x - width * 0.5f * press, center.y - height * 0.5f * press };
        const ImVec2 max{ center.x + width * 0.5f * press, center.y + height * 0.5f * press };
        draw->AddRectFilled(min, max, paint::to_u32(paint::mix(idle_fill, busy_fill, dark)), (max.y - min.y) * 0.5f);

        const ImU32 ink = paint::to_u32(paint::mix(idle_ink, busy_ink, dark));
        const float text_y = std::round(center.y - ImGui::GetFontSize() * 0.52f);

        const char* from = "";
        const char* to = "";
        float from_width = 0.0f;
        float to_width = 0.0f;
        float morph = 1.0f;
        if (elapsed >= 0.0f && elapsed < working_duration)
        {
            to = working_suffix;
            to_width = working_width;
            morph = motion::smooth(motion::window(elapsed, 0.04f, 0.26f));
        }
        else if (elapsed >= working_duration && elapsed < reset_at)
        {
            from = working_suffix;
            from_width = working_width;
            to = done_suffix;
            to_width = done_width;
            morph = motion::smooth(motion::window(elapsed, working_duration, 0.26f));
        }
        else if (elapsed >= reset_at)
        {
            from = done_suffix;
            from_width = done_width;
            morph = motion::smooth(motion::window(elapsed, reset_at, 0.22f));
        }

        const float total = prefix_width + std::lerp(from_width, to_width, morph);
        const float text_x = std::round(center.x - total * 0.5f);
        draw->AddText(ImVec2(text_x, text_y), ink, prefix);
        const float blur = std::sin(morph * std::numbers::pi_v<float>) * 2.2f * s;
        const ImVec2 suffix_position{ text_x + prefix_width, text_y };
        if (*from)
            blurred_text(draw, ImVec2(suffix_position.x, suffix_position.y - (morph * 2.0f * s)), ink, 1.0f - morph, blur, from);
        if (*to)
            blurred_text(draw, ImVec2(suffix_position.x, suffix_position.y + ((1.0f - morph) * 2.0f * s)), ink, morph, blur, to);
        ImGui::PopFont();

        const float appear = motion::window(elapsed, 0.04f, 0.26f);
        const float vanish = motion::smooth(motion::window(elapsed, reset_at, 0.18f));
        const float badge = elapsed >= 0.0f ? motion::out_back(appear, 1.2f) * (1.0f - vanish) : 0.0f;
        if (badge > 0.0f)
        {
            const float alpha = std::clamp(appear * 1.6f, 0.0f, 1.0f) * (1.0f - vanish);
            const ImVec2 badge_center{ max.x - height * 0.26f, min.y + height * 0.20f };
            const float radius = height * 0.234f * std::lerp(0.55f, 1.0f, badge);
            draw->AddCircleFilled(badge_center, radius, paint::to_u32(busy_fill, alpha), 40);

            const float swap = motion::smooth(motion::window(elapsed, working_duration, 0.14f));
            const float check = motion::smooth(motion::window(elapsed, working_duration + 0.04f, 0.2f));

            if (swap < 1.0f)
            {
                const float ring = height * 0.155f * std::lerp(0.6f, 1.0f, badge);
                const float stroke = height * 0.036f;
                const float spinner_alpha = alpha * (1.0f - swap);
                draw->AddCircle(badge_center, ring, paint::rgba(1.0f, 1.0f, 1.0f, 0.26f * spinner_alpha), 40, stroke);
                const float angle = static_cast<float>(now) * 7.0f;
                draw->PathArcTo(badge_center, ring, angle, angle + 1.95f, 24);
                draw->PathStroke(paint::rgba(1.0f, 1.0f, 1.0f, 0.95f * spinner_alpha), ImDrawFlags_None, stroke);
            }

            if (check > 0.0f)
            {
                const float disk = height * 0.19f * std::lerp(0.75f, 1.0f, check);
                const float check_alpha = alpha * check;
                draw->AddCircleFilled(badge_center, disk, paint::rgba(1.0f, 1.0f, 1.0f, check_alpha), 36);
                const ImVec2 points[] = {
                    ImVec2(badge_center.x - disk * 0.42f, badge_center.y + disk * 0.02f),
                    ImVec2(badge_center.x - disk * 0.12f, badge_center.y + disk * 0.32f),
                    ImVec2(badge_center.x + disk * 0.42f, badge_center.y - disk * 0.30f),
                };
                paint::round_polyline(draw, points, 3, paint::to_u32(busy_fill, check_alpha), height * 0.042f);
            }
        }

        if (elapsed >= working_duration && !reported_)
        {
            reported_ = true;
            return true;
        }
        return false;
    }
}
