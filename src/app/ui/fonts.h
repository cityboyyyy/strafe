#pragma once

#include <imgui.h>

namespace menu::ui::fonts
{
    enum class role
    {
        body,
        strong,
        mono
    };

    void load();
    ImFont* get(role which);
}
