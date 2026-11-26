#pragma once

#include "../input/controller.h"
#include "../input/desktop_input.h"
#include "../input/elite_input.h"
#include "../input/playstation_input.h"
#include "../input/raw_input.h"
#include "../output/output.h"
#include "../output/virtual_duo.h"
#include "../output/virtual_pad.h"

#include <engine/types.h>

#include <windows.h>

#include <chrono>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <thread>
#include <atomic>
#include <string>
#include <vector>

namespace engine::remapping
{
    struct device_counts
    {
        int keyboards = 0;
        int mice = 0;
    };

    class remap_engine
    {
    public:
        remap_engine();
        ~remap_engine();

        remap_engine(const remap_engine&) = delete;
        remap_engine& operator=(const remap_engine&) = delete;

        void start_detection(HWND window);
        bool select_controller(int slot);
        void select_playstation_controller();
        void set_pad_options(const pad_options& options) { pad_options_ = options; }
        bool paused() const { return window_active_ || !target_focused_; }
        bool passes(const input_binding& source) const { return paused() || (menu_mouse_ && source.kind == input_kind::mouse); }
        void prepare_pad();
        void set_aim(const aim_settings& settings) { aim_ = settings; }
        bool starting() const { return pending_start_; }
        pad_connection pad_status() const;
        void start(const binding_map& profile);
        bool replace_bindings(const binding_map& profile);
        void set_editor_focused(bool focused);
        void stop();
        void process_raw_input(HRAWINPUT handle);
        void handle_raw_input_device_change(HANDLE device, bool removed);
        void poll();

        bool active() const { return active_; }
        bool editor_focused() const { return window_active_; }
        bool controller_connected() const { return controller_connected_; }
        const std::optional<std::string>& controller_device_name() const { return controller_device_name_; }
        control_button controller_pressed() const { return controller_pressed_; }
        const input::gamepad& controller_raw() const { return controller_raw_; }
        const std::optional<device_counts>& devices() const { return devices_; }
        const std::vector<input::raw_input::device_entry>& device_entries() const { return device_entries_; }
        std::optional<std::uint32_t> last_keyboard() const { return last_keyboard_; }
        std::optional<std::uint32_t> last_mouse() const { return last_mouse_; }
        const std::optional<std::string>& error() const { return error_; }
        bool desktop_ready() const { return desktop_ready_; }
        bool motion_ready() const { return motion_ready_; }
        bool playstation_ready() const { return playstation_ready_; }
        bool paddles_available() const { return elite_.available(); }
        bool playstation_only() const { return playstation_only_; }
        int controller_slot() const { return controller_slot_; }
        HANDLE active_playstation() const { return active_playstation_; }
        std::vector<input::playstation_input::device_view> playstation_devices() const { return playstation_.devices(); }

    private:
        struct pending_event
        {
            input_binding source;
            bool down = false;
        };

        long long now() const;
        void release();
        int motion_for(output_action action) const;
        bool handle_desktop(const input_binding& source, bool down);
        void poll_controller(long long now);
        int delta(output_action positive, output_action negative) const;
        bool apply();
        bool fail(const std::string& reason);
        void drop_input_kind(input_kind kind);
        static bool needs_xbox(const binding_map& profile);

        input::controller controller_;
        input::elite_input elite_;
        input::playstation_input playstation_;
        input::raw_input raw_input_;
        std::chrono::steady_clock::time_point clock_start_ = std::chrono::steady_clock::now();
        input::desktop_input desktop_;
        output::windows_output windows_output_;
        output::output_tracker output_;
        output::action_set desired_;
        std::set<input_binding> physical_down_;
        std::set<input_binding> swallowed_;
        std::deque<pending_event> events_;
        std::set<input_binding> held_;
        std::map<input_binding, long long> pulses_;
        std::set<std::uint32_t> modifiers_;
        binding_map bindings_;
        struct pad_job
        {
            std::thread worker;
            std::atomic<bool> done{ false };
            std::unique_ptr<output::virtual_pad> pad;
            std::unique_ptr<output::virtual_pad> retired;
            std::string error;
            pad_options options;
        };

        void finish_pad_job();
        bool pad_matches() const { return pad_built_.kind == pad_options_.kind && pad_built_.duo_library == pad_options_.duo_library; }
        void activate();

        void update_aim();
        void update_target();

        std::unique_ptr<output::virtual_pad> xbox_;
        pad_options pad_options_;
        aim_settings aim_;
        double aim_vx_ = 0.0;
        double aim_vy_ = 0.0;
        double aim_interval_ = 0.008;
        int aim_x_ = 0;
        int aim_y_ = 0;
        std::chrono::steady_clock::time_point aim_tick_ = std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point aim_motion_ = std::chrono::steady_clock::now();
        pad_options pad_built_;
        std::unique_ptr<pad_job> pad_job_;
        std::string pad_error_;
        bool pending_start_ = false;
        bool window_active_ = true;
        bool target_focused_ = true;
        bool menu_mouse_ = false;
        DWORD target_pid_ = 0;
        bool target_match_ = false;
        long long next_target_check_ = 0;
        bool controller_neutral_ = false;
        bool controller_eligible_ = true;
        bool playstation_only_ = false;
        bool xinput_connected_ = false;
        control_button xinput_pressed_ = control_button::none;
        input::gamepad xinput_raw_;
        bool controller_ready_ = false;
        bool using_xinput_ = false;
        HANDLE active_playstation_ = nullptr;
        bool stop_requested_ = false;
        long long next_controller_check_ = 0;
        long long next_desktop_check_ = 0;
        long long xbox_ready_deadline_ = 0;
        int controller_slot_ = 0;
        int queued_motion_x_ = 0;
        int queued_motion_y_ = 0;
        int motion_x_ = 0;
        int motion_y_ = 0;

        bool active_ = false;
        bool controller_connected_ = false;
        std::optional<std::string> controller_device_name_;
        control_button controller_pressed_ = control_button::none;
        input::gamepad controller_raw_;
        std::optional<device_counts> devices_;
        std::vector<input::raw_input::device_entry> device_entries_;
        std::optional<std::uint32_t> last_keyboard_;
        std::optional<std::uint32_t> last_mouse_;
        std::optional<std::string> error_;
        bool desktop_ready_ = false;
        bool motion_ready_ = false;
        bool playstation_ready_ = false;
    };
}
