#include "binding_editor.h"
#include "fonts.h"
#include "icons.h"
#include "paint.h"
#include "widgets/button_badge.h"
#include "widgets/knob.h"

#include <Windows.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <numbers>

namespace menu::ui
{
    namespace
    {
        using model::button;

        constexpr button mirror_options[] = {
            button::south, button::east, button::west, button::north, button::view,
            button::left_bumper, button::right_bumper, button::left_trigger, button::right_trigger, button::menu,
        };

        float dpi()
        {
            return ImGui::GetStyle().FontScaleDpi;
        }

        float animate(ImGuiID key, float target, float speed = 14.0f)
        {
            ImGuiStorage* storage = ImGui::GetStateStorage();
            float value = storage->GetFloat(key, target);
            value += (target - value) * (1.0f - std::exp(-speed * ImGui::GetIO().DeltaTime));
            storage->SetFloat(key, value);
            return value;
        }

        void text(ImDrawList* draw, ImFont* font, float size, ImVec2 position, ImU32 color, const char* value)
        {
            ImGui::PushFont(font, size);
            draw->AddText(ImVec2(std::round(position.x), std::round(position.y)), color, value);
            ImGui::PopFont();
        }

        void centered(ImDrawList* draw, ImFont* font, float size, ImVec2 center, ImU32 color, const char* value)
        {
            ImGui::PushFont(font, size);
            const ImVec2 extent = ImGui::CalcTextSize(value);
            draw->AddText(ImVec2(std::round(center.x - extent.x * 0.5f), std::round(center.y - extent.y * 0.5f)), color, value);
            ImGui::PopFont();
        }

        void section(ImDrawList* draw, ImVec2 position, const char* label, float appear)
        {
            ImGui::PushFont(fonts::get(fonts::role::strong), 10.5f);
            paint::spaced_text(draw, ImVec2(std::round(position.x), std::round(position.y)), 1.3f * dpi(), paint::rgba(0.52f, 0.52f, 0.56f, appear), label);
            ImGui::PopFont();
        }

        void arrow(ImDrawList* draw, ImVec2 center, float size, int direction, ImU32 color)
        {
            ImVec2 forward{ 0.0f, -1.0f };
            if (direction == 1)
                forward = { 0.0f, 1.0f };
            else if (direction == 2)
                forward = { -1.0f, 0.0f };
            else if (direction == 3)
                forward = { 1.0f, 0.0f };
            const ImVec2 side{ -forward.y, forward.x };
            const ImVec2 tip{ center.x + forward.x * size * 0.45f, center.y + forward.y * size * 0.45f };
            const ImVec2 back{ center.x - forward.x * size * 0.35f, center.y - forward.y * size * 0.35f };
            const ImVec2 points[] = { ImVec2(back.x + side.x * size * 0.6f, back.y + side.y * size * 0.6f), tip, ImVec2(back.x - side.x * size * 0.6f, back.y - side.y * size * 0.6f) };
            paint::round_polyline(draw, points, 3, color, 1.5f * dpi());
        }

        const char* kind_of(button which)
        {
            switch (which)
            {
            case button::left_stick:
            case button::right_stick:
                return "Analog stick";
            case button::dpad:
                return "Directional pad";
            case button::left_trigger:
            case button::right_trigger:
                return "Trigger";
            case button::left_bumper:
            case button::right_bumper:
                return "Bumper";
            default:
                return which == button::guide ? "Emulation power" : model::is_paddle(which) ? "Back paddle" : "Button";
            }
        }

        struct preset
        {
            const char* label;
            std::uint32_t keys[4];
            bool mouse;
        };

        constexpr preset stick_presets[] = {
            { "WASD", { 'W', 'S', 'A', 'D' }, false },
            { "Arrows", { VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT }, false },
            { "Mouse", { 0, 0, 0, 0 }, true },
        };

        constexpr preset dpad_presets[] = {
            { "1  2  3  4", { '1', '2', '3', '4' }, false },
            { "Arrows", { VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT }, false },
        };

        void dashed_round_rect(ImDrawList* draw, ImVec2 min, ImVec2 max, float radius, ImU32 color, float thickness)
        {
            draw->PathRect(min, max, radius);
            ImVector<ImVec2> points = draw->_Path;
            draw->PathClear();
            if (points.Size < 2)
                return;
            points.push_back(points[0]);
            paint::dashed_polyline(draw, points.Data, points.Size, color, thickness, 4.0f * dpi(), 3.5f * dpi());
        }
    }

    bool binding_editor::keycap(const char* id, ImVec2 min, ImVec2 max, const std::string& value)
    {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float s = dpi();
        const int index = model::press_slot;
        ImGui::SetCursorScreenPos(min);
        const bool pressed = ImGui::InvisibleButton(id, ImVec2(max.x - min.x, max.y - min.y), ImGuiButtonFlags_PressedOnClick);
        const bool hovered = ImGui::IsItemHovered();
        const bool listening = capture_.active() && capture_.target() == index;
        if (hovered)
        {
            control_hovered_ = true;
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
        if (listening && hovered)
            slot_hovered_ = true;

        const ImGuiID key = ImGui::GetItemID();
        const float hover = animate(key, hovered ? 1.0f : 0.0f);
        const float listen = animate(key + 1, listening ? 1.0f : 0.0f, 16.0f);
        const float since = static_cast<float>(ImGui::GetTime() - assigned_at_[static_cast<std::size_t>(index)]);
        const float flash = std::clamp(1.0f - since / 0.7f, 0.0f, 1.0f);
        const float time = static_cast<float>(ImGui::GetTime());
        const float pulse = 0.5f + 0.5f * std::sin(time * 5.0f);
        const ImVec4 accent{ accent_.x, accent_.y, accent_.z, 1.0f };
        const float radius = 16.0f * s;

        ImVec4 fill = paint::mix(ImVec4(1.0f, 1.0f, 1.0f, 0.022f), ImVec4(1.0f, 1.0f, 1.0f, 0.04f), hover);
        fill = paint::mix(fill, ImVec4(accent.x, accent.y, accent.z, 0.07f), listen);
        ImVec4 edge = paint::mix(ImVec4(1.0f, 1.0f, 1.0f, 0.06f), ImVec4(1.0f, 1.0f, 1.0f, 0.11f), hover);
        edge = paint::mix(edge, ImVec4(accent.x, accent.y, accent.z, 0.35f + 0.3f * pulse), listen);
        draw->AddRectFilled(min, max, paint::to_u32(fill, appear_), radius);
        draw->AddRect(min, max, paint::to_u32(edge, appear_), radius, 0, 1.0f * s);

        const ImVec2 center{ (min.x + max.x) * 0.5f, min.y + (max.y - min.y) * 0.42f };
        const float hint_y = max.y - 15.0f * s;

        if (listening)
        {
            const float cap_w = 150.0f * s;
            const float cap_h = 42.0f * s;
            const ImVec2 cap_min{ center.x - cap_w * 0.5f, center.y - cap_h * 0.5f };
            const ImVec2 cap_max{ center.x + cap_w * 0.5f, center.y + cap_h * 0.5f };
            paint::soft_ellipse(draw, center, ImVec2(cap_w * 0.7f, cap_h * 1.1f), ImVec4(accent.x, accent.y, accent.z, 0.10f * listen * appear_), 8, 40);
            dashed_round_rect(draw, cap_min, cap_max, 10.0f * s, paint::to_u32(ImVec4(accent.x, accent.y, accent.z, 0.55f + 0.4f * pulse), appear_), 1.3f * s);
            centered(draw, fonts::get(fonts::role::strong), 14.0f, center, paint::to_u32(ImVec4(accent.x, accent.y, accent.z, 0.8f + 0.2f * pulse), appear_), "Press any key");
            centered(draw, nullptr, 11.0f, ImVec2(center.x, hint_y), paint::rgba(0.55f, 0.55f, 0.59f, appear_), "Mouse buttons and wheel work too  \xC2\xB7  click outside to cancel");
            return pressed;
        }

        if (value.empty())
        {
            const float cap_w = 120.0f * s;
            const float cap_h = 42.0f * s;
            const ImVec2 cap_min{ center.x - cap_w * 0.5f, center.y - cap_h * 0.5f };
            const ImVec2 cap_max{ center.x + cap_w * 0.5f, center.y + cap_h * 0.5f };
            dashed_round_rect(draw, cap_min, cap_max, 10.0f * s, paint::rgba(1.0f, 1.0f, 1.0f, (0.22f + 0.2f * hover) * appear_), 1.2f * s);
            centered(draw, fonts::get(fonts::role::strong), 13.0f, center, paint::rgba(0.6f, 0.6f, 0.64f, (0.8f + 0.2f * hover) * appear_), "+  Bind a key");
            centered(draw, nullptr, 11.0f, ImVec2(center.x, hint_y), paint::rgba(0.5f, 0.5f, 0.54f, (0.6f + 0.4f * hover) * appear_), "Click, then press any key or mouse button");
            return pressed;
        }

        ImGui::PushFont(fonts::get(fonts::role::strong), 18.0f);
        const ImVec2 label = ImGui::CalcTextSize(value.c_str());
        ImGui::PopFont();
        const float pop = 1.0f + 0.08f * flash * flash;
        const float cap_w = std::clamp(label.x + 44.0f * s, 64.0f * s, (max.x - min.x) - 40.0f * s) * pop;
        const float cap_h = 44.0f * s * pop;
        const float lift = hover * 1.5f * s;
        const ImVec2 cap_min{ center.x - cap_w * 0.5f, center.y - cap_h * 0.5f - lift };
        const ImVec2 cap_max{ center.x + cap_w * 0.5f, center.y + cap_h * 0.5f - lift };
        const float cap_radius = 10.0f * s;
        const float depth = 5.0f * s;

        paint::soft_ellipse(draw, ImVec2(center.x, cap_max.y + depth + 2.0f * s), ImVec2(cap_w * 0.62f, 9.0f * s), ImVec4(0.0f, 0.0f, 0.0f, 0.55f * appear_), 8, 40);
        if (flash > 0.0f)
            paint::soft_ellipse(draw, center, ImVec2(cap_w * 0.8f, cap_h * 1.2f), ImVec4(accent.x, accent.y, accent.z, 0.22f * flash * appear_), 8, 40);
        draw->AddRectFilled(ImVec2(cap_min.x, cap_min.y + depth), ImVec2(cap_max.x, cap_max.y + depth), paint::rgba(0.075f, 0.075f, 0.085f, appear_), cap_radius);
        draw->AddRect(ImVec2(cap_min.x, cap_min.y + depth), ImVec2(cap_max.x, cap_max.y + depth), paint::rgba(0.0f, 0.0f, 0.0f, 0.5f * appear_), cap_radius, 0, 1.0f * s);
        const ImVec4 face = paint::mix(ImVec4(0.15f, 0.15f, 0.165f, 1.0f), ImVec4(0.175f, 0.175f, 0.195f, 1.0f), hover);
        draw->AddRectFilled(cap_min, cap_max, paint::to_u32(face, appear_), cap_radius);
        constexpr int bands = 12;
        for (int band = 0; band < bands; ++band)
        {
            const float t = static_cast<float>(band + 1) / static_cast<float>(bands);
            draw->PushClipRect(cap_min, ImVec2(cap_max.x, cap_min.y + (cap_max.y - cap_min.y) * (1.0f - t)), true);
            draw->AddRectFilled(cap_min, cap_max, paint::rgba(1.0f, 1.0f, 1.0f, 0.0075f * appear_), cap_radius);
            draw->PopClipRect();
        }
        const ImVec4 rim = paint::mix(ImVec4(1.0f, 1.0f, 1.0f, 0.12f), accent, flash * 0.9f);
        draw->AddRect(cap_min, cap_max, paint::to_u32(rim, appear_), cap_radius, 0, 1.0f * s);
        draw->AddLine(ImVec2(cap_min.x + cap_radius, cap_min.y + 1.0f * s), ImVec2(cap_max.x - cap_radius, cap_min.y + 1.0f * s), paint::rgba(1.0f, 1.0f, 1.0f, 0.16f * appear_), 1.0f * s);

        const ImU32 text_color = paint::to_u32(paint::mix(ImVec4(0.97f, 0.97f, 0.98f, 1.0f), accent, flash * 0.5f), appear_);
        centered(draw, fonts::get(fonts::role::strong), 18.0f * pop, ImVec2(center.x, center.y - lift - 0.5f * s), text_color, value.c_str());
        centered(draw, nullptr, 11.0f, ImVec2(center.x, hint_y), paint::rgba(0.5f, 0.5f, 0.54f, (0.45f + 0.55f * hover) * appear_), "Click to change");
        return pressed;
    }

    void binding_editor::stop()
    {
        capture_.cancel();
    }

    void binding_editor::assign(model::emulation_set& set, int index, std::uint32_t code, editor_result& result)
    {
        model::assign(set, which_, index, code);
        if (index >= 0 && index < static_cast<int>(assigned_at_.size()))
            assigned_at_[static_cast<std::size_t>(index)] = ImGui::GetTime();
        result.changed = true;
    }

    bool binding_editor::slot(const char* id, ImVec2 min, ImVec2 max, int index, const std::string& value, const char* caption, int arrow_direction, bool large)
    {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float s = dpi();
        ImGui::SetCursorScreenPos(min);
        const bool pressed = ImGui::InvisibleButton(id, ImVec2(max.x - min.x, max.y - min.y), ImGuiButtonFlags_PressedOnClick);
        const bool hovered = ImGui::IsItemHovered();
        const bool listening = capture_.active() && capture_.target() == index;
        if (hovered)
        {
            control_hovered_ = true;
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
        if (listening && hovered)
            slot_hovered_ = true;

        const ImGuiID key = ImGui::GetItemID();
        const float hover = animate(key, hovered ? 1.0f : 0.0f);
        const float listen = animate(key + 1, listening ? 1.0f : 0.0f, 18.0f);
        const float since = static_cast<float>(ImGui::GetTime() - assigned_at_[static_cast<std::size_t>(index)]);
        const float flash = std::clamp(1.0f - since / 0.6f, 0.0f, 1.0f);
        const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(ImGui::GetTime()) * 6.0f);
        const float radius = large ? 14.0f * s : 10.0f * s;
        const ImVec4 accent{ accent_.x, accent_.y, accent_.z, 1.0f };

        ImVec4 fill = paint::mix(ImVec4(1.0f, 1.0f, 1.0f, 0.035f), ImVec4(1.0f, 1.0f, 1.0f, 0.065f), hover);
        fill = paint::mix(fill, ImVec4(accent.x, accent.y, accent.z, 0.12f), std::max(listen, flash * 0.9f));
        ImVec4 edge = paint::mix(ImVec4(1.0f, 1.0f, 1.0f, 0.08f), ImVec4(1.0f, 1.0f, 1.0f, 0.16f), hover);
        edge = paint::mix(edge, ImVec4(accent.x, accent.y, accent.z, 0.55f + 0.4f * pulse), listen);
        edge = paint::mix(edge, ImVec4(accent.x, accent.y, accent.z, 0.9f), flash);
        draw->AddRectFilled(min, max, paint::to_u32(fill, appear_), radius);
        draw->AddRect(min, max, paint::to_u32(edge, appear_), radius, 0, (1.0f + 0.3f * listen) * s);

        const ImVec2 center{ (min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f };
        float text_center = center.x;
        if (arrow_direction >= 0)
        {
            arrow(draw, ImVec2(min.x + 13.0f * s, center.y), 7.0f * s, arrow_direction, paint::rgba(0.6f, 0.6f, 0.64f, 0.8f * appear_));
            text_center += 6.0f * s;
        }
        if (caption)
        {
            ImGui::PushFont(nullptr, 12.0f);
            const float height = ImGui::GetFontSize();
            draw->AddText(ImVec2(std::round(min.x + 14.0f * s), std::round(center.y - height * 0.5f)), paint::rgba(0.55f, 0.55f, 0.59f, appear_), caption);
            ImGui::PopFont();
        }

        const ImU32 strong = paint::to_u32(paint::mix(ImVec4(0.96f, 0.96f, 0.97f, 1.0f), accent, flash * 0.6f), appear_);
        if (listening)
        {
            const ImU32 color = paint::to_u32(ImVec4(accent.x, accent.y, accent.z, 0.75f + 0.25f * pulse), appear_);
            if (large)
            {
                centered(draw, fonts::get(fonts::role::strong), 15.0f, ImVec2(center.x, center.y - 8.0f * s), color, "Press any key or mouse button");
                centered(draw, nullptr, 11.5f, ImVec2(center.x, center.y + 11.0f * s), paint::rgba(0.55f, 0.55f, 0.59f, appear_), "Click outside to cancel");
            }
            else
            {
                centered(draw, fonts::get(fonts::role::strong), 12.5f, ImVec2(caption ? center.x + 14.0f * s : text_center, center.y), color, "Press a key");
            }
        }
        else if (value.empty())
        {
            centered(draw, nullptr, large ? 15.0f : 12.5f, ImVec2(caption ? center.x + 14.0f * s : text_center, center.y), paint::rgba(0.45f, 0.45f, 0.49f, appear_), large ? "Click to bind" : "None");
        }
        else
        {
            centered(draw, fonts::get(fonts::role::strong), large ? 21.0f : 13.5f, ImVec2(caption ? center.x + 14.0f * s : text_center, center.y), strong, value.c_str());
        }
        return pressed;
    }

    bool binding_editor::pill(const char* id, ImVec2 min, ImVec2 max, const char* label, bool active)
    {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float s = dpi();
        ImGui::SetCursorScreenPos(min);
        const bool clicked = ImGui::InvisibleButton(id, ImVec2(max.x - min.x, max.y - min.y));
        const bool hovered = ImGui::IsItemHovered();
        if (hovered)
        {
            control_hovered_ = true;
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
        const ImGuiID key = ImGui::GetItemID();
        const float hover = animate(key, hovered ? 1.0f : 0.0f);
        const float on = animate(key + 1, active ? 1.0f : 0.0f);
        const float held = ImGui::IsItemActive() ? 0.97f : 1.0f;
        const ImVec2 center{ (min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f };
        const ImVec2 half{ (max.x - min.x) * 0.5f * held, (max.y - min.y) * 0.5f * held };
        const ImVec2 a{ center.x - half.x, center.y - half.y };
        const ImVec2 b{ center.x + half.x, center.y + half.y };

        ImVec4 fill = paint::mix(ImVec4(1.0f, 1.0f, 1.0f, 0.04f), ImVec4(1.0f, 1.0f, 1.0f, 0.08f), hover);
        fill = paint::mix(fill, ImVec4(accent_.x, accent_.y, accent_.z, 0.16f), on);
        ImVec4 edge = paint::mix(ImVec4(1.0f, 1.0f, 1.0f, 0.06f), ImVec4(1.0f, 1.0f, 1.0f, 0.14f), hover);
        edge = paint::mix(edge, ImVec4(accent_.x, accent_.y, accent_.z, 0.8f), on);
        draw->AddRectFilled(a, b, paint::to_u32(fill, appear_), half.y);
        draw->AddRect(a, b, paint::to_u32(edge, appear_), half.y, 0, 1.0f * s);
        const ImVec4 color = paint::mix(paint::mix(ImVec4(0.72f, 0.72f, 0.76f, 1.0f), ImVec4(0.97f, 0.97f, 0.98f, 1.0f), hover), ImVec4(accent_.x, accent_.y, accent_.z, 1.0f), on * 0.35f);
        centered(draw, fonts::get(fonts::role::strong), 12.0f, center, paint::to_u32(color, appear_), label);
        return clicked;
    }

    bool binding_editor::mirror_cell(const char* id, ImVec2 min, ImVec2 max, model::button option, model::family family, bool active)
    {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float s = dpi();
        ImGui::SetCursorScreenPos(min);
        const bool clicked = ImGui::InvisibleButton(id, ImVec2(max.x - min.x, max.y - min.y));
        const bool hovered = ImGui::IsItemHovered();
        if (hovered)
        {
            control_hovered_ = true;
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            ImGui::SetTooltip("%s", model::button_name(option, family));
        }
        const ImGuiID key = ImGui::GetItemID();
        const float hover = animate(key, hovered ? 1.0f : 0.0f);
        const float on = animate(key + 1, active ? 1.0f : 0.0f);
        const float radius = 9.0f * s;

        ImVec4 fill = paint::mix(ImVec4(1.0f, 1.0f, 1.0f, 0.03f), ImVec4(1.0f, 1.0f, 1.0f, 0.07f), hover);
        fill = paint::mix(fill, ImVec4(accent_.x, accent_.y, accent_.z, 0.14f), on);
        ImVec4 edge = paint::mix(ImVec4(1.0f, 1.0f, 1.0f, 0.05f), ImVec4(1.0f, 1.0f, 1.0f, 0.13f), hover);
        edge = paint::mix(edge, ImVec4(accent_.x, accent_.y, accent_.z, 0.85f), on);
        draw->AddRectFilled(min, max, paint::to_u32(fill, appear_), radius);
        draw->AddRect(min, max, paint::to_u32(edge, appear_), radius, 0, 1.0f * s);

        const ImVec2 center{ (min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f };
        const ImVec4 tint = paint::mix(ImVec4(0.8f, 0.8f, 0.84f, 1.0f), ImVec4(accent_.x, accent_.y, accent_.z, 1.0f), on);
        if (const icons::icon* image = icons::get(option, family, icons::style::solid))
            icons::draw(draw, *image, center, (max.y - min.y) * 0.6f, paint::to_u32(tint, appear_ * (0.8f + 0.2f * std::max(hover, on))));
        else
            widgets::button_glyph(draw, center, (max.y - min.y) * 0.3f, option, family, paint::to_u32(tint, appear_));
        return clicked;
    }

    editor_result binding_editor::draw(ImVec2 min, ImVec2 max, model::button which, model::family family, model::emulation_set& set,
        std::array<float, 4>* tuning, const power_state* power, const ImVec4& accent, float appear)
    {
        editor_result result;
        if (which != which_)
        {
            stop();
            which_ = which;
            assigned_at_.fill(-10.0);
        }

        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float s = dpi();
        accent_ = accent;
        appear_ = std::clamp(appear, 0.0f, 1.0f);
        slot_hovered_ = false;
        control_hovered_ = false;

        const float slide = (1.0f - appear_) * 26.0f * s;
        min.x += slide;
        max.x += slide;
        const float corner = 18.0f * s;
        paint::soft_ellipse(draw, ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f + 18.0f * s), ImVec2((max.x - min.x) * 0.62f, (max.y - min.y) * 0.58f),
            ImVec4(0.0f, 0.0f, 0.0f, 0.35f * appear_), 8, 48);
        draw->AddRectFilled(min, max, paint::rgba(0.05f, 0.05f, 0.058f, 0.84f * appear_), corner);
        draw->AddRect(min, max, paint::rgba(1.0f, 1.0f, 1.0f, 0.075f * appear_), corner, 0, 1.0f);
        paint::vertical_gradient(draw, ImVec2(min.x + corner, min.y + 1.0f), ImVec2(max.x - corner, min.y + 2.0f), paint::rgba(1.0f, 1.0f, 1.0f, 0.06f * appear_), paint::rgba(1.0f, 1.0f, 1.0f, 0.0f));

        const float pad = 20.0f * s;
        const float left = min.x + pad;
        const float right = max.x - pad;
        const float width = right - left;
        float y = min.y + pad;

        const ImVec2 icon_center{ left + 17.0f * s, y + 17.0f * s };
        if (const icons::icon* image = icons::get(which, family, icons::style::solid))
            icons::draw(draw, *image, icon_center, 32.0f * s, paint::rgba(0.97f, 0.97f, 0.98f, appear_));
        else
        {
            draw->AddCircle(icon_center, 15.0f * s, paint::rgba(0.97f, 0.97f, 0.98f, appear_), 32, 1.6f * s);
            widgets::button_glyph(draw, icon_center, 15.0f * s, which, family, paint::rgba(0.97f, 0.97f, 0.98f, appear_));
        }
        text(draw, fonts::get(fonts::role::strong), 17.0f, ImVec2(left + 46.0f * s, y - 1.0f * s), paint::rgba(0.97f, 0.97f, 0.98f, appear_), model::button_name(which, family));
        text(draw, nullptr, 12.0f, ImVec2(left + 46.0f * s, y + 20.0f * s), paint::rgba(0.53f, 0.53f, 0.57f, appear_), kind_of(which));

        {
            const ImVec2 close_center{ right - 11.0f * s, y + 11.0f * s };
            const float close_radius = 13.0f * s;
            ImGui::SetCursorScreenPos(ImVec2(close_center.x - close_radius, close_center.y - close_radius));
            if (ImGui::InvisibleButton("##close", ImVec2(close_radius * 2.0f, close_radius * 2.0f)))
                result.close = true;
            const bool hovered = ImGui::IsItemHovered();
            if (hovered)
            {
                control_hovered_ = true;
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }
            const float hover = animate(ImGui::GetItemID(), hovered ? 1.0f : 0.0f);
            draw->AddCircleFilled(close_center, close_radius, paint::rgba(1.0f, 1.0f, 1.0f, (0.04f + 0.06f * hover) * appear_), 32);
            const float arm = 4.0f * s;
            const ImU32 color = paint::rgba(0.85f, 0.85f, 0.88f, (0.7f + 0.3f * hover) * appear_);
            draw->AddLine(ImVec2(close_center.x - arm, close_center.y - arm), ImVec2(close_center.x + arm, close_center.y + arm), color, 1.5f * s);
            draw->AddLine(ImVec2(close_center.x - arm, close_center.y + arm), ImVec2(close_center.x + arm, close_center.y - arm), color, 1.5f * s);
        }
        y += 56.0f * s;

        const bool directional = model::has_directions(which);
        const bool stick = which == button::left_stick || which == button::right_stick;
        const bool motion = stick && set[static_cast<std::size_t>(which)].mouse_motion;
        const float gap = 6.0f * s;

        if (directional)
        {
            section(draw, ImVec2(left, y), "DIRECTIONS", appear_);
            y += 20.0f * s;
            const float cell_w = (width - gap * 2.0f) / 3.0f;
            const float cell_h = 34.0f * s;
            const ImVec2 cells[4] = {
                ImVec2(left + cell_w + gap, y),
                ImVec2(left + cell_w + gap, y + (cell_h + gap) * 2.0f),
                ImVec2(left, y + cell_h + gap),
                ImVec2(left + (cell_w + gap) * 2.0f, y + cell_h + gap),
            };
            const char* ids[4] = { "##up", "##down", "##left", "##right" };
            for (int d = 0; d < 4; ++d)
            {
                const ImVec2 a = cells[d];
                const ImVec2 b{ a.x + cell_w, a.y + cell_h };
                const std::string value = motion ? std::string("Mouse") : model::input_name(model::slot(set, which, d));
                if (slot(ids[d], a, b, d, value, nullptr, d, false) && !(capture_.active() && capture_.target() == d))
                    capture_.begin(d);
            }
            const ImVec2 hub{ left + cell_w * 1.5f + gap, y + cell_h * 1.5f + gap };
            draw->AddCircle(hub, 10.0f * s, paint::rgba(1.0f, 1.0f, 1.0f, 0.12f * appear_), 32, 1.2f * s);
            draw->AddCircleFilled(hub, 3.0f * s, paint::rgba(1.0f, 1.0f, 1.0f, (motion ? 0.7f : 0.25f) * appear_), 16);
            y += cell_h * 3.0f + gap * 2.0f + 12.0f * s;

            if (stick)
            {
                const ImVec2 a{ left, y };
                const ImVec2 b{ right, y + 34.0f * s };
                if (slot("##click", a, b, model::press_slot, model::input_name(model::slot(set, which, model::press_slot)), "Click", -1, false) &&
                    !(capture_.active() && capture_.target() == model::press_slot))
                    capture_.begin(model::press_slot);
                y += 34.0f * s + 14.0f * s;
            }

            const float row_h = 28.0f * s;
            const preset* presets = stick ? stick_presets : dpad_presets;
            const std::size_t count = stick ? std::size(stick_presets) : std::size(dpad_presets);
            const float w = (width - gap * static_cast<float>(count - 1)) / static_cast<float>(count);
            for (std::size_t i = 0; i < count; ++i)
            {
                const preset& option = presets[i];
                bool active = option.mouse ? motion : !motion;
                for (int d = 0; d < 4 && active && !option.mouse; ++d)
                    active = model::slot(set, which, d) == option.keys[d];
                const ImVec2 a{ left + (w + gap) * static_cast<float>(i), y };
                ImGui::PushID(static_cast<int>(i));
                if (pill("##preset", a, ImVec2(a.x + w, a.y + row_h), option.label, active))
                {
                    stop();
                    if (option.mouse)
                    {
                        model::set_mouse_motion(set, which, true);
                        result.changed = true;
                    }
                    else
                    {
                        for (int d = 0; d < 4; ++d)
                            assign(set, d, option.keys[d], result);
                    }
                }
                ImGui::PopID();
            }
            y += row_h + 18.0f * s;

            if (stick && tuning)
            {
                constexpr const char* labels[] = { "Dead zone", "Horizontal", "Vertical", "Curve" };
                const float radius = 16.0f * s;
                const float cell = width / 4.0f;
                const float center_y = y + 20.0f * s + radius;
                for (std::size_t i = 0; i < std::size(labels); ++i)
                {
                    ImGui::PushID(static_cast<int>(i) + 40);
                    widgets::knob("##tune", labels[i], ImVec2(left + cell * (static_cast<float>(i) + 0.5f), center_y), radius, (*tuning)[i], accent);
                    if (ImGui::IsItemHovered())
                        control_hovered_ = true;
                    ImGui::PopID();
                }
            }
        }
        else if (which == button::guide)
        {
            section(draw, ImVec2(left, y), "POWER KEY", appear_);
            y += 20.0f * s;
            const float big_h = 92.0f * s;
            const std::uint32_t power_key = model::slot(set, which, model::press_slot);
            if (keycap("##press", ImVec2(left, y), ImVec2(right, y + big_h), model::input_name(power_key)) &&
                !(capture_.active() && capture_.target() == model::press_slot))
                capture_.begin(model::press_slot);
            y += big_h + 14.0f * s;

            if (power && !power->error.empty())
            {
                ImGui::PushFont(nullptr, 12.0f);
                draw->AddText(nullptr, 0.0f, ImVec2(left + 2.0f * s, y), paint::rgba(1.0f, 0.5f, 0.45f, 0.95f * appear_), power->error.c_str(), nullptr, width - 4.0f * s);
                ImGui::PopFont();
            }
        }
        else
        {
            section(draw, ImVec2(left, y), "KEY", appear_);
            y += 20.0f * s;
            const float big_h = 92.0f * s;
            if (keycap("##press", ImVec2(left, y), ImVec2(right, y + big_h), model::input_name(model::slot(set, which, model::press_slot))) &&
                !(capture_.active() && capture_.target() == model::press_slot))
                capture_.begin(model::press_slot);
            y += big_h + 18.0f * s;

            if (model::needs_mirror(which))
            {
                section(draw, ImVec2(left, y), "ACTS AS", appear_);
                y += 20.0f * s;
                const float cell_w = (width - gap * 4.0f) / 5.0f;
                const float cell_h = 34.0f * s;
                auto& mirror = set[static_cast<std::size_t>(which)].mirror;
                for (std::size_t i = 0; i < std::size(mirror_options); ++i)
                {
                    const float column = static_cast<float>(i % 5);
                    const float row = static_cast<float>(i / 5);
                    const ImVec2 a{ left + (cell_w + gap) * column, y + (cell_h + gap) * row };
                    ImGui::PushID(static_cast<int>(i) + 20);
                    if (mirror_cell("##mirror", a, ImVec2(a.x + cell_w, a.y + cell_h), mirror_options[i], family, mirror == mirror_options[i]))
                    {
                        mirror = mirror == mirror_options[i] ? button::count : mirror_options[i];
                        result.changed = true;
                    }
                    ImGui::PopID();
                }
            }
        }

        {
            const float row_h = 28.0f * s;
            const float w = (width - gap) * 0.5f;
            const float top = max.y - pad - row_h;
            if (pill("##clear", ImVec2(left, top), ImVec2(left + w, top + row_h), "Clear", false))
            {
                stop();
                model::clear(set, which);
                assigned_at_.fill(ImGui::GetTime());
                result.changed = true;
            }
            if (pill("##default", ImVec2(left + w + gap, top), ImVec2(right, top + row_h), "Default", false))
            {
                stop();
                const model::emulation_set defaults = model::default_emulation();
                model::clear(set, which);
                for (int index = 0; index <= model::press_slot; ++index)
                    model::assign(set, which, index, model::slot(defaults, which, index));
                if (which != button::dpad)
                {
                    if (defaults[static_cast<std::size_t>(which)].mouse_motion)
                        model::set_mouse_motion(set, which, true);
                    set[static_cast<std::size_t>(which)].mirror = defaults[static_cast<std::size_t>(which)].mirror;
                }
                assigned_at_.fill(ImGui::GetTime());
                result.changed = true;
            }
        }

        if (ImGui::IsMouseHoveringRect(min, max))
            control_hovered_ = true;

        if (capture_.active())
        {
            if (const auto code = capture_.poll(slot_hovered_))
            {
                const bool wheel = *code == model::wheel_up_code || *code == model::wheel_down_code;
                if (!(which_ == button::guide && wheel))
                {
                    const int target = capture_.target();
                    stop();
                    assign(set, target, *code, result);
                }
            }
            else if (capture_.active() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !slot_hovered_ && !control_hovered_)
            {
                stop();
            }
        }
        return result;
    }
}
