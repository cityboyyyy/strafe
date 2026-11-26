#pragma once

#include <engine/types.h>

#include <windows.h>

#include <functional>
#include <string>

namespace engine::input
{
    class desktop_input
    {
    public:
        using handler = std::function<bool(const input_binding&, bool)>;

        explicit desktop_input(handler on_input);
        ~desktop_input();

        desktop_input(const desktop_input&) = delete;
        desktop_input& operator=(const desktop_input&) = delete;

        bool start(std::string& error);
        void dispose();

    private:
        static LRESULT CALLBACK observe_keyboard(int code, WPARAM message, LPARAM data);
        static LRESULT CALLBACK observe_mouse(int code, WPARAM message, LPARAM data);

        handler on_input_;
        HHOOK keyboard_hook_ = nullptr;
        HHOOK mouse_hook_ = nullptr;
    };
}
