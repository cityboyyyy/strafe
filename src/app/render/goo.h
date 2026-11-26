#pragma once

#include <imgui.h>

namespace menu::render::goo
{
    struct shape
    {
        ImVec2 pill_min{};
        ImVec2 pill_max{};
        float pill_radius = 0.0f;
        ImVec2 bubble_center{};
        float bubble_radius = 0.0f;
        float smoothness = 0.0f;
        ImU32 pill_color = 0;
        ImU32 bubble_color = 0;
        ImU32 glow_color = 0;
        float glow_start = 0.0f;
        float glow_edge = 0.0f;
        float glow_feather = 0.0f;
        float glow_alpha = 0.0f;
        float shadow_alpha = 0.0f;
        float shadow_blur = 0.0f;
        float shadow_offset = 0.0f;
        float highlight = 0.0f;
        float alpha = 1.0f;
    };

    bool available();
    void draw(ImDrawList* draw_list, const shape& value);
    void release();
}
