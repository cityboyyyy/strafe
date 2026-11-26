#pragma once

#include <imgui.h>

namespace menu::ui::widgets
{
    float segmented_width(const char* const* labels, int count);
    bool segmented(const char* id, const char* const* labels, int count, int& index, ImVec2 center);
}
