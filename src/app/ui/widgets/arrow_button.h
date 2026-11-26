#pragma once

#include "motion.h"

#include <imgui.h>

namespace menu::ui::widgets
{
    class arrow_button
    {
    public:
        bool draw(const char* id, ImVec2 center, float height, int direction, const ImVec4& accent);
        void trigger();
        static float width(float height);

    private:
        double started_ = -1.0;
        motion::spring hover_;
    };
}
