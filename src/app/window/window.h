#pragma once

#include <Windows.h>
#include <cstdint>

namespace menu::window
{
    struct rect
    {
        float x;
        float y;
        float width;
        float height;
    };

    bool create(const wchar_t* title, int width, int height);
    void destroy();
    void show();
    void minimize();
    void close();
    bool pump();
    HWND handle();
    float dpi_scale();
    bool take_resize(std::uint32_t& width, std::uint32_t& height);
    rect panel();
    rect controls();
    void set_activate_handler(void (*handler)(bool active));
    void clear_drag_holes();
    void add_drag_hole(const rect& area);
}
