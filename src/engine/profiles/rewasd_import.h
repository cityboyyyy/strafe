#pragma once

#include <engine/types.h>

#include <filesystem>
#include <string>

namespace engine::profiles
{
    struct rewasd_result
    {
        binding_map bindings;
        int skipped = 0;
    };

    bool load_rewasd(const std::filesystem::path& path, rewasd_result& result, std::string& error);
    bool try_rewasd_key(int dik, output_action& key);
}
