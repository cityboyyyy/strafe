#pragma once

#include "../../model/buttons.h"

#include <imgui.h>

namespace menu::ui::widgets
{
    enum class chip_side
    {
        left,
        right,
        top,
        bottom
    };

    struct chip_result
    {
        bool clicked = false;
        bool hovered = false;
        ImVec2 port{};
    };

    struct chip_style
    {
        float hover = 0.0f;
        float select = 0.0f;
        float appear = 1.0f;
        bool bound = false;
    };

    float callout_chip_width(model::button which, model::family family, const char* text, float height);
    chip_result callout_chip(const char* id, ImVec2 glyph_center, chip_side side, float height, model::button which, model::family family,
        const char* text, const chip_style& look, const ImVec4& accent);
}
