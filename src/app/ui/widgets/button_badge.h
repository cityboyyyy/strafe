#pragma once

#include "../../model/buttons.h"

#include <imgui.h>

namespace menu::ui::widgets
{
    struct badge_result
    {
        bool clicked = false;
        bool hovered = false;
    };

    void button_glyph(ImDrawList* draw, ImVec2 center, float size, model::button which, model::family style, ImU32 color);
    badge_result button_badge(const char* id, ImVec2 center, float radius, model::button which, model::family style, float hover, float select, const ImVec4& accent);
}
