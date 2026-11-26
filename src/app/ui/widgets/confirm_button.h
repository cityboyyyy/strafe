#pragma once

#include "motion.h"

#include <imgui.h>

namespace menu::ui::widgets
{
    class confirm_button
    {
    public:
        bool draw(const char* id, ImVec2 center, float height);
        bool busy() const { return started_ >= 0.0; }
        float idle_width(float height) const;

    private:
        double started_ = -1.0;
        bool reported_ = false;
        motion::spring width_;
    };
}
