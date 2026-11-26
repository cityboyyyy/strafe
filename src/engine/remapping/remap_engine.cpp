#include "remap_engine.h"

#include "../bindings/bindings.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace engine::remapping
{
    namespace
    {
        int add_motion(int prior, int value)
        {
            return static_cast<int>(std::clamp(static_cast<long long>(prior) + value, -1024ll, 1024ll));
        }

        bool is_modifier(std::uint32_t key)
        {
            return key == output::keys::left_control || key == output::keys::right_control ||
                key == output::keys::left_menu || key == output::keys::right_menu;
        }
    }

    remap_engine::remap_engine()
        : desktop_([this](const input_binding& source, bool down) { return handle_desktop(source, down); })
        , output_([this](output_action action, bool down) { return windows_output_.send(action, down); })
    {
    }

    remap_engine::~remap_engine()
    {
        stop();
        desktop_.dispose();
        if (pad_job_)
            pad_job_->worker.join();
        pad_job_.reset();
        xbox_.reset();
    }

    long long remap_engine::now() const
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - clock_start_).count();
    }

    void remap_engine::start_detection(HWND window)
    {
        motion_ready_ = input::raw_input::register_device(window, 2);
        const bool gamepad_ready = input::raw_input::register_device(window, 5, true);
        const bool joystick_ready = input::raw_input::register_device(window, 4, true);
        playstation_ready_ = gamepad_ready || joystick_ready;
        next_desktop_check_ = 0;
        std::string failure;
        if (desktop_.start(failure))
            desktop_ready_ = true;
        else
            error_ = "Keyboard and mouse capture could not start. " + failure;
    }

    bool remap_engine::select_controller(int slot)
    {
        if (slot < 0 || slot > 3)
            return false;
        stop();
        playstation_only_ = false;
        controller_slot_ = slot;
        controller_.reset();
        poll();
        return true;
    }

    void remap_engine::select_playstation_controller()
    {
        stop();
        playstation_only_ = true;
        poll();
    }

    bool remap_engine::needs_xbox(const binding_map& profile)
    {
        return std::any_of(profile.begin(), profile.end(), [](const binding_map::entry& row)
        {
            return std::any_of(row.value.targets.begin(), row.value.targets.end(), output::is_xbox);
        });
    }

    void remap_engine::start(const binding_map& profile)
    {
        stop();
        if (!output_.update(desired_))
            return;
        error_.reset();
        if (profile.empty())
        {
            error_ = "Add or import a binding before starting.";
            return;
        }
        if (!desktop_ready_ && std::any_of(profile.begin(), profile.end(),
                [](const binding_map::entry& row) { return row.source.kind != input_kind::controller; }))
        {
            error_ = "Keyboard and mouse capture is unavailable.";
            return;
        }
        if (!motion_ready_ && std::any_of(profile.begin(), profile.end(),
                [](const binding_map::entry& row) { return bindings::is_motion(row.source); }))
        {
            error_ = "Windows mouse motion capture is unavailable.";
            return;
        }

        if (needs_xbox(profile))
        {
            control_button ignored = control_button::none;
            input::gamepad raw;
            controller_eligible_ = !playstation_only_ &&
                controller_.try_read(static_cast<std::uint32_t>(controller_slot_), ignored, raw);
            bindings_ = profile;
            controller_neutral_ = false;
            if (xbox_ && pad_matches() && !pad_job_)
            {
                activate();
                return;
            }
            pending_start_ = true;
            prepare_pad();
            return;
        }
        bindings_ = profile;
        controller_neutral_ = false;
        active_ = true;
    }

    void remap_engine::activate()
    {
        active_ = true;
        xbox_ready_deadline_ = now() + 2500;
    }

    void remap_engine::prepare_pad()
    {
        if (pad_job_ || (xbox_ && pad_matches()))
            return;
        if (active_)
            stop();
        auto job = std::make_unique<pad_job>();
        job->options = pad_options_;
        job->retired = std::move(xbox_);
        pad_error_.clear();
        pad_job* raw = job.get();
        job->worker = std::thread([raw]
        {
            raw->retired.reset();
            std::string failure;
            if (raw->options.duo_library.empty())
                failure = "Install the controller driver first.";
            else
                raw->pad = output::virtual_duo::open(raw->options.duo_library, raw->options.kind, failure);
            raw->error = failure;
            raw->done.store(true);
        });
        pad_job_ = std::move(job);
    }

    void remap_engine::finish_pad_job()
    {
        if (!pad_job_ || !pad_job_->done.load())
            return;
        pad_job_->worker.join();
        xbox_ = std::move(pad_job_->pad);
        pad_built_ = pad_job_->options;
        pad_error_ = xbox_ ? std::string() : pad_job_->error;
        pad_job_.reset();
        if (!pending_start_)
            return;
        pending_start_ = false;
        if (xbox_)
            activate();
        else
        {
            controller_eligible_ = true;
            error_ = pad_error_;
        }
    }

    pad_connection remap_engine::pad_status() const
    {
        if (pad_job_)
            return pad_connection::connecting;
        if (xbox_)
            return pad_connection::connected;
        return pad_error_.empty() ? pad_connection::none : pad_connection::failed;
    }

    bool remap_engine::replace_bindings(const binding_map& profile)
    {
        if (!active_)
        {
            bindings_ = profile;
            return true;
        }
        if (profile.empty() || (needs_xbox(profile) && !xbox_) ||
            (!desktop_ready_ && std::any_of(profile.begin(), profile.end(),
                [](const binding_map::entry& row) { return row.source.kind != input_kind::controller; })) ||
            (!motion_ready_ && std::any_of(profile.begin(), profile.end(),
                [](const binding_map::entry& row) { return bindings::is_motion(row.source); })))
        {
            start(profile);
            return active_;
        }
        release();
        bindings_ = profile;
        std::erase_if(swallowed_, [this](const input_binding& source) { return !bindings_.contains(source); });
        return true;
    }

    void remap_engine::set_editor_focused(bool focused)
    {
        window_active_ = focused;
        if (focused)
            release();
    }

    void remap_engine::stop()
    {
        active_ = false;
        pending_start_ = false;
        stop_requested_ = false;
        modifiers_.clear();
        release();
        controller_eligible_ = true;
        next_controller_check_ = 0;
    }

    void remap_engine::release()
    {
        events_.clear();
        held_.clear();
        pulses_.clear();
        queued_motion_x_ = queued_motion_y_ = motion_x_ = motion_y_ = 0;
        controller_neutral_ = false;
        desired_.clear();
        if (!output_.update(desired_))
            error_ = "Windows blocked an output release. Switch to a regular desktop app.";
        if (xbox_)
            xbox_->set_buttons(control_button::none);
    }

    void remap_engine::process_raw_input(HRAWINPUT handle)
    {
        if (!raw_input_.read(handle))
            return;
        if (raw_input_.type() == input::raw_input::hid)
        {
            if (playstation_ready_)
                playstation_.read(raw_input_);
            return;
        }
        int x = 0;
        int y = 0;
        if ((!window_active_ && !active_) || !raw_input_.try_get_motion(x, y))
            return;
        if (window_active_)
        {
            const output_action direction = std::llabs(x) >= std::llabs(y)
                ? (x < 0 ? output_action::move_left : output_action::move_right)
                : (y < 0 ? output_action::move_up : output_action::move_down);
            last_mouse_ = output::code(direction);
        }
        if (active_ && !paused() && !menu_mouse_)
        {
            queued_motion_x_ = add_motion(queued_motion_x_, x);
            queued_motion_y_ = add_motion(queued_motion_y_, y);
        }
    }

    void remap_engine::handle_raw_input_device_change(HANDLE device, bool removed)
    {
        if (removed)
        {
            playstation_.remove(device);
            poll_controller(now());
        }
        next_desktop_check_ = 0;
    }

    int remap_engine::motion_for(output_action action) const
    {
        switch (action)
        {
        case output_action::move_left:
            return std::max(0, -motion_x_);
        case output_action::move_right:
            return std::max(0, motion_x_);
        case output_action::move_up:
            return std::max(0, -motion_y_);
        case output_action::move_down:
            return std::max(0, motion_y_);
        default:
            return 0;
        }
    }

    bool remap_engine::handle_desktop(const input_binding& source, bool down)
    {
        const bool wheel = bindings::is_wheel(source);
        if (window_active_ && down)
        {
            if (source.kind == input_kind::keyboard)
                last_keyboard_ = static_cast<std::uint32_t>(source.code);
            else
                last_mouse_ = static_cast<std::uint32_t>(source.code);
        }

        if (active_ && source.kind == input_kind::keyboard)
        {
            const auto key = static_cast<std::uint32_t>(source.code);
            if (is_modifier(key))
            {
                if (down)
                    modifiers_.insert(key);
                else
                    modifiers_.erase(key);
            }
            else if (down && key == output::keys::f12 &&
                (modifiers_.contains(output::keys::left_control) || modifiers_.contains(output::keys::right_control)) &&
                (modifiers_.contains(output::keys::left_menu) || modifiers_.contains(output::keys::right_menu)))
            {
                stop_requested_ = true;
                swallowed_.insert(source);
                return true;
            }
        }

        if (!wheel && !down)
        {
            physical_down_.erase(source);
            const bool consumed = swallowed_.erase(source) != 0;
            if (active_ && !passes(source) && bindings_.contains(source))
                events_.push_back({ source, false });
            return consumed;
        }
        const binding* target = !active_ || passes(source) ? nullptr : bindings_.find(source);
        if (target == nullptr)
            return swallowed_.contains(source);
        if (!wheel && !physical_down_.insert(source).second)
            return swallowed_.contains(source);

        events_.push_back({ source, down });
        const bool suppress = target->suppress &&
            std::find(target->targets.begin(), target->targets.end(), static_cast<output_action>(source.code)) ==
                target->targets.end();
        if (suppress && !wheel)
            swallowed_.insert(source);
        return suppress;
    }

    void remap_engine::poll()
    {
        const long long current = now();
        if (current >= next_desktop_check_)
        {
            if (auto found = input::raw_input::devices())
            {
                const int keyboards = static_cast<int>(std::count_if(found->begin(), found->end(),
                    [](const auto& device) { return device.type == input::raw_input::keyboard; }));
                const int mice = static_cast<int>(std::count_if(found->begin(), found->end(),
                    [](const auto& device) { return device.type == input::raw_input::mouse; }));
                if (devices_ && keyboards < devices_->keyboards)
                    drop_input_kind(input_kind::keyboard);
                if (devices_ && mice < devices_->mice)
                    drop_input_kind(input_kind::mouse);
                devices_ = device_counts{ keyboards, mice };
                if (playstation_ready_)
                    playstation_.refresh(*found);
                device_entries_ = std::move(*found);
            }
            next_desktop_check_ = current + 1500;
        }
        poll_controller(current);
        finish_pad_job();
        if (current >= next_target_check_)
        {
            next_target_check_ = current + 50;
            update_target();
        }

        if (stop_requested_)
            stop();
        if (!active_ || paused())
        {
            desired_.clear();
            output_.update(desired_);
            return;
        }
        if (xbox_ready_deadline_ > 0 && xbox_)
        {
            if (!xbox_->set_buttons(control_button::none))
            {
                if (current >= xbox_ready_deadline_)
                    fail("The virtual Xbox controller did not become ready.");
                return;
            }
            xbox_ready_deadline_ = 0;
        }

        if (controller_ready_ && controller_pressed_ == control_button::none)
            controller_neutral_ = true;
        motion_x_ = queued_motion_x_;
        motion_y_ = queued_motion_y_;
        queued_motion_x_ = queued_motion_y_ = 0;
        update_aim();
        std::erase_if(pulses_, [current](const auto& item) { return item.second <= current; });

        while (!events_.empty())
        {
            const pending_event change = events_.front();
            events_.pop_front();
            if (bindings::is_wheel(change.source))
                pulses_[change.source] = current + 45;
            else if (change.down)
                held_.insert(change.source);
            else
                held_.erase(change.source);
            if (!apply())
                return;
        }

        if (apply() && !output::windows_output::move_pointer(delta(output_action::move_right, output_action::move_left),
                delta(output_action::move_down, output_action::move_up)))
            fail("Windows blocked pointer output.");
    }

    void remap_engine::poll_controller(long long current)
    {
        elite_.refresh();
        if (playstation_only_ || !controller_eligible_)
        {
            xinput_connected_ = false;
            xinput_pressed_ = control_button::none;
            xinput_raw_ = {};
        }
        else if (xinput_connected_ || current >= next_controller_check_)
        {
            xinput_connected_ = controller_.try_read(static_cast<std::uint32_t>(controller_slot_), xinput_pressed_, xinput_raw_);
            if (xinput_connected_)
                xinput_pressed_ |= elite_.paddles();
            else
                next_controller_check_ = current + 500;
        }

        input::playstation_input::selection sony;
        if (playstation_ready_ && !xinput_connected_ && (playstation_only_ || controller_slot_ == 0))
            sony = playstation_.selected();
        const bool ready = xinput_connected_ || sony.ready;
        if (!ready || xinput_connected_ != using_xinput_ || sony.handle != active_playstation_)
            controller_neutral_ = false;
        using_xinput_ = xinput_connected_;
        active_playstation_ = sony.handle;
        controller_ready_ = ready;
        if (xinput_connected_)
            controller_device_name_ = "XInput slot " + std::to_string(controller_slot_ + 1);
        else
            controller_device_name_ = sony.name;
        controller_connected_ = controller_device_name_.has_value();
        controller_pressed_ = xinput_connected_ ? xinput_pressed_ : sony.pressed;
        controller_raw_ = xinput_connected_ ? xinput_raw_ : sony.raw;
    }

    void remap_engine::update_target()
    {
        bool focused = true;
        if (!pad_options_.target_process.empty())
        {
            focused = false;
            HWND window = GetForegroundWindow();
            DWORD process = 0;
            if (window)
                GetWindowThreadProcessId(window, &process);
            if (process != 0 && process == target_pid_)
            {
                focused = target_match_;
            }
            else if (process != 0)
            {
                bool match = false;
                if (HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process))
                {
                    wchar_t path[MAX_PATH]{};
                    DWORD length = MAX_PATH;
                    if (QueryFullProcessImageNameW(handle, 0, path, &length))
                    {
                        const wchar_t* name = wcsrchr(path, L'\\');
                        match = _wcsicmp(name ? name + 1 : path, pad_options_.target_process.c_str()) == 0;
                    }
                    CloseHandle(handle);
                }
                target_pid_ = process;
                target_match_ = match;
                focused = match;
            }
        }
        bool cursor = false;
        if (focused)
        {
            CURSORINFO info{};
            info.cbSize = sizeof(info);
            cursor = GetCursorInfo(&info) && (info.flags & CURSOR_SHOWING) != 0 && info.hCursor != nullptr;
        }
        if (focused != target_focused_ || cursor != menu_mouse_)
        {
            target_focused_ = focused;
            menu_mouse_ = cursor;
            aim_vx_ = aim_vy_ = 0.0;
            release();
        }
    }

    void remap_engine::update_aim()
    {
        using clock = std::chrono::steady_clock;
        const clock::time_point tick = clock::now();
        const double step = std::clamp(std::chrono::duration<double>(tick - aim_tick_).count(), 0.0002, 0.05);
        aim_tick_ = tick;
        const double idle = std::chrono::duration<double>(tick - aim_motion_).count();

        const double stop_after = std::clamp(aim_interval_ * 1.6, 0.002, 0.012);
        if (motion_x_ != 0 || motion_y_ != 0)
        {
            if (idle < 0.05)
                aim_interval_ += (std::max(idle, step) - aim_interval_) * 0.08;
            const double span = std::clamp(idle, step, stop_after);
            const double vx = motion_x_ / span;
            const double vy = motion_y_ / span;
            const double tau = 0.0008 + 0.018 * std::clamp(static_cast<double>(aim_.smoothing), 0.0, 1.0);
            const double k = 1.0 - std::exp(-span / tau);
            aim_vx_ += (vx - aim_vx_) * k;
            aim_vy_ += (vy - aim_vy_) * k;
            aim_motion_ = tick;
        }
        else if (idle > stop_after)
        {
            const double k = 1.0 - std::exp(-step / 0.0035);
            aim_vx_ -= aim_vx_ * k;
            aim_vy_ -= aim_vy_ * k;
        }

        const auto full_scale = [](float sensitivity) { return 6000.0 * std::pow(0.05, std::clamp(static_cast<double>(sensitivity), 0.0, 1.0)); };
        const double nx = aim_vx_ / full_scale(aim_.sensitivity_x);
        const double ny = -aim_vy_ / full_scale(aim_.sensitivity_y);
        const double magnitude = std::hypot(nx, ny);
        if (magnitude < 0.003)
        {
            aim_x_ = aim_y_ = 0;
            return;
        }
        const double clamped = std::min(magnitude, 1.0);
        const double position = clamped * 16.0;
        const int segment = std::min(static_cast<int>(position), 15);
        const double blend = position - segment;
        const double curved = aim_.curve[segment] + (aim_.curve[segment + 1] - aim_.curve[segment]) * blend;
        const double floor = std::clamp(static_cast<double>(aim_.anti_deadzone), 0.0, 0.6);
        const double output = std::clamp(floor + (1.0 - floor) * std::clamp(curved, 0.0, 1.0), 0.0, 1.0);
        aim_x_ = static_cast<int>(std::round(nx / magnitude * output * 32767.0));
        aim_y_ = static_cast<int>(std::round(ny / magnitude * output * 32767.0));
    }

    int remap_engine::delta(output_action positive, output_action negative) const
    {
        return (desired_.contains(positive) ? 9 : 0) - (desired_.contains(negative) ? 9 : 0);
    }

    bool remap_engine::apply()
    {
        desired_.clear();
        control_button xbox_buttons = control_button::none;
        output::xbox_motion xbox_motion;
        bool aim_left = false;
        bool aim_right = false;
        for (const auto& row : bindings_)
        {
            const bool motion_input = bindings::is_motion(row.source);
            if (motion_input)
            {
                bool routed = false;
                for (const output_action target : row.value.targets)
                {
                    if (!output::is_xbox(target))
                        continue;
                    const control_button button = output::xbox_button(target);
                    if (any(button & (control_button::left_stick_up | control_button::left_stick_down | control_button::left_stick_left | control_button::left_stick_right)))
                        aim_left = routed = true;
                    if (any(button & (control_button::right_stick_up | control_button::right_stick_down | control_button::right_stick_left | control_button::right_stick_right)))
                        aim_right = routed = true;
                }
                if (routed)
                    continue;
            }
            const int strength = motion_input ? motion_for(static_cast<output_action>(row.source.code)) : 0;
            bool pressed = false;
            if (motion_input)
                pressed = strength > 0;
            else if (row.source.kind == input_kind::controller)
                pressed = controller_neutral_ && any(controller_pressed_ & static_cast<control_button>(row.source.code));
            else
                pressed = held_.contains(row.source) || pulses_.contains(row.source);
            if (!pressed)
                continue;
            for (const output_action target : row.value.targets)
            {
                if (output::is_xbox(target))
                {
                    const control_button button = output::xbox_button(target);
                    if (!motion_input || !xbox_motion.add(button, std::min(32767, strength * 1300)))
                        xbox_buttons |= button;
                }
                else if (row.source.kind == input_kind::controller || output::code(target) != row.source.code)
                {
                    desired_.insert(target);
                }
            }
        }
        if (aim_left)
        {
            xbox_motion.left_x += aim_x_;
            xbox_motion.left_y += aim_y_;
        }
        if (aim_right)
        {
            xbox_motion.right_x += aim_x_;
            xbox_motion.right_y += aim_y_;
        }
        if (!output_.update(desired_) || (xbox_ && !xbox_->set_buttons(xbox_buttons, xbox_motion)))
            return fail("Windows or the virtual Xbox device rejected an output. Remapping stopped.");
        return true;
    }

    bool remap_engine::fail(const std::string& reason)
    {
        stop();
        error_ = reason;
        return false;
    }

    void remap_engine::drop_input_kind(input_kind kind)
    {
        const auto matches = [kind](const input_binding& source) { return source.kind == kind; };
        std::erase_if(physical_down_, matches);
        std::erase_if(swallowed_, matches);
        std::erase_if(held_, matches);
        std::erase_if(pulses_, [kind](const auto& item) { return item.first.kind == kind; });
        std::erase_if(events_, [kind](const pending_event& item) { return item.source.kind == kind; });
    }
}
