#pragma once

#include <cstdint>
#include <filesystem>
#include <span>

namespace menu::model
{
    std::span<const std::uint8_t> asset_bytes(const std::filesystem::path& relative);
}
