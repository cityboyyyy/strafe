#include "bindings.h"

#include "../output/output.h"

#include <algorithm>
#include <array>
#include <utility>

namespace engine
{
    const binding* binding_map::find(const input_binding& source) const
    {
        for (const auto& item : entries_)
        {
            if (item.source == source)
                return &item.value;
        }
        return nullptr;
    }

    bool binding_map::contains(const input_binding& source) const
    {
        return find(source) != nullptr;
    }

    void binding_map::set(const input_binding& source, binding value)
    {
        for (auto& item : entries_)
        {
            if (item.source == source)
            {
                item.value = std::move(value);
                return;
            }
        }
        entries_.push_back({ source, std::move(value) });
    }

    bool binding_map::erase(const input_binding& source)
    {
        return std::erase_if(entries_, [&source](const entry& item) { return item.source == source; }) != 0;
    }

    void binding_map::clear()
    {
        entries_.clear();
    }
}

namespace engine::bindings
{
    namespace
    {
        constexpr std::array<controller_input, 35> inputs =
        { {
            { control_button::a, "A" }, { control_button::b, "B" }, { control_button::x, "X" }, { control_button::y, "Y" },
            { control_button::lb, "Left bumper" }, { control_button::rb, "Right bumper" },
            { control_button::lt, "Left trigger" }, { control_button::rt, "Right trigger" },
            { control_button::dpad_up, "D-pad up" }, { control_button::dpad_down, "D-pad down" },
            { control_button::dpad_left, "D-pad left" }, { control_button::dpad_right, "D-pad right" },
            { control_button::left_stick_up, "Left stick up" }, { control_button::left_stick_down, "Left stick down" },
            { control_button::left_stick_left, "Left stick left" }, { control_button::left_stick_right, "Left stick right" },
            { control_button::left_stick_press, "Left stick click" },
            { control_button::right_stick_up, "Right stick up" }, { control_button::right_stick_down, "Right stick down" },
            { control_button::right_stick_left, "Right stick left" }, { control_button::right_stick_right, "Right stick right" },
            { control_button::right_stick_press, "Right stick click" },
            { control_button::start, "Menu / Start" }, { control_button::back, "View / Back" },
            { control_button::guide, "Guide / PS" },
            { control_button::touchpad, "Touchpad click" }, { control_button::mute, "Microphone mute" },
            { control_button::paddle_1, "Paddle P1" }, { control_button::paddle_2, "Paddle P2" },
            { control_button::paddle_3, "Paddle P3" }, { control_button::paddle_4, "Paddle P4" },
            { control_button::left_paddle, "Left back button" }, { control_button::right_paddle, "Right back button" },
            { control_button::left_function, "Left Fn" }, { control_button::right_function, "Right Fn" }
        } };
    }

    std::span<const controller_input> controller_inputs()
    {
        return inputs;
    }

    std::string controller_name(control_button button)
    {
        for (const auto& item : inputs)
        {
            if (item.button == button)
                return std::string(item.name);
        }
        return std::to_string(static_cast<std::uint64_t>(button));
    }

    input_binding controller(control_button button)
    {
        return { input_kind::controller, static_cast<std::uint64_t>(button) };
    }

    std::string input_name(const input_binding& source)
    {
        if (source.kind != input_kind::controller)
            return output::name(static_cast<output_action>(source.code));
        switch (static_cast<control_button>(source.code))
        {
        case control_button::a:
            return "A / Cross";
        case control_button::b:
            return "B / Circle";
        case control_button::x:
            return "X / Square";
        case control_button::y:
            return "Y / Triangle";
        default:
            return controller_name(static_cast<control_button>(source.code));
        }
    }

    bool is_mouse_input(output_action action)
    {
        return action >= output_action::mouse_left && action <= output_action::move_right;
    }

    bool is_wheel(const input_binding& source)
    {
        const auto action = static_cast<output_action>(source.code);
        return source.kind == input_kind::mouse && (action == output_action::wheel_up || action == output_action::wheel_down);
    }

    bool is_motion(const input_binding& source)
    {
        const auto action = static_cast<output_action>(source.code);
        return source.kind == input_kind::mouse && action >= output_action::move_up && action <= output_action::move_right;
    }

    binding_map defaults()
    {
        const std::pair<control_button, output_action> rows[] =
        {
            { control_button::a, output::key_action(output::keys::space) },
            { control_button::b, output::key_action(output::keys::escape) },
            { control_button::x, output::key_action(0x45) },
            { control_button::y, output::key_action(output::keys::tab) },
            { control_button::lb, output::key_action(output::keys::left_shift) },
            { control_button::rb, output::key_action(0x52) },
            { control_button::lt, output_action::mouse_right },
            { control_button::rt, output_action::mouse_left },
            { control_button::start, output::key_action(output::keys::enter) },
            { control_button::dpad_up, output::key_action(output::keys::up) },
            { control_button::dpad_down, output::key_action(output::keys::down) },
            { control_button::dpad_left, output::key_action(output::keys::left) },
            { control_button::dpad_right, output::key_action(output::keys::right) },
            { control_button::left_stick_up, output::key_action(0x57) },
            { control_button::left_stick_down, output::key_action(0x53) },
            { control_button::left_stick_left, output::key_action(0x41) },
            { control_button::left_stick_right, output::key_action(0x44) }
        };
        binding_map result;
        for (const auto& [button, action] : rows)
            result.set(controller(button), binding{ { action }, false });
        return result;
    }
}
