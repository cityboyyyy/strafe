#include <engine/engine.h>

#include "remap_engine.h"

#include "../util/text.h"

#include <windows.h>
#include <hidsdi.h>

#include <algorithm>
#include <deque>
#include <functional>
#include <future>
#include <map>
#include <mutex>
#include <thread>

namespace engine
{
    namespace
    {
        constexpr UINT command_message = WM_APP + 1;
        constexpr UINT quit_message = WM_APP + 2;
        constexpr wchar_t class_name[] = L"engine.remapper";
        constexpr long long poll_period_ms = 2;
        constexpr long long publish_period_ms = 15;
        constexpr ULONGLONG device_refresh_ms = 1500;

        float stick(std::int16_t value)
        {
            return std::clamp(static_cast<float>(value) / 32767.0f, -1.0f, 1.0f);
        }

        float trigger(std::uint8_t value)
        {
            return static_cast<float>(value) / 255.0f;
        }

        std::string product_name(HANDLE device, const char* fallback)
        {
            const std::wstring path = input::raw_input::device_path(device);
            if (path.empty())
                return fallback;
            HANDLE file = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
            if (file == INVALID_HANDLE_VALUE)
                return fallback;
            wchar_t buffer[127]{};
            const bool ok = HidD_GetProductString(file, buffer, sizeof(buffer)) != FALSE;
            CloseHandle(file);
            if (!ok || buffer[0] == L'\0')
                return fallback;
            buffer[std::size(buffer) - 1] = L'\0';
            return util::to_utf8(buffer);
        }
    }

    struct remapper::impl
    {
        std::thread thread;
        HWND window = nullptr;
        mutable std::mutex mutex;
        std::deque<std::packaged_task<void()>> commands;
        status snapshot;
        std::vector<device_info> device_list;
        binding_map stored;
        remapping::remap_engine* core = nullptr;
        std::map<HANDLE, std::string> names;
        std::vector<input::raw_input::device_entry> known_entries;
        bool xinput_slots[4]{};
        ULONGLONG next_device_refresh = 0;

        void run(std::promise<bool>& ready);
        void drain();
        void publish();
        void refresh_devices();
        void execute(const std::function<void(remapping::remap_engine&)>& action);

        static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    };

    LRESULT CALLBACK remapper::impl::window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
    {
        if (message == WM_NCCREATE)
        {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        }
        auto* self = reinterpret_cast<impl*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (self != nullptr && self->core != nullptr)
        {
            switch (message)
            {
            case WM_INPUT:
                self->core->process_raw_input(reinterpret_cast<HRAWINPUT>(lparam));
                break;
            case WM_INPUT_DEVICE_CHANGE:
                self->core->handle_raw_input_device_change(reinterpret_cast<HANDLE>(lparam), wparam == GIDC_REMOVAL);
                self->next_device_refresh = 0;
                return 0;
            case command_message:
                self->drain();
                return 0;
            case quit_message:
                PostQuitMessage(0);
                return 0;
            default:
                break;
            }
        }
        return DefWindowProcW(window, message, wparam, lparam);
    }

    void remapper::impl::run(std::promise<bool>& ready)
    {
        HINSTANCE instance = GetModuleHandleW(nullptr);
        WNDCLASSEXW type{};
        type.cbSize = sizeof(type);
        type.lpfnWndProc = window_proc;
        type.hInstance = instance;
        type.lpszClassName = class_name;
        if (RegisterClassExW(&type) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
            ready.set_value(false);
            return;
        }
        HWND created = CreateWindowExW(0, class_name, L"", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, instance, this);
        if (created == nullptr)
        {
            ready.set_value(false);
            return;
        }

        HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        if (timer == nullptr)
            timer = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
        LARGE_INTEGER due{};
        due.QuadPart = -poll_period_ms * 10000;
        if (timer == nullptr || !SetWaitableTimer(timer, &due, static_cast<LONG>(poll_period_ms), nullptr, nullptr, FALSE))
        {
            if (timer != nullptr)
                CloseHandle(timer);
            DestroyWindow(created);
            ready.set_value(false);
            return;
        }

        {
            remapping::remap_engine engine;
            core = &engine;
            window = created;
            engine.start_detection(created);
            engine.poll();
            publish();
            ready.set_value(true);

            bool running = true;
            long long last_publish = 0;
            while (running)
            {
                const DWORD result = MsgWaitForMultipleObjectsEx(1, &timer, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
                if (result == WAIT_OBJECT_0)
                {
                    engine.poll();
                    const long long tick = static_cast<long long>(GetTickCount64());
                    if (tick - last_publish >= publish_period_ms)
                    {
                        last_publish = tick;
                        publish();
                    }
                }
                else if (result == WAIT_FAILED)
                {
                    running = false;
                }
                MSG message{};
                while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
                {
                    if (message.message == WM_QUIT)
                    {
                        running = false;
                        break;
                    }
                    DispatchMessageW(&message);
                }
            }
            drain();
            core = nullptr;
        }

        CancelWaitableTimer(timer);
        CloseHandle(timer);
        DestroyWindow(created);
    }

    void remapper::impl::drain()
    {
        for (;;)
        {
            std::packaged_task<void()> task;
            {
                std::lock_guard lock(mutex);
                if (commands.empty())
                    break;
                task = std::move(commands.front());
                commands.pop_front();
            }
            task();
        }
    }

    void remapper::impl::execute(const std::function<void(remapping::remap_engine&)>& action)
    {
        if (window == nullptr)
            return;
        std::packaged_task<void()> task([this, &action]
        {
            if (core == nullptr)
                return;
            action(*core);
            publish();
        });
        auto done = task.get_future();
        {
            std::lock_guard lock(mutex);
            commands.push_back(std::move(task));
        }
        if (!PostMessageW(window, command_message, 0, 0))
        {
            std::lock_guard lock(mutex);
            commands.clear();
            return;
        }
        done.get();
    }

    void remapper::impl::refresh_devices()
    {
        const ULONGLONG now = GetTickCount64();
        const auto& entries = core->device_entries();
        const bool changed = entries.size() != known_entries.size() ||
            !std::equal(entries.begin(), entries.end(), known_entries.begin(),
                [](const auto& a, const auto& b) { return a.handle == b.handle && a.type == b.type; });
        if (!changed && now < next_device_refresh)
            return;
        next_device_refresh = now + device_refresh_ms;
        known_entries = entries;

        for (std::uint32_t slot = 0; slot < 4; ++slot)
            xinput_slots[slot] = input::controller::connected(slot);

        std::map<HANDLE, std::string> kept;
        std::vector<device_info> list;
        int keyboard_count = 0;
        int mouse_count = 0;
        for (const auto& entry : entries)
        {
            if (entry.type != input::raw_input::keyboard && entry.type != input::raw_input::mouse)
                continue;
            const bool keyboard = entry.type == input::raw_input::keyboard;
            auto found = names.find(entry.handle);
            std::string name = found != names.end() ? found->second : product_name(entry.handle, keyboard ? "Keyboard" : "Mouse");
            kept[entry.handle] = name;
            device_info info;
            info.kind = keyboard ? device_kind::keyboard : device_kind::mouse;
            info.name = std::move(name);
            info.slot = keyboard ? keyboard_count++ : mouse_count++;
            list.push_back(std::move(info));
        }
        names = std::move(kept);

        for (int slot = 0; slot < 4; ++slot)
        {
            if (!xinput_slots[slot])
                continue;
            device_info info;
            info.kind = device_kind::xinput;
            info.name = "XInput slot " + std::to_string(slot + 1);
            info.slot = slot;
            info.selected = !core->playstation_only() && core->controller_slot() == slot;
            list.push_back(std::move(info));
        }

        int playstation_index = 0;
        for (const auto& pad : core->playstation_devices())
        {
            device_info info;
            info.kind = device_kind::playstation;
            info.name = pad.name;
            info.slot = playstation_index++;
            info.bluetooth = pad.bluetooth;
            info.selected = pad.handle == core->active_playstation();
            list.push_back(std::move(info));
        }

        std::lock_guard lock(mutex);
        device_list = std::move(list);
    }

    void remapper::impl::publish()
    {
        if (core == nullptr)
            return;
        refresh_devices();
        status next;
        next.active = core->active();
        next.starting = core->starting();
        next.pad = core->pad_status();
        next.editor_focused = core->editor_focused();
        next.error = core->error().value_or(std::string());
        next.desktop_ready = core->desktop_ready();
        next.motion_ready = core->motion_ready();
        next.playstation_ready = core->playstation_ready();
        next.controller.connected = core->controller_connected();
        next.controller.name = core->controller_device_name().value_or(std::string());
        next.controller.pressed = core->controller_pressed();
        const input::gamepad& raw = core->controller_raw();
        next.controller.left_x = stick(raw.left_x);
        next.controller.left_y = stick(raw.left_y);
        next.controller.right_x = stick(raw.right_x);
        next.controller.right_y = stick(raw.right_y);
        next.controller.left_trigger = trigger(raw.left_trigger);
        next.controller.right_trigger = trigger(raw.right_trigger);
        next.controller.paddles_available = core->paddles_available();
        if (const auto& counts = core->devices())
            next.devices = device_counts{ counts->keyboards, counts->mice };
        next.last_keyboard = core->last_keyboard();
        next.last_mouse = core->last_mouse();

        std::lock_guard lock(mutex);
        snapshot = std::move(next);
    }

    remapper::remapper()
        : impl_(std::make_unique<impl>())
    {
        std::promise<bool> ready;
        auto started = ready.get_future();
        impl* state = impl_.get();
        impl_->thread = std::thread([state, &ready] { state->run(ready); });
        if (!started.get())
        {
            impl_->thread.join();
            impl_->snapshot.error = "The remapping engine could not start.";
        }
    }

    remapper::~remapper()
    {
        if (impl_->thread.joinable())
        {
            if (impl_->window != nullptr)
                PostMessageW(impl_->window, quit_message, 0, 0);
            impl_->thread.join();
        }
    }

    bool remapper::select_xinput(int slot)
    {
        bool accepted = false;
        impl_->execute([&](remapping::remap_engine& core) { accepted = core.select_controller(slot); });
        return accepted;
    }

    void remapper::select_playstation()
    {
        impl_->execute([](remapping::remap_engine& core) { core.select_playstation_controller(); });
    }

    void remapper::set_pad_options(const pad_options& options)
    {
        impl_->execute([options](remapping::remap_engine& core) { core.set_pad_options(options); });
    }

    void remapper::set_aim(const aim_settings& settings)
    {
        impl_->execute([settings](remapping::remap_engine& core) { core.set_aim(settings); });
    }

    void remapper::prepare_pad()
    {
        impl_->execute([](remapping::remap_engine& core) { core.prepare_pad(); });
    }

    void remapper::set_bindings(const binding_map& bindings)
    {
        {
            std::lock_guard lock(impl_->mutex);
            impl_->stored = bindings;
        }
        impl_->execute([&](remapping::remap_engine& core) { core.replace_bindings(bindings); });
    }

    binding_map remapper::bindings() const
    {
        std::lock_guard lock(impl_->mutex);
        return impl_->stored;
    }

    bool remapper::start()
    {
        const binding_map profile = bindings();
        bool active = false;
        impl_->execute([&](remapping::remap_engine& core)
        {
            core.start(profile);
            active = core.active() || core.starting();
        });
        return active;
    }

    void remapper::stop()
    {
        impl_->execute([](remapping::remap_engine& core) { core.stop(); });
    }

    void remapper::set_editor_focused(bool focused)
    {
        impl_->execute([focused](remapping::remap_engine& core) { core.set_editor_focused(focused); });
    }

    status remapper::state() const
    {
        std::lock_guard lock(impl_->mutex);
        return impl_->snapshot;
    }

    std::vector<device_info> remapper::devices() const
    {
        std::lock_guard lock(impl_->mutex);
        return impl_->device_list;
    }
}
