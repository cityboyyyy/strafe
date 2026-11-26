#pragma once

#include "model.h"

#include <cstddef>
#include <cstdint>

namespace menu::model
{
    bool decode_image(const std::uint8_t* data, std::size_t size, std::uint32_t max_size, image& out);
}
