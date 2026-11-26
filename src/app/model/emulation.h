#pragma once

#include "buttons.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

namespace menu::model
{
    constexpr std::uint32_t wheel_up_code = 0x10001;
    constexpr std::uint32_t wheel_down_code = 0x10002;
    constexpr int press_slot = 4;

    struct emulated_input
    {
        std::uint32_t press = 0;
        std::array<std::uint32_t, 4> move{};
        bool mouse_motion = false;
        button mirror = button::count;
    };

    using emulation_set = std::array<emulated_input, static_cast<std::size_t>(button::count)>;

    emulation_set default_emulation();
    bool has_directions(button which);
    bool has_press(button which);
    bool is_paddle(button which);
    bool needs_mirror(button which);

    std::uint32_t slot(const emulation_set& set, button which, int index);
    void assign(emulation_set& set, button which, int index, std::uint32_t code);
    void set_mouse_motion(emulation_set& set, button which, bool enabled);
    void clear(emulation_set& set, button which);
    bool is_bound(const emulation_set& set, button which);

    std::string input_name(std::uint32_t code);
    std::string describe(const emulation_set& set, button which);
    bool is_held(std::uint32_t code);

    std::filesystem::path emulation_path(const wchar_t* model_file);
    bool save_emulation(const std::filesystem::path& path, const emulation_set& set);
    bool load_emulation(const std::filesystem::path& path, emulation_set& set);
}
