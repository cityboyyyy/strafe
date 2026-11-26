#pragma once

#include "model.h"

#include <cstdint>
#include <span>
#include <string>

namespace menu::model
{
    bool load(std::span<const std::uint8_t> bytes, std::uint32_t max_texture_size, model& out, std::string& error);
}
