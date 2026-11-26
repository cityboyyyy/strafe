#pragma once

#include <windows.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace engine::input
{
    class raw_input
    {
    public:
        static constexpr UINT message = WM_INPUT;
        static constexpr UINT device_change_message = WM_INPUT_DEVICE_CHANGE;
        static constexpr DWORD mouse = RIM_TYPEMOUSE;
        static constexpr DWORD keyboard = RIM_TYPEKEYBOARD;
        static constexpr DWORD hid = RIM_TYPEHID;

        struct device_entry
        {
            HANDLE handle = nullptr;
            DWORD type = 0;
        };

        struct hid_info
        {
            DWORD vendor = 0;
            DWORD product = 0;
            USHORT usage_page = 0;
            USHORT usage = 0;
        };

        struct hid_reports
        {
            const std::uint8_t* data = nullptr;
            std::size_t offset = 0;
            std::size_t stride = 0;
            std::size_t count = 0;
            std::size_t length = 0;
        };

        raw_input();

        bool read(HRAWINPUT handle);
        HANDLE device() const;
        DWORD type() const;
        bool try_get_motion(int& x, int& y) const;
        bool try_get_hid_reports(hid_reports& reports) const;

        static bool register_device(HWND window, USHORT usage, bool notify = false);
        static std::optional<std::vector<device_entry>> devices();
        static bool try_get_hid_info(HANDLE device, hid_info& info);
        static std::wstring device_path(HANDLE device);
        static bool is_bluetooth(HANDLE device);

    private:
        std::vector<std::uint8_t> packet_;
        std::size_t length_ = 0;
    };
}
