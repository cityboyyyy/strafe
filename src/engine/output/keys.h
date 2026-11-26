#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace engine::output
{
    bool key_defined(std::uint32_t code);
    std::optional<std::string_view> key_name(std::uint32_t code);
    std::optional<std::string_view> key_display_name(std::uint32_t code);
    std::optional<std::uint32_t> key_from_name(std::string_view name);
}
