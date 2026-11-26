#pragma once

#include <Windows.h>

#include <filesystem>
#include <string>

namespace menu::system::driver
{
    enum class stage
    {
        idle,
        extracting,
        installing,
        done,
        failed
    };

    struct status
    {
        stage current = stage::idle;
        std::string error;
    };

    bool installed();
    bool elevated();
    void install();
    status state();
    bool busy();
    void reset();
    void shutdown();
    std::filesystem::path folder();
    std::wstring library();
}
