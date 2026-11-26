#include "playstation_input.h"

#include <algorithm>
#include <set>

namespace engine::input
{
    namespace
    {
        control_button bit(std::uint8_t value, int mask, control_button button)
        {
            return (value & mask) == 0 ? control_button::none : button;
        }

        std::int16_t axis(std::uint8_t value, bool invert)
        {
            const int raw = (static_cast<int>(value) - 128) * 256;
            return static_cast<std::int16_t>(std::min(32767, invert ? -raw : raw));
        }
    }

    std::string playstation_input::device::name() const
    {
        switch (model)
        {
        case playstation_model::dual_shock_4:
            return "DualShock 4";
        case playstation_model::dual_sense_edge:
            return "DualSense Edge";
        default:
            return "DualSense";
        }
    }

    void playstation_input::refresh(const std::vector<raw_input::device_entry>& devices_in_windows)
    {
        std::set<HANDLE> handles;
        std::vector<HANDLE> ordered;
        for (const auto& entry : devices_in_windows)
        {
            if (entry.type == raw_input::hid && handles.insert(entry.handle).second)
                ordered.push_back(entry.handle);
        }

        std::vector<HANDLE> gone;
        for (const auto& entry : devices_)
        {
            if (!handles.contains(entry.first))
                gone.push_back(entry.first);
        }
        for (HANDLE handle : gone)
            remove(handle);
        for (HANDLE handle : ordered)
            identify(handle);
    }

    void playstation_input::remove(HANDLE handle)
    {
        std::erase_if(devices_, [handle](const auto& entry) { return entry.first == handle; });
        if (selected_ == handle)
            selected_ = nullptr;
    }

    void playstation_input::read(const raw_input& raw)
    {
        raw_input::hid_reports reports;
        if (!raw.try_get_hid_reports(reports))
            return;
        device* target = identify(raw.device());
        if (target == nullptr)
            return;
        std::size_t offset = reports.offset;
        for (std::size_t report = 0; report < reports.count; ++report, offset += reports.stride)
        {
            control_button pressed = control_button::none;
            gamepad pad;
            if (try_decode(target->model, target->bluetooth, reports.data, reports.length, offset, reports.stride,
                    target->pressed, pressed, pad))
            {
                target->ready = true;
                target->pressed = pressed;
                target->raw = pad;
            }
        }
    }

    playstation_input::selection playstation_input::selected()
    {
        if (selected_ == nullptr)
        {
            for (const auto& entry : devices_)
            {
                if (entry.second && entry.second->ready)
                {
                    selected_ = entry.first;
                    break;
                }
            }
            if (selected_ == nullptr)
            {
                selection empty;
                for (const auto& entry : devices_)
                {
                    if (entry.second)
                    {
                        empty.name = entry.second->name();
                        break;
                    }
                }
                return empty;
            }
        }
        const device& current = **find(selected_);
        selection result;
        result.handle = selected_;
        result.name = current.name();
        result.ready = true;
        result.pressed = current.pressed;
        result.raw = current.raw;
        return result;
    }

    std::vector<playstation_input::device_view> playstation_input::devices() const
    {
        std::vector<device_view> views;
        for (const auto& entry : devices_)
        {
            if (entry.second)
                views.push_back({ entry.first, entry.second->name(), entry.second->bluetooth, entry.second->ready });
        }
        return views;
    }

    std::optional<playstation_input::device>* playstation_input::find(HANDLE handle)
    {
        for (auto& entry : devices_)
        {
            if (entry.first == handle)
                return &entry.second;
        }
        return nullptr;
    }

    playstation_input::device* playstation_input::identify(HANDLE handle)
    {
        if (auto* existing = find(handle))
            return existing->has_value() ? &**existing : nullptr;
        raw_input::hid_info info;
        if (!raw_input::try_get_hid_info(handle, info))
            return nullptr;
        std::optional<playstation_model> kind;
        if (info.vendor == 0x054C && info.usage_page == 1 && (info.usage == 4 || info.usage == 5))
            kind = model(info.product);
        std::optional<device> created;
        if (kind)
        {
            created.emplace();
            created->model = *kind;
            created->bluetooth = raw_input::is_bluetooth(handle);
        }
        devices_.emplace_back(handle, created);
        auto& stored = devices_.back().second;
        return stored ? &*stored : nullptr;
    }

    std::optional<playstation_model> playstation_input::model(DWORD product)
    {
        switch (product)
        {
        case 0x05C4:
        case 0x09CC:
            return playstation_model::dual_shock_4;
        case 0x0CE6:
            return playstation_model::dual_sense;
        case 0x0DF2:
            return playstation_model::dual_sense_edge;
        default:
            return std::nullopt;
        }
    }

    bool playstation_input::try_decode(playstation_model model, bool bluetooth, const std::uint8_t* data,
        std::size_t length, std::size_t offset, std::size_t size, control_button previous, control_button& pressed,
        gamepad& raw)
    {
        pressed = control_button::none;
        raw = {};
        if (data == nullptr || size < 10 || size > length || offset > length - size)
            return false;

        const std::uint8_t* report = data + offset;
        const std::uint8_t id = report[0];
        const bool dual_shock = model == playstation_model::dual_shock_4;
        std::size_t axes = 0;
        bool simple = false;
        if (id == 0x01)
        {
            axes = 1;
            simple = dual_shock || bluetooth || size < 64;
        }
        else if (dual_shock && id == 0x11)
        {
            axes = 3;
            simple = true;
        }
        else if (!dual_shock && id == 0x31)
        {
            axes = 2;
            simple = false;
        }
        else
        {
            return false;
        }

        const std::size_t buttons = axes + (simple ? 4 : 7);
        const std::size_t trigger = axes + (simple ? 7 : 4);
        if (buttons + 2 >= size || trigger + 1 >= size)
            return false;

        const std::uint8_t face = report[buttons];
        const std::uint8_t shoulders = report[buttons + 1];
        const std::uint8_t system = report[buttons + 2];
        control_button digital = control_button::none;
        switch (face & 15)
        {
        case 0:
            digital = control_button::dpad_up;
            break;
        case 1:
            digital = control_button::dpad_up | control_button::dpad_right;
            break;
        case 2:
            digital = control_button::dpad_right;
            break;
        case 3:
            digital = control_button::dpad_down | control_button::dpad_right;
            break;
        case 4:
            digital = control_button::dpad_down;
            break;
        case 5:
            digital = control_button::dpad_down | control_button::dpad_left;
            break;
        case 6:
            digital = control_button::dpad_left;
            break;
        case 7:
            digital = control_button::dpad_up | control_button::dpad_left;
            break;
        default:
            break;
        }
        digital |= bit(face, 0x10, control_button::x) | bit(face, 0x20, control_button::a) |
            bit(face, 0x40, control_button::b) | bit(face, 0x80, control_button::y) |
            bit(shoulders, 0x01, control_button::lb) | bit(shoulders, 0x02, control_button::rb) |
            bit(shoulders, 0x10, control_button::back) | bit(shoulders, 0x20, control_button::start) |
            bit(shoulders, 0x40, control_button::left_stick_press) | bit(shoulders, 0x80, control_button::right_stick_press);

        raw.buttons = static_cast<std::uint16_t>(digital);
        raw.left_x = axis(report[axes], false);
        raw.left_y = axis(report[axes + 1], true);
        raw.right_x = axis(report[axes + 2], false);
        raw.right_y = axis(report[axes + 3], true);
        raw.left_trigger = report[trigger];
        raw.right_trigger = report[trigger + 1];
        pressed = controller::decode(raw, previous) | bit(system, 0x01, control_button::guide) |
            bit(system, 0x02, control_button::touchpad);
        if (!dual_shock && !simple)
            pressed |= bit(system, 0x04, control_button::mute);
        if (model == playstation_model::dual_sense_edge && !simple)
            pressed |= bit(system, 0x10, control_button::left_function) | bit(system, 0x20, control_button::right_function) |
                bit(system, 0x40, control_button::left_paddle) | bit(system, 0x80, control_button::right_paddle);
        return true;
    }
}
