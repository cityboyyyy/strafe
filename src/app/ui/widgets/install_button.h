#pragma once

#include <imgui.h>

namespace menu::ui::widgets
{
    enum class install_phase
    {
        idle,
        working,
        done,
        failed
    };

    bool install_button(const char* id, ImVec2 right_center, float visibility, install_phase phase, float progress, const ImVec4& accent);
}
