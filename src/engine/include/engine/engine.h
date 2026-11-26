#pragma once

#include "types.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace engine
{
    struct controller_state
    {
        bool connected = false;
        std::string name;
        control_button pressed = control_button::none;
        float left_x = 0;
        float left_y = 0;
        float right_x = 0;
        float right_y = 0;
        float left_trigger = 0;
        float right_trigger = 0;
        bool paddles_available = false;
    };

    struct device_counts
    {
        int keyboards = 0;
        int mice = 0;
    };

    struct status
    {
        bool active = false;
        bool starting = false;
        pad_connection pad = pad_connection::none;
        bool editor_focused = true;
        std::string error;
        bool desktop_ready = false;
        bool motion_ready = false;
        bool playstation_ready = false;
        controller_state controller;
        std::optional<device_counts> devices;
        std::optional<std::uint32_t> last_keyboard;
        std::optional<std::uint32_t> last_mouse;
    };

    enum class device_kind
    {
        keyboard,
        mouse,
        xinput,
        playstation
    };

    struct device_info
    {
        device_kind kind = device_kind::keyboard;
        std::string name;
        int slot = -1;
        bool bluetooth = false;
        bool selected = false;
    };

    struct import_result
    {
        binding_map bindings;
        int skipped = 0;
    };

    class remapper
    {
    public:
        remapper();
        ~remapper();

        remapper(const remapper&) = delete;
        remapper& operator=(const remapper&) = delete;

        bool select_xinput(int slot);
        void select_playstation();
        void set_pad_options(const pad_options& options);
        void prepare_pad();
        void set_aim(const aim_settings& settings);

        void set_bindings(const binding_map& bindings);
        binding_map bindings() const;
        bool start();
        void stop();
        void set_editor_focused(bool focused);

        status state() const;
        std::vector<device_info> devices() const;

    private:
        struct impl;
        std::unique_ptr<impl> impl_;
    };

    std::span<const controller_input_info> controller_inputs();
    std::span<const output_action> mouse_outputs();
    std::vector<output_action> xbox_outputs();

    std::string controller_name(control_button button);
    std::string input_name(const input_binding& source);
    std::string output_name(output_action action);

    input_binding controller_input(control_button button);
    input_binding keyboard_input(std::uint32_t virtual_key);
    input_binding mouse_input(output_action action);
    output_action key_output(std::uint32_t virtual_key);
    output_action xbox_output(control_button button);

    bool is_key(std::uint32_t virtual_key);
    bool is_xbox(output_action action);
    bool is_xbox_button(control_button button);
    control_button xbox_button(output_action action);
    bool is_mouse_input(output_action action);
    std::uint32_t capture_key(std::uint32_t virtual_key, std::int64_t message_flags);

    std::string serialize_output(output_action action);
    bool parse_output(std::string_view text, output_action& action);

    binding_map default_bindings();
    bool load_profile(const std::filesystem::path& path, binding_map& bindings, std::string& error);
    bool save_profile(const std::filesystem::path& path, const binding_map& bindings, std::string& error);
    bool import_rewasd(const std::filesystem::path& path, import_result& result, std::string& error);
}
