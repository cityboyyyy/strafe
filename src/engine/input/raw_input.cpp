#include "raw_input.h"

#include <cstddef>
#include <cstring>
#include <cwctype>

namespace engine::input
{
    namespace
    {
        constexpr std::size_t header_size = sizeof(RAWINPUTHEADER);

        template <typename T>
        T read_value(const std::vector<std::uint8_t>& bytes, std::size_t offset)
        {
            T value{};
            std::memcpy(&value, bytes.data() + offset, sizeof(T));
            return value;
        }

        bool contains_ignore_case(const std::wstring& text, const wchar_t* needle)
        {
            const std::size_t length = std::wcslen(needle);
            if (length == 0 || text.size() < length)
                return false;
            for (std::size_t i = 0; i + length <= text.size(); ++i)
            {
                std::size_t j = 0;
                while (j < length && std::towupper(text[i + j]) == std::towupper(needle[j]))
                    ++j;
                if (j == length)
                    return true;
            }
            return false;
        }
    }

    raw_input::raw_input()
        : packet_(256)
    {
    }

    bool raw_input::read(HRAWINPUT handle)
    {
        UINT size = static_cast<UINT>(packet_.size());
        UINT result = GetRawInputData(handle, RID_INPUT, packet_.data(), &size, static_cast<UINT>(header_size));
        if (result == static_cast<UINT>(-1))
        {
            size = 0;
            if (GetRawInputData(handle, RID_INPUT, nullptr, &size, static_cast<UINT>(header_size)) != static_cast<UINT>(-1) &&
                size > packet_.size() && size <= 65536)
            {
                packet_.assign(size, 0);
                result = GetRawInputData(handle, RID_INPUT, packet_.data(), &size, static_cast<UINT>(header_size));
            }
        }
        length_ = result != static_cast<UINT>(-1) && result >= header_size && result <= packet_.size() ? result : 0;
        return length_ != 0;
    }

    HANDLE raw_input::device() const
    {
        return read_value<HANDLE>(packet_, offsetof(RAWINPUTHEADER, hDevice));
    }

    DWORD raw_input::type() const
    {
        return read_value<DWORD>(packet_, offsetof(RAWINPUTHEADER, dwType));
    }

    bool raw_input::try_get_motion(int& x, int& y) const
    {
        x = y = 0;
        if (length_ < header_size + 24 || type() != mouse || device() == nullptr ||
            (read_value<USHORT>(packet_, header_size) & MOUSE_MOVE_ABSOLUTE) != 0)
            return false;
        x = read_value<LONG>(packet_, header_size + offsetof(RAWMOUSE, lLastX));
        y = read_value<LONG>(packet_, header_size + offsetof(RAWMOUSE, lLastY));
        return x != 0 || y != 0;
    }

    bool raw_input::try_get_hid_reports(hid_reports& reports) const
    {
        reports = {};
        reports.data = packet_.data();
        reports.length = packet_.size();
        reports.offset = header_size + 8;
        if (length_ < reports.offset || type() != hid || device() == nullptr)
            return false;
        const DWORD report_size = read_value<DWORD>(packet_, header_size);
        const DWORD report_count = read_value<DWORD>(packet_, header_size + 4);
        if (report_size == 0 || report_count == 0 || report_count > (length_ - reports.offset) / report_size)
            return false;
        reports.stride = report_size;
        reports.count = report_count;
        return true;
    }

    bool raw_input::register_device(HWND window, USHORT usage, bool notify)
    {
        RAWINPUTDEVICE device{};
        device.usUsagePage = 1;
        device.usUsage = usage;
        device.dwFlags = RIDEV_INPUTSINK | (notify ? RIDEV_DEVNOTIFY : 0);
        device.hwndTarget = window;
        return RegisterRawInputDevices(&device, 1, sizeof(device)) != FALSE;
    }

    std::optional<std::vector<raw_input::device_entry>> raw_input::devices()
    {
        for (int attempt = 0; attempt < 3; ++attempt)
        {
            UINT count = 0;
            if (GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST)) == static_cast<UINT>(-1) || count > 4096)
                return std::nullopt;
            if (count == 0)
                return std::vector<device_entry>{};

            std::vector<RAWINPUTDEVICELIST> list(count);
            const UINT found = GetRawInputDeviceList(list.data(), &count, sizeof(RAWINPUTDEVICELIST));
            if (found == static_cast<UINT>(-1))
                continue;
            list.resize(found);
            std::vector<device_entry> entries;
            entries.reserve(list.size());
            for (const auto& item : list)
                entries.push_back({ item.hDevice, item.dwType });
            return entries;
        }
        return std::nullopt;
    }

    bool raw_input::try_get_hid_info(HANDLE device, hid_info& info)
    {
        RID_DEVICE_INFO data{};
        data.cbSize = sizeof(data);
        UINT size = sizeof(data);
        if (GetRawInputDeviceInfoW(device, RIDI_DEVICEINFO, &data, &size) == static_cast<UINT>(-1) || data.dwType != hid)
            return false;
        info.vendor = data.hid.dwVendorId;
        info.product = data.hid.dwProductId;
        info.usage_page = data.hid.usUsagePage;
        info.usage = data.hid.usUsage;
        return true;
    }

    std::wstring raw_input::device_path(HANDLE device)
    {
        UINT size = 0;
        if (GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, nullptr, &size) == static_cast<UINT>(-1) || size == 0 || size > 4096)
            return {};
        std::wstring name(size, L'\0');
        if (GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, name.data(), &size) == static_cast<UINT>(-1))
            return {};
        name.resize(std::wcslen(name.c_str()));
        return name;
    }

    bool raw_input::is_bluetooth(HANDLE device)
    {
        const std::wstring path = device_path(device);
        return contains_ignore_case(path, L"VID&") || contains_ignore_case(path, L"BTHENUM") ||
            contains_ignore_case(path, L"00001124-0000-1000-8000-00805F9B34FB");
    }
}
