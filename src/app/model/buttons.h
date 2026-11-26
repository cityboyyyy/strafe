#pragma once

#include <cstdint>

namespace menu::model
{
    enum class button : std::uint8_t
    {
        south,
        east,
        west,
        north,
        left_bumper,
        right_bumper,
        left_trigger,
        right_trigger,
        dpad,
        dpad_up,
        dpad_down,
        dpad_left,
        dpad_right,
        left_stick,
        right_stick,
        view,
        menu,
        guide,
        touchpad,
        mute,
        paddle_1,
        paddle_2,
        paddle_3,
        paddle_4,
        count
    };

    enum class family : std::uint8_t
    {
        xbox,
        playstation
    };

    const char* button_name(button which, family style);
}
