#include "virtual_duo.h"

#include "util/text.h"

#pragma warning(push)
#pragma warning(disable : 4201 4214 4115)
#include "../../../ext/duocontroller/DuoController/DuoController.h"
#pragma warning(pop)

#include <algorithm>
#include <cstring>

namespace engine::output
{
    namespace
    {
        static_assert(sizeof(DUO_CONTROLLER_INPUT_REPORT_XBOX) <= 128);
        static_assert(sizeof(DUO_CONTROLLER_INPUT_REPORT_DUALSENSE) <= 128);

        bool held(control_button buttons, control_button which)
        {
            return any(buttons & which);
        }

        int axis(control_button buttons, control_button positive, control_button negative, int motion)
        {
            const control_button pair = buttons & (positive | negative);
            if (pair == positive)
                return 32767;
            if (pair == negative)
                return -32768;
            return std::clamp(motion, -32768, 32767);
        }

        int hat(control_button buttons, bool playstation)
        {
            const bool up = held(buttons, control_button::dpad_up);
            const bool down = held(buttons, control_button::dpad_down);
            const bool left = held(buttons, control_button::dpad_left);
            const bool right = held(buttons, control_button::dpad_right);
            const int vertical = up == down ? 0 : up ? 1 : -1;
            const int horizontal = left == right ? 0 : right ? 1 : -1;
            int index = -1;
            if (vertical == 1 && horizontal == 0)
                index = 0;
            else if (vertical == 1 && horizontal == 1)
                index = 1;
            else if (vertical == 0 && horizontal == 1)
                index = 2;
            else if (vertical == -1 && horizontal == 1)
                index = 3;
            else if (vertical == -1 && horizontal == 0)
                index = 4;
            else if (vertical == -1 && horizontal == -1)
                index = 5;
            else if (vertical == 0 && horizontal == -1)
                index = 6;
            else if (vertical == 1 && horizontal == -1)
                index = 7;
            if (playstation)
                return index < 0 ? 8 : index;
            return index < 0 ? 0 : index + 1;
        }

        std::uint16_t wide_horizontal(int value)
        {
            return static_cast<std::uint16_t>(value + 32768);
        }

        std::uint16_t wide_vertical(int value)
        {
            return static_cast<std::uint16_t>(32767 - value);
        }

        std::uint8_t narrow_horizontal(int value)
        {
            return static_cast<std::uint8_t>((value + 32768) >> 8);
        }

        std::uint8_t narrow_vertical(int value)
        {
            return static_cast<std::uint8_t>((32767 - value) >> 8);
        }

        std::string hresult_message(const char* prefix, HRESULT result)
        {
            if (result == E_ACCESSDENIED)
                return std::string(prefix) + " Run the app as administrator.";
            if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
                return std::string(prefix) + " The driver package is missing.";
            return std::string(prefix) + " " + util::win32_message(static_cast<DWORD>(result & 0xFFFF));
        }
    }

    std::unique_ptr<virtual_duo> virtual_duo::open(const std::wstring& library, virtual_pad_kind kind, std::string& error)
    {
        std::unique_ptr<virtual_duo> pad(new virtual_duo());
        pad->kind_ = kind;
        pad->module_ = LoadLibraryExW(library.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!pad->module_)
        {
            error = "Could not load the controller driver library.";
            return nullptr;
        }

        const auto initialize = reinterpret_cast<initialize_fn>(GetProcAddress(pad->module_, "DuoController_Initialize"));
        pad->uninitialize_ = reinterpret_cast<initialize_fn>(GetProcAddress(pad->module_, "DuoController_Uninitialize"));
        const auto create = reinterpret_cast<create_fn>(GetProcAddress(pad->module_, "DuoController_CreateController"));
        pad->remove_ = reinterpret_cast<remove_fn>(GetProcAddress(pad->module_, "DuoController_RemoveController"));
        pad->send_ = reinterpret_cast<send_fn>(GetProcAddress(pad->module_, "DuoController_SendReport"));
        if (!initialize || !pad->uninitialize_ || !create || !pad->remove_ || !pad->send_)
        {
            error = "The controller driver library is incomplete.";
            return nullptr;
        }

        HRESULT result = initialize();
        if (FAILED(result))
        {
            error = hresult_message("Could not start the controller driver.", result);
            return nullptr;
        }

        const int type = kind == virtual_pad_kind::dualsense ? DuoControllerTypeDualSense : DuoControllerTypeXbox;
        result = create(type, nullptr, nullptr, &pad->controller_);
        if (FAILED(result) || !pad->controller_)
        {
            pad->controller_ = nullptr;
            error = hresult_message("Could not create the virtual controller.", result);
            return nullptr;
        }
        return pad;
    }

    virtual_duo::~virtual_duo()
    {
        dispose();
    }

    bool virtual_duo::set_buttons(control_button buttons, xbox_motion motion)
    {
        if (!controller_)
            return false;
        const unsigned long long tick = GetTickCount64();
        if (has_last_ && buttons == last_buttons_ && motion == last_motion_ && tick - last_sent_ < 250)
            return true;
        last_buttons_ = buttons;
        last_motion_ = motion;
        last_sent_ = tick;
        has_last_ = true;

        const int left_x = axis(buttons, control_button::left_stick_right, control_button::left_stick_left, motion.left_x);
        const int left_y = axis(buttons, control_button::left_stick_up, control_button::left_stick_down, motion.left_y);
        const int right_x = axis(buttons, control_button::right_stick_right, control_button::right_stick_left, motion.right_x);
        const int right_y = axis(buttons, control_button::right_stick_up, control_button::right_stick_down, motion.right_y);

        if (kind_ == virtual_pad_kind::dualsense)
        {
            DUO_CONTROLLER_INPUT_REPORT_DUALSENSE report;
            std::memcpy(&report, report_.data(), sizeof(report));
            report.LeftStickHorizontal = narrow_horizontal(left_x);
            report.LeftStickVertical = narrow_vertical(left_y);
            report.RightStickHorizontal = narrow_horizontal(right_x);
            report.RightStickVertical = narrow_vertical(right_y);
            report.LeftTrigger = held(buttons, control_button::lt) ? 0xFF : 0;
            report.RightTrigger = held(buttons, control_button::rt) ? 0xFF : 0;
            report.DPad = static_cast<UINT8>(hat(buttons, true));
            report.Cross = held(buttons, control_button::a);
            report.Circle = held(buttons, control_button::b);
            report.Square = held(buttons, control_button::x);
            report.Triangle = held(buttons, control_button::y);
            report.L1 = held(buttons, control_button::lb);
            report.R1 = held(buttons, control_button::rb);
            report.L2 = held(buttons, control_button::lt);
            report.R2 = held(buttons, control_button::rt);
            report.Create = held(buttons, control_button::back);
            report.Options = held(buttons, control_button::start);
            report.L3 = held(buttons, control_button::left_stick_press);
            report.R3 = held(buttons, control_button::right_stick_press);
            report.Home = held(buttons, control_button::guide);
            report.Touchpad = held(buttons, control_button::touchpad);
            report.Mute = held(buttons, control_button::mute);
            report.TouchData.Finger[0].NotTouching = 1;
            report.TouchData.Finger[1].NotTouching = 1;
            report.PowerState = Complete;
            report.PowerPercent = 10;
            report.PluggedUsbData = 1;
            report.PluggedUsbPower = 1;
            const HRESULT result = send_(controller_, &report);
            std::memcpy(report_.data(), &report, sizeof(report));
            return SUCCEEDED(result);
        }

        DUO_CONTROLLER_INPUT_REPORT_XBOX report;
        std::memset(&report, 0, sizeof(report));
        report.Guide = held(buttons, control_button::guide);
        report.Start = held(buttons, control_button::start);
        report.Back = held(buttons, control_button::back);
        report.A = held(buttons, control_button::a);
        report.B = held(buttons, control_button::b);
        report.X = held(buttons, control_button::x);
        report.Y = held(buttons, control_button::y);
        report.DPad = static_cast<UINT8>(hat(buttons, false));
        report.LeftBumper = held(buttons, control_button::lb);
        report.RightBumper = held(buttons, control_button::rb);
        report.LeftStick = held(buttons, control_button::left_stick_press);
        report.RightStick = held(buttons, control_button::right_stick_press);
        report.LeftTrigger = held(buttons, control_button::lt) ? 0xFF : 0;
        report.RightTrigger = held(buttons, control_button::rt) ? 0xFF : 0;
        report.LeftStickHorizontal = wide_horizontal(left_x);
        report.LeftStickVertical = wide_vertical(left_y);
        report.RightStickHorizontal = wide_horizontal(right_x);
        report.RightStickVertical = wide_vertical(right_y);
        report.Paddle1 = held(buttons, control_button::paddle_1);
        report.Paddle2 = held(buttons, control_button::paddle_2);
        report.Paddle3 = held(buttons, control_button::paddle_3);
        report.Paddle4 = held(buttons, control_button::paddle_4);
        return SUCCEEDED(send_(controller_, &report));
    }

    void virtual_duo::dispose()
    {
        if (controller_)
        {
            set_buttons(control_button::none);
            remove_(controller_);
            controller_ = nullptr;
        }
        if (module_)
        {
            if (uninitialize_)
                uninitialize_();
            FreeLibrary(module_);
            module_ = nullptr;
        }
    }
}

namespace engine::output
{
    bool xbox_motion::add(control_button target, int amount)
    {
        switch (target)
        {
        case control_button::left_stick_left:
            left_x -= amount;
            break;
        case control_button::left_stick_right:
            left_x += amount;
            break;
        case control_button::left_stick_down:
            left_y -= amount;
            break;
        case control_button::left_stick_up:
            left_y += amount;
            break;
        case control_button::right_stick_left:
            right_x -= amount;
            break;
        case control_button::right_stick_right:
            right_x += amount;
            break;
        case control_button::right_stick_down:
            right_y -= amount;
            break;
        case control_button::right_stick_up:
            right_y += amount;
            break;
        default:
            return false;
        }
        return true;
    }
}
