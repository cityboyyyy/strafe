#pragma once

#include "input_capture.h"

#include "../model/emulation.h"

#include <imgui.h>

#include <array>
#include <string>

namespace menu::ui
{
    struct editor_result
    {
        bool changed = false;
        bool close = false;
    };

    struct power_state
    {
        std::string error;
    };

    class binding_editor
    {
    public:
        editor_result draw(ImVec2 min, ImVec2 max, model::button which, model::family family, model::emulation_set& set,
            std::array<float, 4>* tuning, const power_state* power, const ImVec4& accent, float appear);
        void stop();
        bool capturing() const { return capture_.active(); }
        int capture_target() const { return capture_.target(); }

    private:
        bool slot(const char* id, ImVec2 min, ImVec2 max, int index, const std::string& value, const char* caption, int arrow, bool large);
        bool keycap(const char* id, ImVec2 min, ImVec2 max, const std::string& value);
        bool pill(const char* id, ImVec2 min, ImVec2 max, const char* label, bool active);
        bool mirror_cell(const char* id, ImVec2 min, ImVec2 max, model::button option, model::family family, bool active);
        void assign(model::emulation_set& set, int index, std::uint32_t code, editor_result& result);

        input_capture capture_;
        model::button which_ = model::button::count;
        std::array<double, 5> assigned_at_{ -10.0, -10.0, -10.0, -10.0, -10.0 };
        bool slot_hovered_ = false;
        bool control_hovered_ = false;
        float appear_ = 1.0f;
        ImVec4 accent_{};
    };
}
