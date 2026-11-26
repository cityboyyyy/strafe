#include "emulation.h"

#include <Windows.h>

#include <cstdio>
#include <fstream>
#include <sstream>

namespace menu::model
{
    namespace
    {
        constexpr std::size_t index_of(button which)
        {
            return static_cast<std::size_t>(which);
        }

        struct named_key
        {
            std::uint32_t code;
            const char* name;
        };

        constexpr named_key named_keys[] = {
            { VK_LBUTTON, "LMB" },
            { VK_RBUTTON, "RMB" },
            { VK_MBUTTON, "MMB" },
            { VK_XBUTTON1, "XB1" },
            { VK_XBUTTON2, "XB2" },
            { wheel_up_code, "MMB-Up" },
            { wheel_down_code, "MMB-Down" },
            { VK_LSHIFT, "Shift" },
            { VK_RSHIFT, "Right Shift" },
            { VK_SHIFT, "Shift" },
            { VK_LCONTROL, "Ctrl" },
            { VK_RCONTROL, "Right Ctrl" },
            { VK_CONTROL, "Ctrl" },
            { VK_LMENU, "Alt" },
            { VK_RMENU, "Right Alt" },
            { VK_MENU, "Alt" },
            { VK_LWIN, "Win" },
            { VK_RWIN, "Right Win" },
            { VK_APPS, "Menu" },
            { VK_SPACE, "Space" },
            { VK_TAB, "Tab" },
            { VK_ESCAPE, "Esc" },
            { VK_RETURN, "Enter" },
            { VK_BACK, "Backspace" },
            { VK_CAPITAL, "Caps Lock" },
            { VK_PRIOR, "Page Up" },
            { VK_NEXT, "Page Down" },
            { VK_HOME, "Home" },
            { VK_END, "End" },
            { VK_INSERT, "Insert" },
            { VK_DELETE, "Delete" },
            { VK_UP, "Up" },
            { VK_DOWN, "Down" },
            { VK_LEFT, "Left" },
            { VK_RIGHT, "Right" },
            { VK_SNAPSHOT, "Print Screen" },
            { VK_SCROLL, "Scroll Lock" },
            { VK_PAUSE, "Pause" },
            { VK_NUMLOCK, "Num Lock" },
            { VK_MULTIPLY, "Num *" },
            { VK_ADD, "Num +" },
            { VK_SUBTRACT, "Num -" },
            { VK_DECIMAL, "Num ." },
            { VK_DIVIDE, "Num /" },
        };

        std::string narrow(const wchar_t* text)
        {
            const int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
            if (length <= 1)
                return {};
            std::string result(static_cast<std::size_t>(length - 1), '\0');
            WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), length, nullptr, nullptr);
            return result;
        }

        std::uint32_t* raw_slot(emulation_set& set, button which, int index)
        {
            if (which == button::dpad)
                return index >= 0 && index < 4 ? &set[index_of(button::dpad_up) + static_cast<std::size_t>(index)].press : nullptr;
            emulated_input& entry = set[index_of(which)];
            if (index == press_slot)
                return has_press(which) ? &entry.press : nullptr;
            return has_directions(which) && index >= 0 && index < 4 ? &entry.move[static_cast<std::size_t>(index)] : nullptr;
        }

        bool matches(const emulation_set& set, button which, std::uint32_t up, std::uint32_t down, std::uint32_t left, std::uint32_t right)
        {
            return slot(set, which, 0) == up && slot(set, which, 1) == down && slot(set, which, 2) == left && slot(set, which, 3) == right;
        }
    }

    emulation_set default_emulation()
    {
        emulation_set set{};
        const auto press = [&](button which, std::uint32_t code) { set[index_of(which)].press = code; };
        press(button::south, VK_SPACE);
        press(button::east, 'Q');
        press(button::west, 'E');
        press(button::north, 'F');
        press(button::left_bumper, wheel_up_code);
        press(button::right_bumper, wheel_down_code);
        press(button::left_trigger, VK_RBUTTON);
        press(button::right_trigger, VK_LBUTTON);
        press(button::dpad_up, 'B');
        press(button::dpad_down, 'T');
        press(button::left_stick, VK_LSHIFT);
        set[index_of(button::left_stick)].move = { 'W', 'S', 'A', 'D' };
        press(button::right_stick, VK_LCONTROL);
        set[index_of(button::right_stick)].mouse_motion = true;
        press(button::view, 'M');
        press(button::menu, VK_ESCAPE);
        press(button::paddle_1, 'R');
        set[index_of(button::paddle_1)].mirror = button::west;
        set[index_of(button::touchpad)].mirror = button::view;
        return set;
    }

    bool has_directions(button which)
    {
        return which == button::left_stick || which == button::right_stick || which == button::dpad;
    }

    bool has_press(button which)
    {
        return which != button::dpad && which != button::count;
    }

    bool is_paddle(button which)
    {
        return which >= button::paddle_1 && which <= button::paddle_4;
    }

    bool needs_mirror(button which)
    {
        return is_paddle(which) || which == button::touchpad || which == button::mute;
    }

    std::uint32_t slot(const emulation_set& set, button which, int index)
    {
        const std::uint32_t* value = raw_slot(const_cast<emulation_set&>(set), which, index);
        return value ? *value : 0;
    }

    void assign(emulation_set& set, button which, int index, std::uint32_t code)
    {
        std::uint32_t* target = raw_slot(set, which, index);
        if (!target)
            return;
        if (code != 0)
        {
            for (auto& entry : set)
            {
                if (entry.press == code)
                    entry.press = 0;
                for (auto& direction : entry.move)
                {
                    if (direction == code)
                        direction = 0;
                }
            }
        }
        *target = code;
        if (index != press_slot && code != 0 && which != button::dpad)
            set[index_of(which)].mouse_motion = false;
    }

    void set_mouse_motion(emulation_set& set, button which, bool enabled)
    {
        if (which != button::left_stick && which != button::right_stick)
            return;
        if (enabled)
        {
            set[index_of(button::left_stick)].mouse_motion = false;
            set[index_of(button::right_stick)].mouse_motion = false;
            set[index_of(which)].move = {};
        }
        set[index_of(which)].mouse_motion = enabled;
    }

    void clear(emulation_set& set, button which)
    {
        for (int index = 0; index <= press_slot; ++index)
            assign(set, which, index, 0);
        if (which != button::dpad)
            set[index_of(which)].mouse_motion = false;
    }

    bool is_bound(const emulation_set& set, button which)
    {
        if (which == button::count)
            return false;
        if (which != button::dpad && set[index_of(which)].mouse_motion)
            return true;
        for (int index = 0; index <= press_slot; ++index)
        {
            if (slot(set, which, index) != 0)
                return true;
        }
        return false;
    }

    std::string input_name(std::uint32_t code)
    {
        if (code == 0)
            return {};
        for (const auto& key : named_keys)
        {
            if (key.code == code)
                return key.name;
        }
        if (code >= VK_F1 && code <= VK_F24)
            return "F" + std::to_string(code - VK_F1 + 1);
        if (code >= VK_NUMPAD0 && code <= VK_NUMPAD9)
            return "Num " + std::to_string(code - VK_NUMPAD0);
        if ((code >= '0' && code <= '9') || (code >= 'A' && code <= 'Z'))
            return std::string(1, static_cast<char>(code));

        const UINT scan = MapVirtualKeyW(code, MAPVK_VK_TO_VSC_EX);
        if (scan != 0)
        {
            const LONG param = static_cast<LONG>(((scan & 0xFF) << 16) | ((scan & 0xFF00) == 0xE000 ? (1 << 24) : 0));
            wchar_t buffer[64]{};
            if (GetKeyNameTextW(param, buffer, static_cast<int>(std::size(buffer))) > 0)
            {
                std::string name = narrow(buffer);
                if (!name.empty())
                {
                    if (name.size() == 1 && name[0] >= 'a' && name[0] <= 'z')
                        name[0] = static_cast<char>(name[0] - 'a' + 'A');
                    return name;
                }
            }
        }

        char fallback[16];
        std::snprintf(fallback, sizeof(fallback), "Key %02X", code);
        return fallback;
    }

    std::string describe(const emulation_set& set, button which)
    {
        if (which == button::count)
            return {};

        std::string result;
        if (has_directions(which))
        {
            if (which != button::dpad && set[index_of(which)].mouse_motion)
                result = "Mouse";
            else if (matches(set, which, 'W', 'S', 'A', 'D'))
                result = "WASD";
            else if (matches(set, which, VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT))
                result = "Arrows";
            else if (matches(set, which, 'I', 'K', 'J', 'L'))
                result = "IJKL";
            else
            {
                for (int index = 0; index < 4; ++index)
                {
                    const std::string name = input_name(slot(set, which, index));
                    if (name.empty())
                        continue;
                    if (!result.empty())
                        result += "  ";
                    result += name;
                }
            }
        }

        if (has_press(which))
        {
            const std::string press = input_name(set[index_of(which)].press);
            if (!press.empty())
            {
                if (!result.empty())
                    result += "  \xC2\xB7  ";
                result += press;
            }
        }
        return result;
    }

    bool is_held(std::uint32_t code)
    {
        if (code == 0 || code > 0xFF)
            return false;
        return (GetAsyncKeyState(static_cast<int>(code)) & 0x8000) != 0;
    }

    std::filesystem::path emulation_path(const wchar_t* model_file)
    {
        wchar_t buffer[MAX_PATH]{};
        const DWORD length = GetEnvironmentVariableW(L"APPDATA", buffer, MAX_PATH);
        std::filesystem::path root = length > 0 && length < MAX_PATH ? std::filesystem::path(buffer) : std::filesystem::temp_directory_path();
        static const bool migrated = [&root]
        {
            std::error_code error;
            const auto old_root = root / L"Controller";
            const auto new_root = root / L"Strafe";
            if (!std::filesystem::exists(new_root, error) && std::filesystem::exists(old_root, error))
                std::filesystem::rename(old_root, new_root, error);
            return true;
        }();
        (void)migrated;
        return root / L"Strafe" / L"bindings" / (std::filesystem::path(model_file).stem().wstring() + L".cfg");
    }

    bool save_emulation(const std::filesystem::path& path, const emulation_set& set)
    {
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        std::ofstream file(path, std::ios::trunc);
        if (!file)
            return false;
        file << "controller-bindings 2\n";
        for (std::size_t i = 0; i < set.size(); ++i)
        {
            const auto& entry = set[i];
            file << i << ' ' << std::hex << entry.press << ' ' << entry.move[0] << ' ' << entry.move[1] << ' ' << entry.move[2] << ' ' << entry.move[3]
                 << std::dec << ' ' << (entry.mouse_motion ? 1 : 0) << ' ' << static_cast<int>(entry.mirror) << '\n';
        }
        return static_cast<bool>(file);
    }

    bool load_emulation(const std::filesystem::path& path, emulation_set& set)
    {
        std::ifstream file(path);
        if (!file)
            return false;
        std::string header;
        std::getline(file, header);
        const bool legacy = header == "controller-bindings 1";
        if (!legacy && header != "controller-bindings 2")
            return false;

        emulation_set loaded = default_emulation();
        std::string line;
        while (std::getline(file, line))
        {
            std::istringstream row(line);
            std::size_t index = 0;
            emulated_input entry;
            int motion = 0;
            int mirror = 0;
            row >> index >> std::hex >> entry.press >> entry.move[0] >> entry.move[1] >> entry.move[2] >> entry.move[3] >> std::dec >> motion >> mirror;
            if (!row || index >= loaded.size() || mirror < 0 || mirror > static_cast<int>(button::count))
                continue;
            if (legacy && index != index_of(button::guide))
                continue;
            entry.mouse_motion = motion != 0;
            entry.mirror = static_cast<button>(mirror);
            loaded[index] = entry;
        }
        set = loaded;
        return true;
    }
}
