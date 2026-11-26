#include "input_capture.h"

#include "../model/emulation.h"

#include <Windows.h>
#include <imgui.h>

namespace menu::ui
{
    namespace
    {
        constexpr double grace = 0.18;

        bool skipped(int key)
        {
            return key == VK_SHIFT || key == VK_CONTROL || key == VK_MENU || key == VK_PACKET || key == VK_PROCESSKEY;
        }

        bool down(int key)
        {
            return (GetAsyncKeyState(key) & 0x8000) != 0;
        }

        bool focused()
        {
            const HWND active = GetActiveWindow();
            return active != nullptr && GetForegroundWindow() == active;
        }
    }

    void input_capture::begin(int target)
    {
        active_ = true;
        armed_ = false;
        target_ = target;
        started_ = ImGui::GetTime();
        previous_.fill(true);
    }

    void input_capture::cancel()
    {
        active_ = false;
        armed_ = false;
        target_ = -1;
    }

    std::optional<std::uint32_t> input_capture::poll(bool allow_left_click)
    {
        if (!active_)
            return std::nullopt;
        if (!focused())
        {
            cancel();
            return std::nullopt;
        }

        std::array<bool, 256> now{};
        bool any = false;
        for (int key = 1; key < 255; ++key)
        {
            if (skipped(key))
                continue;
            now[static_cast<std::size_t>(key)] = down(key);
            any = any || now[static_cast<std::size_t>(key)];
        }

        const bool settled = ImGui::GetTime() - started_ >= grace;
        if (!armed_)
        {
            armed_ = settled && !any;
            previous_ = now;
            return std::nullopt;
        }

        const float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f)
            return wheel > 0.0f ? model::wheel_up_code : model::wheel_down_code;

        std::optional<std::uint32_t> result;
        for (int key = 1; key < 255 && !result; ++key)
        {
            const auto index = static_cast<std::size_t>(key);
            if (!now[index] || previous_[index])
                continue;
            if (key == VK_LBUTTON && !allow_left_click)
                continue;
            result = static_cast<std::uint32_t>(key);
        }
        previous_ = now;
        return result;
    }
}
