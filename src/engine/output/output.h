#pragma once

#include <engine/types.h>

#include <windows.h>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace engine::output
{
    inline constexpr std::array<output_action, 5> mouse_buttons =
    {
        output_action::mouse_left, output_action::mouse_right, output_action::mouse_middle,
        output_action::mouse_back, output_action::mouse_forward
    };

    namespace keys
    {
        inline constexpr std::uint32_t back = 0x08;
        inline constexpr std::uint32_t tab = 0x09;
        inline constexpr std::uint32_t enter = 0x0D;
        inline constexpr std::uint32_t shift = 0x10;
        inline constexpr std::uint32_t control = 0x11;
        inline constexpr std::uint32_t menu = 0x12;
        inline constexpr std::uint32_t pause = 0x13;
        inline constexpr std::uint32_t escape = 0x1B;
        inline constexpr std::uint32_t space = 0x20;
        inline constexpr std::uint32_t left = 0x25;
        inline constexpr std::uint32_t up = 0x26;
        inline constexpr std::uint32_t right = 0x27;
        inline constexpr std::uint32_t down = 0x28;
        inline constexpr std::uint32_t print_screen = 0x2C;
        inline constexpr std::uint32_t d0 = 0x30;
        inline constexpr std::uint32_t d9 = 0x39;
        inline constexpr std::uint32_t f12 = 0x7B;
        inline constexpr std::uint32_t packet = 0xE7;
        inline constexpr std::uint32_t left_shift = 0xA0;
        inline constexpr std::uint32_t right_shift = 0xA1;
        inline constexpr std::uint32_t left_control = 0xA2;
        inline constexpr std::uint32_t right_control = 0xA3;
        inline constexpr std::uint32_t left_menu = 0xA4;
        inline constexpr std::uint32_t right_menu = 0xA5;
    }

    constexpr output_action key_action(std::uint32_t key)
    {
        return static_cast<output_action>(key);
    }

    constexpr std::uint32_t code(output_action action)
    {
        return static_cast<std::uint32_t>(action);
    }

    std::string name(output_action action);
    bool is_key(std::uint32_t key);
    bool is_xbox(output_action action);
    control_button xbox_button(output_action action);
    bool is_xbox_button(control_button button);
    output_action for_xbox(control_button button);
    std::string serialize(output_action action);
    bool try_parse(std::string_view value, output_action& action);
    std::uint32_t normalize_modifier(std::uint32_t key);
    std::uint32_t capture_key(std::uint32_t key, std::int64_t message_flags);

    class action_set
    {
    public:
        bool insert(output_action action);
        bool erase(output_action action);
        bool contains(output_action action) const;
        void clear() { items_.clear(); }
        bool empty() const { return items_.empty(); }
        std::vector<output_action>::const_iterator begin() const { return items_.begin(); }
        std::vector<output_action>::const_iterator end() const { return items_.end(); }

    private:
        std::vector<output_action> items_;
    };

    class output_tracker
    {
    public:
        using sender = std::function<bool(output_action, bool)>;

        explicit output_tracker(sender send);

        bool update(const action_set& desired);

    private:
        sender send_;
        action_set held_;
        std::vector<output_action> to_release_;
    };

    class windows_output
    {
    public:
        bool send(output_action action, bool down);

        static bool move_pointer(int x, int y);

    private:
        static KEYBDINPUT find_key(output_action action);
        static bool send_mouse(output_action action, bool down);
        static bool send_mouse_input(DWORD flags, DWORD data, int x = 0, int y = 0);

        std::map<output_action, KEYBDINPUT> held_keys_;
    };
}
