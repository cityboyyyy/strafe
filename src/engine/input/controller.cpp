#include "controller.h"

#include <windows.h>
#include <xinput.h>

namespace engine::input
{
    namespace
    {
        constexpr control_button digital_buttons = control_button::dpad_up | control_button::dpad_down |
            control_button::dpad_left | control_button::dpad_right | control_button::start | control_button::back |
            control_button::left_stick_press | control_button::right_stick_press | control_button::lb |
            control_button::rb | control_button::a | control_button::b | control_button::x | control_button::y;

        control_button from_axis(int value, control_button button, control_button previous, int press = 18000,
            int release = 14000)
        {
            const int threshold = any(previous & button) ? release : press;
            return value >= threshold ? button : control_button::none;
        }

        struct button_row
        {
            control_button button;
            std::string_view id;
        };

        constexpr button_row button_rows[] =
        {
            { control_button::dpad_up, "DPadUp" },
            { control_button::dpad_down, "DPadDown" },
            { control_button::dpad_left, "DPadLeft" },
            { control_button::dpad_right, "DPadRight" },
            { control_button::start, "Start" },
            { control_button::back, "Back" },
            { control_button::left_stick_press, "LeftStickPress" },
            { control_button::right_stick_press, "RightStickPress" },
            { control_button::lb, "LB" },
            { control_button::rb, "RB" },
            { control_button::guide, "Guide" },
            { control_button::a, "A" },
            { control_button::b, "B" },
            { control_button::x, "X" },
            { control_button::y, "Y" },
            { control_button::lt, "LT" },
            { control_button::rt, "RT" },
            { control_button::left_stick_up, "LeftStickUp" },
            { control_button::left_stick_down, "LeftStickDown" },
            { control_button::left_stick_left, "LeftStickLeft" },
            { control_button::left_stick_right, "LeftStickRight" },
            { control_button::right_stick_up, "RightStickUp" },
            { control_button::right_stick_down, "RightStickDown" },
            { control_button::right_stick_left, "RightStickLeft" },
            { control_button::right_stick_right, "RightStickRight" },
            { control_button::touchpad, "Touchpad" },
            { control_button::mute, "Mute" },
            { control_button::paddle_1, "Paddle1" },
            { control_button::paddle_2, "Paddle2" },
            { control_button::paddle_3, "Paddle3" },
            { control_button::paddle_4, "Paddle4" },
            { control_button::left_paddle, "LeftPaddle" },
            { control_button::right_paddle, "RightPaddle" },
            { control_button::left_function, "LeftFunction" },
            { control_button::right_function, "RightFunction" }
        };
    }

    std::optional<std::string_view> button_id(control_button button)
    {
        for (const auto& row : button_rows)
        {
            if (row.button == button)
                return row.id;
        }
        return std::nullopt;
    }

    std::optional<control_button> button_from_id(std::string_view id)
    {
        for (const auto& row : button_rows)
        {
            if (row.id == id)
                return row.button;
        }
        return std::nullopt;
    }

    bool controller::try_read(std::uint32_t slot, control_button& buttons, gamepad& raw)
    {
        XINPUT_STATE state{};
        if (XInputGetState(slot, &state) != ERROR_SUCCESS)
        {
            reset();
            buttons = control_button::none;
            raw = {};
            return false;
        }

        raw.buttons = state.Gamepad.wButtons;
        raw.left_trigger = state.Gamepad.bLeftTrigger;
        raw.right_trigger = state.Gamepad.bRightTrigger;
        raw.left_x = state.Gamepad.sThumbLX;
        raw.left_y = state.Gamepad.sThumbLY;
        raw.right_x = state.Gamepad.sThumbRX;
        raw.right_y = state.Gamepad.sThumbRY;
        buttons = decode(raw, previous_);
        previous_ = buttons;
        return true;
    }

    void controller::reset()
    {
        previous_ = control_button::none;
    }

    bool controller::connected(std::uint32_t slot)
    {
        XINPUT_STATE state{};
        return XInputGetState(slot, &state) == ERROR_SUCCESS;
    }

    control_button controller::decode(const gamepad& pad, control_button previous)
    {
        control_button buttons = static_cast<control_button>(pad.buttons) & digital_buttons;
        buttons |= from_axis(pad.left_trigger, control_button::lt, previous, 36, 26);
        buttons |= from_axis(pad.right_trigger, control_button::rt, previous, 36, 26);
        buttons |= from_axis(pad.left_y, control_button::left_stick_up, previous);
        buttons |= from_axis(-pad.left_y, control_button::left_stick_down, previous);
        buttons |= from_axis(-pad.left_x, control_button::left_stick_left, previous);
        buttons |= from_axis(pad.left_x, control_button::left_stick_right, previous);
        buttons |= from_axis(pad.right_y, control_button::right_stick_up, previous);
        buttons |= from_axis(-pad.right_y, control_button::right_stick_down, previous);
        buttons |= from_axis(-pad.right_x, control_button::right_stick_left, previous);
        buttons |= from_axis(pad.right_x, control_button::right_stick_right, previous);
        return buttons;
    }
}
