#pragma once

#include "../model/buttons.h"

#include <imgui.h>

namespace menu::ui::icons
{
    enum class style
    {
        outline,
        solid
    };

    struct icon
    {
        void* texture = nullptr;
        float aspect = 1.0f;
    };

    bool load();
    void release();
    const icon* get(model::button which, model::family family, style look);
    void draw(ImDrawList* draw, const icon& image, ImVec2 center, float height, ImU32 tint);
}
