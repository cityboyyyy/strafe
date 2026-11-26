#pragma once

#include "../render/gpu_model.h"
#include "../render/scene.h"
#include "binding_editor.h"
#include "curve_view.h"
#include "widgets/arrow_button.h"

#include "../model/emulation.h"

#include <imgui.h>

#include <engine/types.h>

#include <array>
#include <atomic>
#include <future>
#include <mutex>
#include <stop_token>
#include <thread>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace engine
{
    class remapper;
    struct status;
}

namespace menu::ui
{
    class remap_view
    {
    public:
        bool create();
        void set_engine(engine::remapper* engine) { engine_ = engine; }
        void open(std::shared_ptr<render::gpu_model> model, std::size_t catalog_index);
        void close();
        bool draw();

    private:
        struct callout
        {
            std::size_t part = 0;
            ImVec2 anchor{};
            ImVec2 badge{};
            int side = 0;
            float width = 0.0f;
            float fade = 1.0f;
            bool visible = false;
            std::string label;
        };

        void draw_header(ImVec2 origin, ImVec2 size);
        void draw_controller(ImVec2 min, ImVec2 max);
        void layout_callouts(ImVec2 image_min, ImVec2 image_size, ImVec2 area_min, ImVec2 area_max);
        void draw_callouts();
        void update_parts();
        float entrance(std::size_t part, float delay) const;
        void set_selected(int part);
        void apply_bindings(bool save);
        void toggle_emulation();
        void connect_pad();
        engine::pad_options make_pad_options() const;
        void update_toast(const engine::status& current);
        void draw_toast(ImVec2 center);
        void watch_power(std::stop_token stop);
        void set_power_error(std::string message);
        std::string power_error() const;
        void draw_editor(ImVec2 min, ImVec2 max, float appear);
        void draw_driver_prompt(ImVec2 right_center);

        render::scene scene_;
        std::shared_ptr<render::gpu_model> model_;
        std::size_t catalog_index_ = 0;
        std::vector<callout> callouts_;
        std::vector<float> glow_;
        std::vector<float> hover_glow_;
        int tab_ = 0;
        aim_profile aim_{};
        curve_view curve_;
        std::filesystem::path aim_path_;
        render::camera camera_{};
        widgets::arrow_button back_;
        int hovered_ = -1;
        int badge_hovered_ = -1;
        int selected_ = -1;
        bool dragging_ = false;
        bool back_requested_ = false;
        engine::remapper* engine_ = nullptr;
        std::vector<float> press_;
        bool driver_ready_ = false;
        bool driver_opened_ = false;
        double driver_checked_ = 0.0;
        float driver_fade_ = 1.0f;
        double driver_done_at_ = -1.0;
        bool emulating_ = false;
        std::string power_error_;
        mutable std::mutex power_lock_;
        std::mutex toggle_lock_;
        std::atomic<std::uint32_t> power_code_{ 0 };
        std::atomic<bool> capture_block_{ false };
        std::atomic<engine::virtual_pad_kind> pad_kind_{ engine::virtual_pad_kind::xbox_elite };
        engine::pad_connection pad_seen_ = engine::pad_connection::none;
        int toast_kind_ = 0;
        double toast_at_ = -10.0;
        std::string toast_name_;
        float toast_shown_ = 0.0f;
        std::jthread power_watch_;
        double opened_at_ = 0.0;
        bool turning_ = false;
        bool facing_back_ = false;
        render::camera turn_target_{};
        model::emulation_set bindings_{};
        std::filesystem::path bindings_path_;
        binding_editor editor_;
        float editor_t_ = 0.0f;
        int editor_part_ = -1;
        float wheel_pulse_[2]{};
    };
}
