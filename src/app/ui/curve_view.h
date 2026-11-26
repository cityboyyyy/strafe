#pragma once

#include <engine/types.h>

#include <imgui.h>

#include <array>
#include <filesystem>

namespace menu::ui
{
    struct aim_profile
    {
        float sensitivity_x = 0.5f;
        float sensitivity_y = 0.5f;
        float anti_deadzone = 0.25f;
        float smoothing = 0.12f;
        std::array<float, 6> points{ 0.0f, 0.2f, 0.4f, 0.6f, 0.8f, 1.0f };
    };

    engine::aim_settings to_engine(const aim_profile& profile);
    bool load_aim(const std::filesystem::path& path, aim_profile& profile);
    bool save_aim(const std::filesystem::path& path, const aim_profile& profile);

    class curve_view
    {
    public:
        bool draw(ImVec2 min, ImVec2 max, aim_profile& profile, const ImVec4& accent);

    private:
        bool pill(const char* id, ImVec2 min, ImVec2 max, const char* label, bool active, const ImVec4& accent);

        int dragging_ = -1;

        double opened_at_ = -1.0;
    };
}
