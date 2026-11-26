#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace engine
{
    enum class control_button : std::uint64_t
    {
        none = 0,
        dpad_up = 1u << 0,
        dpad_down = 1u << 1,
        dpad_left = 1u << 2,
        dpad_right = 1u << 3,
        start = 1u << 4,
        back = 1u << 5,
        left_stick_press = 1u << 6,
        right_stick_press = 1u << 7,
        lb = 1u << 8,
        rb = 1u << 9,
        guide = 1u << 10,
        a = 1u << 12,
        b = 1u << 13,
        x = 1u << 14,
        y = 1u << 15,
        lt = 1u << 16,
        rt = 1u << 17,
        left_stick_up = 1u << 18,
        left_stick_down = 1u << 19,
        left_stick_left = 1u << 20,
        left_stick_right = 1u << 21,
        right_stick_up = 1u << 22,
        right_stick_down = 1u << 23,
        right_stick_left = 1u << 24,
        right_stick_right = 1u << 25,
        touchpad = 1u << 26,
        mute = 1u << 27,
        paddle_1 = 1ull << 28,
        paddle_2 = 1ull << 29,
        paddle_3 = 1ull << 30,
        paddle_4 = 1ull << 31,
        left_paddle = 1ull << 32,
        right_paddle = 1ull << 33,
        left_function = 1ull << 34,
        right_function = 1ull << 35
    };

    constexpr control_button operator|(control_button a, control_button b)
    {
        return static_cast<control_button>(static_cast<std::uint64_t>(a) | static_cast<std::uint64_t>(b));
    }

    constexpr control_button operator&(control_button a, control_button b)
    {
        return static_cast<control_button>(static_cast<std::uint64_t>(a) & static_cast<std::uint64_t>(b));
    }

    constexpr control_button operator~(control_button a)
    {
        return static_cast<control_button>(~static_cast<std::uint64_t>(a));
    }

    constexpr control_button& operator|=(control_button& a, control_button b)
    {
        return a = a | b;
    }

    constexpr control_button& operator&=(control_button& a, control_button b)
    {
        return a = a & b;
    }

    constexpr bool any(control_button value)
    {
        return value != control_button::none;
    }

    struct controller_input_info
    {
        control_button button;
        std::string_view name;
    };

    enum class virtual_pad_kind : std::uint8_t
    {
        xbox_elite,
        dualsense
    };

    struct pad_options
    {
        virtual_pad_kind kind = virtual_pad_kind::xbox_elite;
        std::wstring duo_library;
        std::wstring target_process;
    };

    struct aim_settings
    {
        float sensitivity_x = 0.5f;
        float sensitivity_y = 0.5f;
        float anti_deadzone = 0.1f;
        float smoothing = 0.25f;
        float curve[17] = { 0.0f, 0.0625f, 0.125f, 0.1875f, 0.25f, 0.3125f, 0.375f, 0.4375f, 0.5f, 0.5625f, 0.625f, 0.6875f, 0.75f, 0.8125f, 0.875f, 0.9375f, 1.0f };
    };

    enum class pad_connection : std::uint8_t
    {
        none,
        connecting,
        connected,
        failed
    };

    enum class output_action : std::uint32_t
    {
        none = 0,
        mouse_left = 0x100,
        mouse_right,
        mouse_middle,
        mouse_back,
        mouse_forward,
        wheel_up,
        wheel_down,
        move_up,
        move_down,
        move_left,
        move_right,
        xbox = 0x10000000
    };

    enum class input_kind : std::uint8_t
    {
        controller,
        keyboard,
        mouse
    };

    struct input_binding
    {
        input_kind kind = input_kind::controller;
        std::uint64_t code = 0;

        friend auto operator<=>(const input_binding&, const input_binding&) = default;
    };

    struct binding
    {
        std::vector<output_action> targets;
        bool suppress = false;

        friend bool operator==(const binding&, const binding&) = default;
    };

    class binding_map
    {
    public:
        struct entry
        {
            input_binding source;
            binding value;
        };

        const binding* find(const input_binding& source) const;
        bool contains(const input_binding& source) const;
        void set(const input_binding& source, binding value);
        bool erase(const input_binding& source);
        void clear();

        std::size_t size() const { return entries_.size(); }
        bool empty() const { return entries_.empty(); }
        std::vector<entry>::const_iterator begin() const { return entries_.begin(); }
        std::vector<entry>::const_iterator end() const { return entries_.end(); }

    private:
        std::vector<entry> entries_;
    };
}
