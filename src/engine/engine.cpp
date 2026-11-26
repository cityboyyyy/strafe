#include <engine/engine.h>

#include "bindings/bindings.h"
#include "output/output.h"
#include "profiles/profile.h"
#include "profiles/rewasd_import.h"

#include <array>

namespace engine
{
    namespace
    {
        constexpr std::array<output_action, 11> mouse_actions =
        {
            output_action::mouse_left, output_action::mouse_right, output_action::mouse_middle,
            output_action::mouse_back, output_action::mouse_forward, output_action::wheel_up, output_action::wheel_down,
            output_action::move_up, output_action::move_down, output_action::move_left, output_action::move_right
        };
    }

    std::span<const controller_input_info> controller_inputs()
    {
        return bindings::controller_inputs();
    }

    std::span<const output_action> mouse_outputs()
    {
        return mouse_actions;
    }

    std::vector<output_action> xbox_outputs()
    {
        std::vector<output_action> result;
        for (const auto& item : bindings::controller_inputs())
        {
            if (output::is_xbox_button(item.button))
                result.push_back(output::for_xbox(item.button));
        }
        return result;
    }

    std::string controller_name(control_button button)
    {
        return bindings::controller_name(button);
    }

    std::string input_name(const input_binding& source)
    {
        return bindings::input_name(source);
    }

    std::string output_name(output_action action)
    {
        return output::name(action);
    }

    input_binding controller_input(control_button button)
    {
        return bindings::controller(button);
    }

    input_binding keyboard_input(std::uint32_t virtual_key)
    {
        return { input_kind::keyboard, output::normalize_modifier(virtual_key) };
    }

    input_binding mouse_input(output_action action)
    {
        return { input_kind::mouse, output::code(action) };
    }

    output_action key_output(std::uint32_t virtual_key)
    {
        return output::key_action(output::normalize_modifier(virtual_key));
    }

    output_action xbox_output(control_button button)
    {
        return output::is_xbox_button(button) ? output::for_xbox(button) : output_action::none;
    }

    bool is_key(std::uint32_t virtual_key)
    {
        return output::is_key(virtual_key);
    }

    bool is_xbox(output_action action)
    {
        return output::is_xbox(action);
    }

    bool is_xbox_button(control_button button)
    {
        return output::is_xbox_button(button);
    }

    control_button xbox_button(output_action action)
    {
        return output::xbox_button(action);
    }

    bool is_mouse_input(output_action action)
    {
        return bindings::is_mouse_input(action);
    }

    std::uint32_t capture_key(std::uint32_t virtual_key, std::int64_t message_flags)
    {
        return output::capture_key(virtual_key, message_flags);
    }

    std::string serialize_output(output_action action)
    {
        return output::serialize(action);
    }

    bool parse_output(std::string_view text, output_action& action)
    {
        return output::try_parse(text, action);
    }

    binding_map default_bindings()
    {
        return bindings::defaults();
    }

    bool load_profile(const std::filesystem::path& path, binding_map& bindings, std::string& error)
    {
        return profiles::load(path, bindings, error);
    }

    bool save_profile(const std::filesystem::path& path, const binding_map& bindings, std::string& error)
    {
        return profiles::save(path, bindings, error);
    }

    bool import_rewasd(const std::filesystem::path& path, import_result& result, std::string& error)
    {
        profiles::rewasd_result loaded;
        if (!profiles::load_rewasd(path, loaded, error))
            return false;
        result.bindings = std::move(loaded.bindings);
        result.skipped = loaded.skipped;
        return true;
    }
}
