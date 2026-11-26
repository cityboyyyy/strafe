#pragma once

#include <engine/types.h>

#include <span>
#include <string>
#include <string_view>

namespace engine::bindings
{
    using controller_input = controller_input_info;

    std::span<const controller_input> controller_inputs();
    std::string controller_name(control_button button);
    input_binding controller(control_button button);
    std::string input_name(const input_binding& source);
    bool is_mouse_input(output_action action);
    bool is_wheel(const input_binding& source);
    bool is_motion(const input_binding& source);
    binding_map defaults();
}
