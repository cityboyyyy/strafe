#include "window.h"
#include "layout.h"

#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <algorithm>
#include <windowsx.h>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

namespace menu::window
{
    namespace
    {
        constexpr wchar_t class_name[] = L"controller_menu";

        HWND g_hwnd = nullptr;
        HINSTANCE g_instance = nullptr;
        float g_scale = 1.0f;
        std::uint32_t g_resize_width = 0;
        std::uint32_t g_resize_height = 0;
        std::vector<rect> g_drag_holes;
        void (*g_activate_handler)(bool) = nullptr;

        LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
        {
            if (msg == WM_SYSKEYDOWN && wparam == VK_F4 && (HIWORD(lparam) & KF_ALTDOWN))
            {
                PostMessageW(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }

            if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
                return 1;

            switch (msg)
            {
            case WM_ACTIVATE:
                if (g_activate_handler)
                    g_activate_handler(LOWORD(wparam) != WA_INACTIVE);
                break;
            case WM_SIZE:
                if (wparam != SIZE_MINIMIZED)
                {
                    g_resize_width = LOWORD(lparam);
                    g_resize_height = HIWORD(lparam);
                }
                return 0;
            case WM_NCHITTEST:
            {
                POINT point{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
                ScreenToClient(hwnd, &point);
                const rect body = panel();
                const float x = static_cast<float>(point.x);
                const float y = static_cast<float>(point.y);
                const bool in_hole = std::any_of(g_drag_holes.begin(), g_drag_holes.end(), [x, y](const rect& hole)
                {
                    return x >= hole.x && x < hole.x + hole.width && y >= hole.y && y < hole.y + hole.height;
                });
                if (!in_hole && x >= body.x && x < body.x + body.width && y >= body.y && y < body.y + 40.0f * g_scale)
                    return HTCAPTION;
                break;
            }
            case WM_SYSCOMMAND:
                if ((wparam & 0xfff0) == SC_KEYMENU)
                    return 0;
                break;
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
            }

            return DefWindowProcW(hwnd, msg, wparam, lparam);
        }
    }

    bool create(const wchar_t* title, int width, int height)
    {
        ImGui_ImplWin32_EnableDpiAwareness();
        g_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));
        g_instance = GetModuleHandleW(nullptr);

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_CLASSDC;
        wc.lpfnWndProc = proc;
        wc.hInstance = g_instance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = class_name;
        if (!RegisterClassExW(&wc))
            return false;

        RECT work{};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
        const float fit = std::min({ g_scale,
            static_cast<float>(work.right - work.left) / static_cast<float>(width),
            static_cast<float>(work.bottom - work.top) / static_cast<float>(height) });
        const int chrome = static_cast<int>(layout::chrome_height * g_scale);
        const int scaled_width = static_cast<int>(width * fit);
        const int scaled_height = static_cast<int>(height * fit) + chrome;
        const int x = work.left + ((work.right - work.left) - scaled_width) / 2;
        const int y = work.top + ((work.bottom - work.top) - scaled_height) / 2;

        g_hwnd = CreateWindowExW(WS_EX_APPWINDOW | WS_EX_NOREDIRECTIONBITMAP, class_name, title, WS_POPUP, x, y, scaled_width, scaled_height, nullptr, nullptr, g_instance, nullptr);
        if (!g_hwnd)
            return false;

        const rect body = panel();
        const rect buttons = controls();
        HRGN region = CreateRectRgn(static_cast<int>(body.x), static_cast<int>(body.y),
            static_cast<int>(body.x + body.width), static_cast<int>(body.y + body.height));
        HRGN pill = CreateRectRgn(static_cast<int>(buttons.x) - 2, static_cast<int>(buttons.y) - 2,
            static_cast<int>(buttons.x + buttons.width) + 2, static_cast<int>(buttons.y + buttons.height) + 2);
        CombineRgn(region, region, pill, RGN_OR);
        DeleteObject(pill);
        SetWindowRgn(g_hwnd, region, FALSE);
        return true;
    }

    rect panel()
    {
        RECT client{};
        GetClientRect(g_hwnd, &client);
        const float top = layout::chrome_height * g_scale;
        return { 0.0f, top, static_cast<float>(client.right), static_cast<float>(client.bottom) - top };
    }

    void set_activate_handler(void (*handler)(bool active))
    {
        g_activate_handler = handler;
    }

    void clear_drag_holes()
    {
        g_drag_holes.clear();
    }

    void add_drag_hole(const rect& area)
    {
        g_drag_holes.push_back(area);
    }

    rect controls()
    {
        RECT client{};
        GetClientRect(g_hwnd, &client);
        const float width = layout::control_width() * g_scale;
        return { static_cast<float>(client.right) - width, layout::control_top * g_scale, width, layout::control_height * g_scale };
    }

    void destroy()
    {
        if (g_hwnd)
        {
            DestroyWindow(g_hwnd);
            g_hwnd = nullptr;
        }
        UnregisterClassW(class_name, g_instance);
    }

    void show()
    {
        ShowWindow(g_hwnd, SW_SHOWDEFAULT);
        UpdateWindow(g_hwnd);
    }

    void minimize()
    {
        ShowWindow(g_hwnd, SW_MINIMIZE);
    }

    void close()
    {
        PostMessageW(g_hwnd, WM_CLOSE, 0, 0);
    }

    bool pump()
    {
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT)
                return false;
        }
        return true;
    }

    HWND handle()
    {
        return g_hwnd;
    }

    float dpi_scale()
    {
        return g_scale;
    }

    bool take_resize(std::uint32_t& width, std::uint32_t& height)
    {
        if (g_resize_width == 0 || g_resize_height == 0)
            return false;

        width = g_resize_width;
        height = g_resize_height;
        g_resize_width = 0;
        g_resize_height = 0;
        return true;
    }
}
