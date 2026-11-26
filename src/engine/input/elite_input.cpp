#include "elite_input.h"

#include <windows.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Gaming.Input.h>

#include <atomic>

namespace engine::input
{
    namespace
    {
        namespace gaming = winrt::Windows::Gaming::Input;

        bool is_elite(std::uint16_t vendor, std::uint16_t product)
        {
            return vendor == 0x045E && (product == 0x02E3 || product == 0x0B00 || product == 0x0B05 || product == 0x0B22);
        }
    }

    struct elite_input::state
    {
        bool apartment = false;
        bool ready = false;
        std::atomic<bool> dirty = true;
        gaming::Gamepad elite{ nullptr };
        winrt::event_token added{};
        winrt::event_token removed{};
    };

    elite_input::elite_input()
        : state_(std::make_unique<state>())
    {
        try
        {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            state_->apartment = true;
        }
        catch (const winrt::hresult_error& failure)
        {
            if (failure.code() != RPC_E_CHANGED_MODE)
                return;
        }
        try
        {
            state* shared = state_.get();
            state_->added = gaming::Gamepad::GamepadAdded([shared](const auto&, const auto&) { shared->dirty = true; });
            state_->removed = gaming::Gamepad::GamepadRemoved([shared](const auto&, const auto&) { shared->dirty = true; });
            state_->ready = true;
        }
        catch (...)
        {
            state_->ready = false;
        }
    }

    elite_input::~elite_input()
    {
        if (state_->ready)
        {
            try
            {
                gaming::Gamepad::GamepadAdded(state_->added);
                gaming::Gamepad::GamepadRemoved(state_->removed);
            }
            catch (...)
            {
            }
        }
        state_->elite = nullptr;
        if (state_->apartment)
            winrt::uninit_apartment();
    }

    void elite_input::refresh()
    {
        if (!state_->ready || !state_->dirty.exchange(false))
            return;
        try
        {
            gaming::Gamepad found{ nullptr };
            for (const auto& pad : gaming::Gamepad::Gamepads())
            {
                const auto raw = gaming::RawGameController::FromGameController(pad);
                if (raw && is_elite(raw.HardwareVendorId(), raw.HardwareProductId()))
                {
                    found = pad;
                    break;
                }
            }
            state_->elite = found;
        }
        catch (...)
        {
            state_->elite = nullptr;
            state_->dirty = true;
        }
    }

    control_button elite_input::paddles()
    {
        if (!state_->elite)
            return control_button::none;
        try
        {
            const auto buttons = state_->elite.GetCurrentReading().Buttons;
            control_button result = control_button::none;
            if ((buttons & gaming::GamepadButtons::Paddle1) == gaming::GamepadButtons::Paddle1)
                result |= control_button::paddle_1;
            if ((buttons & gaming::GamepadButtons::Paddle2) == gaming::GamepadButtons::Paddle2)
                result |= control_button::paddle_2;
            if ((buttons & gaming::GamepadButtons::Paddle3) == gaming::GamepadButtons::Paddle3)
                result |= control_button::paddle_3;
            if ((buttons & gaming::GamepadButtons::Paddle4) == gaming::GamepadButtons::Paddle4)
                result |= control_button::paddle_4;
            return result;
        }
        catch (...)
        {
            state_->elite = nullptr;
            state_->dirty = true;
            return control_button::none;
        }
    }

    bool elite_input::available() const
    {
        return static_cast<bool>(state_->elite);
    }
}
