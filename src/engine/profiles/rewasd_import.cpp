#include "rewasd_import.h"
#include "profile.h"

#include "../bindings/bindings.h"
#include "../output/output.h"

#include <windows.h>

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <vector>

namespace engine::profiles
{
    namespace
    {
        const std::map<int, control_button>& xbox_buttons()
        {
            static const std::map<int, control_button> table =
            {
                { 1, control_button::a }, { 2, control_button::b }, { 3, control_button::x }, { 4, control_button::y },
                { 5, control_button::lb }, { 6, control_button::rb }, { 7, control_button::back }, { 8, control_button::start },
                { 9, control_button::left_stick_press }, { 10, control_button::right_stick_press }, { 11, control_button::guide },
                { 33, control_button::dpad_up }, { 34, control_button::dpad_down }, { 35, control_button::dpad_left },
                { 36, control_button::dpad_right }, { 40, control_button::left_stick_up }, { 41, control_button::left_stick_down },
                { 42, control_button::left_stick_left }, { 43, control_button::left_stick_right }, { 47, control_button::right_stick_up },
                { 48, control_button::right_stick_down }, { 49, control_button::right_stick_left }, { 50, control_button::right_stick_right },
                { 51, control_button::lt }, { 55, control_button::rt }
            };
            return table;
        }

        std::optional<control_button> xbox_button_for(int id)
        {
            const auto& table = xbox_buttons();
            const auto found = table.find(id);
            if (found == table.end())
                return std::nullopt;
            return found->second;
        }

        const json::value* object(const json::value* value, std::string_view key)
        {
            if (value == nullptr)
                return nullptr;
            const json::value* item = value->find(key);
            return item != nullptr && item->is_object() ? item : nullptr;
        }

        std::vector<const json::value*> objects(const json::value* value, std::string_view key)
        {
            std::vector<const json::value*> rows;
            if (value == nullptr)
                return rows;
            const json::value* item = value->find(key);
            if (item == nullptr || !item->is_array())
                return rows;
            for (const auto& row : item->items)
            {
                if (row.is_object())
                    rows.push_back(&row);
            }
            return rows;
        }

        int number(const json::value* value, std::string_view key)
        {
            if (value == nullptr)
                return 0;
            const json::value* item = value->find(key);
            return item != nullptr && item->is_int() ? item->integer_value : 0;
        }

        std::string string(const json::value* value, std::string_view key)
        {
            if (value == nullptr)
                return {};
            const json::value* item = value->find(key);
            return item != nullptr && item->is_string() ? item->text : std::string();
        }

        bool boolean(const json::value* value, std::string_view key)
        {
            if (value == nullptr)
                return false;
            const json::value* item = value->find(key);
            return item != nullptr && item->is_bool() && item->flag;
        }

        bool has(const json::value* value, std::string_view key)
        {
            return value != nullptr && value->contains(key);
        }

        bool try_mouse_button(int id, output_action& button)
        {
            if (id >= 1 && id <= 5)
                button = output::mouse_buttons[static_cast<std::size_t>(id - 1)];
            else if (id == 171)
                button = output_action::wheel_up;
            else if (id == 172)
                button = output_action::wheel_down;
            else
                button = output_action::none;
            return button != output_action::none;
        }

        bool try_mouse_input(int id, output_action& action)
        {
            switch (id)
            {
            case 64:
                action = output_action::move_up;
                return true;
            case 65:
                action = output_action::move_down;
                return true;
            case 66:
                action = output_action::move_left;
                return true;
            case 67:
                action = output_action::move_right;
                return true;
            default:
                return try_mouse_button(id, action);
            }
        }

        bool try_input(const json::value* source, const std::map<int, std::string>& devices, input_binding& result)
        {
            result = {};
            const auto found = devices.find(number(source, "deviceId"));
            if (found == devices.end())
                return false;
            const std::string& device = found->second;
            const int button_id = number(source, "buttonId");
            output_action action = output_action::none;
            if (device == "gamepad")
            {
                if (const auto button = xbox_button_for(button_id))
                {
                    result = bindings::controller(*button);
                    return true;
                }
                return false;
            }
            if (device == "keyboard" && try_rewasd_key(button_id, action))
            {
                result = { input_kind::keyboard, output::code(action) };
                return true;
            }
            if (device == "mouse" && try_mouse_input(button_id, action))
            {
                result = { input_kind::mouse, output::code(action) };
                return true;
            }
            return false;
        }

        bool try_output(const json::value* atom, const std::set<int>& virtual_pads, output_action& result)
        {
            result = output_action::none;
            if (atom->members.size() != 1)
                return false;
            const json::value* keyboard = object(atom, "keyboard");
            if (keyboard != nullptr && !has(keyboard, "action") && try_rewasd_key(number(keyboard, "buttonId"), result))
                return true;
            const json::value* mouse = object(atom, "mouse");
            if (mouse != nullptr && !has(mouse, "action"))
            {
                if (try_mouse_button(number(mouse, "buttonId"), result))
                    return true;
                const std::string wheel = string(mouse, "wheel");
                const std::string direction = string(mouse, "direction");
                if (wheel == "up")
                    result = output_action::wheel_up;
                else if (wheel == "down")
                    result = output_action::wheel_down;
                else if (direction == "up")
                    result = output_action::move_up;
                else if (direction == "down")
                    result = output_action::move_down;
                else if (direction == "left")
                    result = output_action::move_left;
                else if (direction == "right")
                    result = output_action::move_right;
                else
                    result = output_action::none;
                return result != output_action::none;
            }
            const json::value* pad = object(atom, "gamepad");
            if (pad != nullptr && virtual_pads.contains(number(pad, "id")))
            {
                const auto button = xbox_button_for(number(pad, "buttonId"));
                const int value = number(pad, "value");
                if (button && !has(pad, "action") && (!has(pad, "value") || value == 32767 || value == 255))
                {
                    result = output::for_xbox(*button);
                    return true;
                }
            }
            return false;
        }
    }

    bool try_rewasd_key(int dik, output_action& key)
    {
        key = output_action::none;
        if (dik <= 0 || dik > 255 || dik == 0x9C)
            return false;
        const UINT scan = dik >= 0x80 ? static_cast<UINT>((dik & 0x7F) | 0xE000) : static_cast<UINT>(dik);
        const std::uint32_t virtual_key = dik == 0xC5 ? output::keys::pause : MapVirtualKeyW(scan, MAPVK_VSC_TO_VK_EX);
        if (!output::is_key(virtual_key))
            return false;
        key = output::key_action(output::normalize_modifier(virtual_key));
        return true;
    }

    bool load_rewasd(const std::filesystem::path& path, rewasd_result& result, std::string& error)
    {
        json::value document;
        if (!read_json(path, document, error))
            return false;
        const json::value* root = &document;
        const json::value* version = root->find("schemaVersion");
        if (version == nullptr || !version->is_number() || version->number != 3)
        {
            error = "This importer supports reWASD schema version 3.";
            return false;
        }

        const json::value* devices = object(root, "devices");
        std::map<int, std::string> hardware;
        for (const json::value* device : objects(devices, "hardware"))
            hardware.try_emplace(number(device, "id"), string(device, "name"));

        std::set<int> virtual_pads;
        for (const json::value* device : objects(object(devices, "virtual"), "gamepads"))
        {
            const std::string kind = string(device, "type");
            const int id = number(device, "id");
            if ((kind == "360" || kind == "xboxOne" || kind == "xboxone") && id > 0)
            {
                virtual_pads.insert(id);
                break;
            }
        }

        int default_shift = 1;
        for (const json::value* shift : objects(root, "shifts"))
        {
            if (string(shift, "type") == "default")
            {
                default_shift = number(shift, "id");
                break;
            }
        }

        std::map<int, input_binding> masks;
        for (const json::value* mask : objects(root, "masks"))
        {
            const auto set = objects(mask, "set");
            const int id = number(mask, "id");
            input_binding source;
            if (id > 0 && set.size() == 1 && try_input(set[0], hardware, source))
                masks[id] = source;
        }

        binding_map imported;
        std::vector<input_binding> suppress;
        int skipped = 0;
        for (const json::value* mapping : objects(root, "mappings"))
        {
            const json::value* condition = object(mapping, "condition");
            const int shift = number(condition, "shiftId");
            const bool base_layer = shift == 0 || shift == default_shift;
            if (const json::value* native = object(mapping, "hardware"))
            {
                input_binding muted;
                if (base_layer && boolean(native, "unmap") && try_input(object(condition, "hardware"), hardware, muted) &&
                    muted.kind != input_kind::controller)
                {
                    if (std::find(suppress.begin(), suppress.end(), muted) == suppress.end())
                        suppress.push_back(muted);
                }
                else
                {
                    ++skipped;
                }
                continue;
            }

            const json::value* mask = object(condition, "mask");
            const json::value* activator = object(mask, "activator");
            const auto found = masks.find(number(mask, "id"));
            if (!base_layer || has(mapping, "jumpToLayer") || string(activator, "type") != "single" ||
                string(activator, "mode") != "hold_until_release" || boolean(object(activator, "params"), "expert") ||
                found == masks.end())
            {
                ++skipped;
                continue;
            }
            const input_binding source = found->second;

            const auto atoms = objects(mapping, "macros");
            std::vector<output_action> targets;
            bool supported = atoms.size() >= 1 && atoms.size() <= 8;
            for (const json::value* atom : atoms)
            {
                output_action target = output_action::none;
                if (!try_output(atom, virtual_pads, target))
                {
                    supported = false;
                    break;
                }
                if (std::find(targets.begin(), targets.end(), target) == targets.end())
                    targets.push_back(target);
            }

            const binding* previous = imported.find(source);
            if (!supported || (previous != nullptr && previous->targets != targets))
            {
                ++skipped;
                continue;
            }
            imported.set(source, binding{ std::move(targets), false });
        }

        for (const input_binding& source : suppress)
        {
            if (const binding* existing = imported.find(source))
                imported.set(source, binding{ existing->targets, true });
            else
                ++skipped;
        }
        result.bindings = std::move(imported);
        result.skipped = skipped;
        return true;
    }
}
