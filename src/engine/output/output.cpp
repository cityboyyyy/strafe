#include "output.h"
#include "keys.h"

#include "../bindings/bindings.h"
#include "../input/controller.h"

#include <algorithm>
#include <charconv>
#include <utility>

namespace engine::output
{
    namespace
    {
        struct mouse_row
        {
            output_action action;
            std::string_view id;
            std::string_view display;
        };

        constexpr mouse_row mouse_rows[] =
        {
            { output_action::mouse_left, "MouseLeft", "Left click" },
            { output_action::mouse_right, "MouseRight", "Right click" },
            { output_action::mouse_middle, "MouseMiddle", "Middle click" },
            { output_action::mouse_back, "MouseBack", "Mouse Back" },
            { output_action::mouse_forward, "MouseForward", "Mouse Forward" },
            { output_action::wheel_up, "WheelUp", "Wheel up" },
            { output_action::wheel_down, "WheelDown", "Wheel down" },
            { output_action::move_up, "MoveUp", "Move pointer up" },
            { output_action::move_down, "MoveDown", "Move pointer down" },
            { output_action::move_left, "MoveLeft", "Move pointer left" },
            { output_action::move_right, "MoveRight", "Move pointer right" }
        };

        const mouse_row* mouse_row_for(output_action action)
        {
            for (const auto& row : mouse_rows)
            {
                if (row.action == action)
                    return &row;
            }
            return nullptr;
        }

        std::string_view trim(std::string_view text)
        {
            while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
                text.remove_prefix(1);
            while (!text.empty() && (text.back() == ' ' || text.back() == '\t'))
                text.remove_suffix(1);
            return text;
        }

        bool parse_number(std::string_view text, std::uint32_t& value)
        {
            if (!text.empty() && text.front() == '+')
                text.remove_prefix(1);
            if (text.empty() || text.front() < '0' || text.front() > '9')
                return false;
            const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
            return result.ec == std::errc{} && result.ptr == text.data() + text.size();
        }

        constexpr DWORD key_up = KEYEVENTF_KEYUP;
        constexpr DWORD scan_code = KEYEVENTF_SCANCODE;
        constexpr DWORD extended_key = KEYEVENTF_EXTENDEDKEY;
        constexpr UINT map_to_extended_scan_code = MAPVK_VK_TO_VSC_EX;
    }

    std::string name(output_action action)
    {
        if (const mouse_row* row = mouse_row_for(action))
            return std::string(row->display);
        if (is_xbox(action))
            return "Xbox " + bindings::controller_name(xbox_button(action));
        const std::uint32_t key = code(action);
        switch (key)
        {
        case keys::left_shift:
            return "Left Shift";
        case keys::right_shift:
            return "Right Shift";
        case keys::left_control:
            return "Left Ctrl";
        case keys::right_control:
            return "Right Ctrl";
        case keys::left_menu:
            return "Left Alt";
        case keys::right_menu:
            return "Right Alt";
        case keys::enter:
            return "Enter";
        case keys::space:
            return "Space";
        default:
            break;
        }
        if (key >= keys::d0 && key <= keys::d9)
            return std::to_string(key - keys::d0);
        if (const auto display = key_display_name(key))
            return std::string(*display);
        return std::to_string(key);
    }

    bool is_key(std::uint32_t key)
    {
        return key >= 8 && key <= 0xFE && key != keys::packet && key_defined(key);
    }

    bool is_xbox(output_action action)
    {
        return (code(action) & code(output_action::xbox)) != 0;
    }

    control_button xbox_button(output_action action)
    {
        return static_cast<control_button>(code(action) & ~code(output_action::xbox));
    }

    bool is_xbox_button(control_button button)
    {
        return button > control_button::none && button <= control_button::right_stick_right &&
            input::button_id(button).has_value();
    }

    output_action for_xbox(control_button button)
    {
        return static_cast<output_action>(code(output_action::xbox) | static_cast<std::uint32_t>(button));
    }

    std::string serialize(output_action action)
    {
        if (is_xbox(action))
        {
            const auto id = input::button_id(xbox_button(action));
            return "Xbox:" + (id ? std::string(*id) : std::to_string(static_cast<std::uint64_t>(xbox_button(action))));
        }
        if (action >= output_action::mouse_left)
        {
            if (const mouse_row* row = mouse_row_for(action))
                return std::string(row->id);
            return std::to_string(code(action));
        }
        const auto id = key_name(code(action));
        return "Key:" + (id ? std::string(*id) : std::to_string(code(action)));
    }

    bool try_parse(std::string_view value, output_action& action)
    {
        if (value.starts_with("Key:"))
        {
            const std::string_view rest = trim(value.substr(4));
            std::uint32_t key = 0;
            bool found = parse_number(rest, key);
            if (!found)
            {
                if (const auto named = key_from_name(rest))
                {
                    key = *named;
                    found = true;
                }
            }
            if (found && is_key(key))
            {
                action = key_action(normalize_modifier(key));
                return true;
            }
        }

        if (value.starts_with("Xbox:"))
        {
            const std::string_view rest = trim(value.substr(5));
            std::uint32_t number = 0;
            std::optional<control_button> button;
            if (parse_number(rest, number))
                button = static_cast<control_button>(number);
            else
                button = input::button_from_id(rest);
            if (button && is_xbox_button(*button))
            {
                action = for_xbox(*button);
                return true;
            }
        }

        for (const auto& row : mouse_rows)
        {
            if (row.id == value)
            {
                action = row.action;
                return true;
            }
        }

        action = output_action::none;
        return false;
    }

    std::uint32_t normalize_modifier(std::uint32_t key)
    {
        switch (key)
        {
        case keys::shift:
            return keys::left_shift;
        case keys::control:
            return keys::left_control;
        case keys::menu:
            return keys::left_menu;
        default:
            return key;
        }
    }

    std::uint32_t capture_key(std::uint32_t key, std::int64_t message_flags)
    {
        const bool extended = (message_flags & (1ll << 24)) != 0;
        switch (key)
        {
        case keys::shift:
            return ((message_flags >> 16) & 0xFF) == 0x36 ? keys::right_shift : keys::left_shift;
        case keys::control:
            return extended ? keys::right_control : keys::left_control;
        case keys::menu:
            return extended ? keys::right_menu : keys::left_menu;
        default:
            return key;
        }
    }

    bool action_set::insert(output_action action)
    {
        if (contains(action))
            return false;
        items_.push_back(action);
        return true;
    }

    bool action_set::erase(output_action action)
    {
        const auto found = std::find(items_.begin(), items_.end(), action);
        if (found == items_.end())
            return false;
        items_.erase(found);
        return true;
    }

    bool action_set::contains(output_action action) const
    {
        return std::find(items_.begin(), items_.end(), action) != items_.end();
    }

    output_tracker::output_tracker(sender send)
        : send_(std::move(send))
    {
    }

    bool output_tracker::update(const action_set& desired)
    {
        to_release_.clear();
        for (output_action action : held_)
        {
            if (!desired.contains(action))
                to_release_.push_back(action);
        }

        bool released_all = true;
        for (output_action action : to_release_)
        {
            if (send_(action, false))
                held_.erase(action);
            else
                released_all = false;
        }

        if (!released_all)
            return false;

        for (output_action action : desired)
        {
            if (held_.contains(action))
                continue;
            if (!send_(action, true))
                return false;
            held_.insert(action);
        }
        return true;
    }

    bool windows_output::send(output_action action, bool down)
    {
        if (code(action) >= 0x100)
            return send_mouse(action, down);

        KEYBDINPUT key{};
        const auto held = held_keys_.find(action);
        if (held != held_keys_.end())
            key = held->second;
        else
            key = find_key(action);

        INPUT data{};
        data.type = INPUT_KEYBOARD;
        data.ki = key;
        if (!down)
            data.ki.dwFlags |= key_up;

        if (SendInput(1, &data, sizeof(INPUT)) != 1)
            return false;

        if (down)
            held_keys_[action] = key;
        else
            held_keys_.erase(action);
        return true;
    }

    KEYBDINPUT windows_output::find_key(output_action action)
    {
        const UINT key = code(action);
        const DWORD foreground_thread = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
        const UINT scan = MapVirtualKeyExW(key, map_to_extended_scan_code, GetKeyboardLayout(foreground_thread));
        KEYBDINPUT result{};
        if (scan == 0 || (scan >> 8) == 0xE1 || key == keys::print_screen)
        {
            result.wVk = static_cast<WORD>(key);
            return result;
        }

        result.wScan = static_cast<WORD>(scan & 0xFF);
        result.dwFlags = scan_code | ((scan >> 8) == 0xE0 ? extended_key : 0);
        return result;
    }

    bool windows_output::send_mouse(output_action action, bool down)
    {
        if (action == output_action::wheel_up || action == output_action::wheel_down)
            return !down || send_mouse_input(MOUSEEVENTF_WHEEL,
                action == output_action::wheel_up ? static_cast<DWORD>(WHEEL_DELTA) : static_cast<DWORD>(-WHEEL_DELTA));
        if (action >= output_action::move_up)
            return true;

        DWORD flags = 0;
        switch (action)
        {
        case output_action::mouse_left:
            flags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
            break;
        case output_action::mouse_right:
            flags = down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
            break;
        case output_action::mouse_middle:
            flags = down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
            break;
        default:
            flags = down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
            break;
        }
        const DWORD data = action == output_action::mouse_back ? XBUTTON1 :
            action == output_action::mouse_forward ? XBUTTON2 : 0;
        return send_mouse_input(flags, data);
    }

    bool windows_output::move_pointer(int x, int y)
    {
        return (x == 0 && y == 0) || send_mouse_input(MOUSEEVENTF_MOVE, 0, x, y);
    }

    bool windows_output::send_mouse_input(DWORD flags, DWORD data, int x, int y)
    {
        INPUT input{};
        input.type = INPUT_MOUSE;
        input.mi.dx = x;
        input.mi.dy = y;
        input.mi.mouseData = data;
        input.mi.dwFlags = flags;
        return SendInput(1, &input, sizeof(INPUT)) == 1;
    }
}
