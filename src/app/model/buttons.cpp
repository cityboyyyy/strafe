#include "buttons.h"

#include <array>

namespace menu::model
{
    namespace
    {
        constexpr std::array<const char*, static_cast<std::size_t>(button::count)> xbox_names{
            "A", "B", "X", "Y",
            "Left bumper", "Right bumper", "Left trigger", "Right trigger",
            "D-pad", "D-pad up", "D-pad down", "D-pad left", "D-pad right",
            "Left stick", "Right stick",
            "View", "Menu", "Xbox button", "Touchpad", "Share",
            "P1", "P2", "P3", "P4",
        };

        constexpr std::array<const char*, static_cast<std::size_t>(button::count)> playstation_names{
            "Cross", "Circle", "Square", "Triangle",
            "L1", "R1", "L2", "R2",
            "D-pad", "D-pad up", "D-pad down", "D-pad left", "D-pad right",
            "Left stick", "Right stick",
            "Create", "Options", "PS button", "Touchpad", "Mute",
            "Back left", "Back right", "Back left 2", "Back right 2",
        };
    }

    const char* button_name(button which, family style)
    {
        const auto index = static_cast<std::size_t>(which);
        if (index >= xbox_names.size())
            return "";
        return style == family::playstation ? playstation_names[index] : xbox_names[index];
    }
}
