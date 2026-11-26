#pragma once

#include <imgui.h>

namespace menu::ui::widgets
{
    bool knob(const char* id, const char* label, ImVec2 center, float radius, float& value, const ImVec4& accent);
}
