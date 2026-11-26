#pragma once

#include "json.h"

#include <engine/types.h>

#include <filesystem>
#include <string>

namespace engine::profiles
{
    inline constexpr std::size_t maximum_bytes = 2 * 1024 * 1024;

    bool load(const std::filesystem::path& path, binding_map& bindings, std::string& error);
    bool save(const std::filesystem::path& path, const binding_map& bindings, std::string& error);
    bool read_json(const std::filesystem::path& path, json::value& root, std::string& error);
    std::string to_json(const binding_map& bindings);
}
