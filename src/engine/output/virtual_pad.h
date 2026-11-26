#pragma once

#include <engine/types.h>

namespace engine::output
{
    struct xbox_motion
    {
        int left_x = 0;
        int left_y = 0;
        int right_x = 0;
        int right_y = 0;

        bool add(control_button target, int amount);

        friend bool operator==(const xbox_motion&, const xbox_motion&) = default;
    };

    class virtual_pad
    {
    public:
        virtual ~virtual_pad() = default;
        virtual bool set_buttons(control_button buttons, xbox_motion motion = {}) = 0;
        virtual void dispose() = 0;
    };
}
