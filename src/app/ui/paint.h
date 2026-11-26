#pragma once

#include <imgui.h>

namespace menu::ui::paint
{
    ImU32 rgba(float r, float g, float b, float a);
    ImVec4 mix(const ImVec4& from, const ImVec4& to, float t);
    ImU32 to_u32(const ImVec4& color, float alpha_scale = 1.0f);

    void soft_ellipse(ImDrawList* draw, ImVec2 center, ImVec2 radius, const ImVec4& color, int rings = 10, int segments = 72);
    void vertical_gradient(ImDrawList* draw, ImVec2 min, ImVec2 max, ImU32 top, ImU32 bottom);
    ImVec2 spaced_text_size(const char* text, float spacing);
    void spaced_text(ImDrawList* draw, ImVec2 position, float spacing, ImU32 color, const char* text);
    void chevron(ImDrawList* draw, ImVec2 center, float size, int direction, ImU32 color, float thickness);
    void render_image(ImDrawList* draw, void* texture, ImVec2 min, ImVec2 max, ImU32 tint);
    void rounded_path(ImDrawList* draw, ImVec2 min, ImVec2 max, float top_left, float top_right, float bottom_right, float bottom_left);
    void round_polyline(ImDrawList* draw, const ImVec2* points, int count, ImU32 color, float thickness);
    void dashed_polyline(ImDrawList* draw, const ImVec2* points, int count, ImU32 color, float thickness, float dash, float gap);
}
