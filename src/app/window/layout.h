#pragma once

namespace menu::window::layout
{
    constexpr float chrome_height = 46.0f;
    constexpr float panel_radius = 18.0f;
    constexpr float control_height = 30.0f;
    constexpr float control_top = 6.0f;
    constexpr float control_segment_ratio = 1.36f;
    constexpr float control_gap_ratio = 0.09f;

    constexpr float control_width()
    {
        return control_height * (control_segment_ratio * 2.0f + control_gap_ratio);
    }
}
