#pragma once

#include <engine/types.h>

#include <cstdint>
#include <optional>
#include <string_view>

namespace engine::input
{
    std::optional<std::string_view> button_id(control_button button);
    std::optional<control_button> button_from_id(std::string_view id);

    struct gamepad
    {
        std::uint16_t buttons = 0;
        std::uint8_t left_trigger = 0;
        std::uint8_t right_trigger = 0;
        std::int16_t left_x = 0;
        std::int16_t left_y = 0;
        std::int16_t right_x = 0;
        std::int16_t right_y = 0;
    };

    class controller
    {
    public:
        bool try_read(std::uint32_t slot, control_button& buttons, gamepad& raw);
        void reset();

        static control_button decode(const gamepad& pad, control_button previous);
        static bool connected(std::uint32_t slot);

    private:
        control_button previous_ = control_button::none;
    };
}
