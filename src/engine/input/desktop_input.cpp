#include "desktop_input.h"

#include "../output/output.h"
#include "../util/text.h"

#include <utility>

namespace engine::input
{
    namespace
    {
        thread_local desktop_input* current = nullptr;
    }

    desktop_input::desktop_input(handler on_input)
        : on_input_(std::move(on_input))
    {
    }

    desktop_input::~desktop_input()
    {
        dispose();
    }

    bool desktop_input::start(std::string& error)
    {
        if (keyboard_hook_ != nullptr && mouse_hook_ != nullptr)
            return true;
        current = this;
        HMODULE module = GetModuleHandleW(nullptr);
        keyboard_hook_ = SetWindowsHookExW(WH_KEYBOARD_LL, observe_keyboard, module, 0);
        if (keyboard_hook_ == nullptr)
        {
            error = util::win32_message(GetLastError());
            return false;
        }
        mouse_hook_ = SetWindowsHookExW(WH_MOUSE_LL, observe_mouse, module, 0);
        if (mouse_hook_ != nullptr)
            return true;
        const DWORD code = GetLastError();
        dispose();
        error = util::win32_message(code);
        return false;
    }

    LRESULT CALLBACK desktop_input::observe_keyboard(int code, WPARAM message, LPARAM data)
    {
        const auto* info = reinterpret_cast<const KBDLLHOOKSTRUCT*>(data);
        if (code != HC_ACTION || current == nullptr || (info->flags & LLKHF_INJECTED) != 0)
            return CallNextHookEx(nullptr, code, message, data);

        const DWORD flags = info->flags;
        const std::int64_t message_flags = (static_cast<std::int64_t>(info->scanCode) << 16) |
            (static_cast<std::int64_t>(flags & LLKHF_EXTENDED) << 24);
        const std::uint32_t key = output::capture_key(info->vkCode, message_flags);
        if (current->on_input_({ input_kind::keyboard, key }, (flags & LLKHF_UP) == 0))
            return 1;
        return CallNextHookEx(nullptr, code, message, data);
    }

    LRESULT CALLBACK desktop_input::observe_mouse(int code, WPARAM message, LPARAM data)
    {
        const auto* info = reinterpret_cast<const MSLLHOOKSTRUCT*>(data);
        const auto id = static_cast<UINT>(message);
        if (code != HC_ACTION || current == nullptr || id == WM_MOUSEMOVE || (info->flags & LLMHF_INJECTED) != 0)
            return CallNextHookEx(nullptr, code, message, data);

        const int mouse_data = static_cast<std::int32_t>(info->mouseData) >> 16;
        output_action action = output_action::none;
        switch (id)
        {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
            action = output_action::mouse_left;
            break;
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
            action = output_action::mouse_right;
            break;
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
            action = output_action::mouse_middle;
            break;
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
            action = mouse_data == 1 ? output_action::mouse_back : output_action::mouse_forward;
            break;
        case WM_MOUSEWHEEL:
            if (mouse_data != 0)
                action = mouse_data > 0 ? output_action::wheel_up : output_action::wheel_down;
            break;
        default:
            break;
        }
        const bool down = id == WM_LBUTTONDOWN || id == WM_RBUTTONDOWN || id == WM_MBUTTONDOWN ||
            id == WM_XBUTTONDOWN || id == WM_MOUSEWHEEL;
        if (action != output_action::none &&
            current->on_input_({ input_kind::mouse, static_cast<std::uint32_t>(action) }, down))
            return 1;
        return CallNextHookEx(nullptr, code, message, data);
    }

    void desktop_input::dispose()
    {
        if (keyboard_hook_ != nullptr)
            UnhookWindowsHookEx(keyboard_hook_);
        if (mouse_hook_ != nullptr)
            UnhookWindowsHookEx(mouse_hook_);
        keyboard_hook_ = mouse_hook_ = nullptr;
        if (current == this)
            current = nullptr;
    }
}
