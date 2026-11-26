#include "callout_chip.h"
#include "button_badge.h"
#include "../fonts.h"
#include "../icons.h"
#include "../paint.h"

#include <algorithm>
#include <cmath>

namespace menu::ui::widgets
{
    namespace
    {
        constexpr float text_size = 12.5f;

        float icon_height(const icons::icon* image, float height)
        {
            const float base = height - 4.0f * ImGui::GetStyle().FontScaleDpi;
            if (!image || image->aspect <= 1.05f)
                return base;
            return std::min(base, base * 1.55f / image->aspect);
        }

        float slot_width(const icons::icon* image, float height)
        {
            if (!image)
                return height;
            return std::max(height, icon_height(image, height) * image->aspect + 8.0f * ImGui::GetStyle().FontScaleDpi);
        }

        float padding(float height)
        {
            return height * 0.42f;
        }
    }

    float callout_chip_width(model::button which, model::family family, const char* text, float height)
    {
        const float slot = slot_width(icons::get(which, family, icons::style::solid), height);
        if (!text || !*text)
            return slot;
        ImGui::PushFont(nullptr, text_size);
        const float label = ImGui::CalcTextSize(text).x;
        ImGui::PopFont();
        return slot + padding(height) * 0.5f + label + padding(height);
    }

    chip_result callout_chip(const char* id, ImVec2 glyph_center, chip_side side, float height, model::button which, model::family family,
        const char* text, const chip_style& look, const ImVec4& accent)
    {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float s = ImGui::GetStyle().FontScaleDpi;
        const icons::icon* solid = icons::get(which, family, icons::style::solid);
        const float slot = slot_width(solid, height);
        const float width = callout_chip_width(which, family, text, height);
        const bool text_left = side == chip_side::left;
        const float appear = std::clamp(look.appear, 0.0f, 1.0f);
        const float slide = (1.0f - appear) * 12.0f * s;

        switch (side)
        {
        case chip_side::left:
            glyph_center.x += slide;
            break;
        case chip_side::right:
            glyph_center.x -= slide;
            break;
        case chip_side::top:
            glyph_center.y += slide;
            break;
        case chip_side::bottom:
            glyph_center.y -= slide;
            break;
        }

        const ImVec2 min{ text_left ? glyph_center.x + slot * 0.5f - width : glyph_center.x - slot * 0.5f, glyph_center.y - height * 0.5f };
        const ImVec2 max{ min.x + width, min.y + height };

        ImGui::SetCursorScreenPos(min);
        chip_result result;
        result.clicked = ImGui::InvisibleButton(id, ImVec2(width, height));
        result.hovered = ImGui::IsItemHovered();
        if (result.hovered)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        switch (side)
        {
        case chip_side::left:
            result.port = ImVec2(max.x, glyph_center.y);
            break;
        case chip_side::right:
            result.port = ImVec2(min.x, glyph_center.y);
            break;
        case chip_side::top:
            result.port = ImVec2(glyph_center.x, max.y);
            break;
        case chip_side::bottom:
            result.port = ImVec2(glyph_center.x, min.y);
            break;
        }

        if (appear <= 0.001f)
            return result;

        const float lift = std::max(look.hover, look.select);
        const float radius = height * 0.5f;
        const ImVec4 fill = paint::mix(ImVec4(0.07f, 0.07f, 0.08f, 0.82f), ImVec4(0.10f, 0.10f, 0.115f, 0.95f), lift);
        const ImVec4 edge = paint::mix(ImVec4(1.0f, 1.0f, 1.0f, 0.07f), ImVec4(accent.x, accent.y, accent.z, 0.9f), std::max(look.select, look.hover * 0.55f));
        if (lift > 0.01f)
            paint::soft_ellipse(draw, ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f), ImVec2(width * 0.62f, height * 1.05f), ImVec4(accent.x, accent.y, accent.z, 0.14f * lift * appear), 8, 40);
        draw->AddRectFilled(min, max, paint::to_u32(fill, appear), radius);
        draw->AddRect(min, max, paint::to_u32(edge, appear), radius, 0, 1.0f);

        const float bloom = 1.0f + 0.08f * lift;
        const ImVec4 tint = paint::mix(ImVec4(0.93f, 0.93f, 0.95f, 1.0f), ImVec4(accent.x, accent.y, accent.z, 1.0f), look.select);
        if (solid)
        {
            const float ih = icon_height(solid, height) * bloom;
            icons::draw(draw, *solid, glyph_center, ih, paint::to_u32(tint, appear * (0.86f + 0.14f * lift)));
        }
        else
        {
            const float inner = (radius - 3.0f * s) * bloom;
            draw->AddCircle(glyph_center, inner, paint::to_u32(tint, appear), 32, 1.6f * s);
            button_glyph(draw, glyph_center, inner * 1.0f, which, family, paint::to_u32(tint, appear));
        }

        if (!text || !*text)
            return result;

        ImGui::PushFont(look.bound ? fonts::get(fonts::role::strong) : nullptr, text_size);
        const ImVec2 label = ImGui::CalcTextSize(text);
        const float text_y = std::round(glyph_center.y - label.y * 0.5f);
        const float text_x = text_left ? min.x + padding(height) : glyph_center.x + slot * 0.5f + padding(height) * 0.5f;
        const ImVec4 color = look.bound ? ImVec4(0.95f, 0.95f, 0.97f, 1.0f) : paint::mix(ImVec4(0.56f, 0.56f, 0.6f, 1.0f), ImVec4(0.9f, 0.9f, 0.93f, 1.0f), lift);
        draw->AddText(ImVec2(std::round(text_x), text_y), paint::to_u32(color, appear), text);
        ImGui::PopFont();
        return result;
    }
}
