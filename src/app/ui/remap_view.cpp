#include "remap_view.h"
#include "fonts.h"
#include "icons.h"
#include "paint.h"
#include "widgets/button_badge.h"
#include "widgets/callout_chip.h"
#include "widgets/install_button.h"
#include "widgets/segmented.h"
#include "widgets/knob.h"

#include "../model/buttons.h"
#include "../system/driver_installer.h"
#include "../model/catalog.h"
#include "../model/emulation.h"
#include "../model/layout.h"
#include "../render/device.h"
#include "../window/window.h"

#include <engine/engine.h>

#include <Windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <numbers>
#include <string>

namespace menu::ui
{
    namespace
    {
        constexpr render::camera default_camera{ 0.0f, 0.2f, 1.0f };
        constexpr float image_aspect = 0.61f / 0.49f;
        constexpr float render_scale = 2.0f;
        constexpr float drag_threshold = 3.0f;
        constexpr float chip_height = 30.0f;

        const ImVec4 text_primary{ 0.965f, 0.965f, 0.97f, 1.0f };
        const ImVec4 text_muted{ 0.53f, 0.53f, 0.56f, 1.0f };

        ImVec4 accent_of(std::size_t index)
        {
            const auto& a = model::catalog()[index].accent;
            return ImVec4(a.x, a.y, a.z, 1.0f);
        }

        float ease(float t)
        {
            t = std::clamp(t, 0.0f, 1.0f);
            return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
        }

        ImVec2 draw_partial(ImDrawList* draw, const ImVec2* points, float progress, ImU32 color, float thickness)
        {
            const float first = std::hypot(points[1].x - points[0].x, points[1].y - points[0].y);
            const float second = std::hypot(points[2].x - points[1].x, points[2].y - points[1].y);
            float remaining = (first + second) * std::clamp(progress, 0.0f, 1.0f);
            ImVec2 path[3] = { points[0], points[0], points[0] };
            int count = 1;
            for (int i = 0; i < 2 && remaining > 0.0f; ++i)
            {
                const float length = i == 0 ? first : second;
                const float t = length > 0.0f ? std::min(remaining / length, 1.0f) : 1.0f;
                path[count++] = ImVec2(points[i].x + (points[i + 1].x - points[i].x) * t, points[i].y + (points[i + 1].y - points[i].y) * t);
                remaining -= length;
            }
            if (count > 1)
                draw->AddPolyline(path, count, color, ImDrawFlags_None, thickness);
            return path[count - 1];
        }

        engine::control_button engine_button(model::button which)
        {
            using model::button;
            using engine::control_button;
            switch (which)
            {
            case button::south: return control_button::a;
            case button::east: return control_button::b;
            case button::west: return control_button::x;
            case button::north: return control_button::y;
            case button::left_bumper: return control_button::lb;
            case button::right_bumper: return control_button::rb;
            case button::left_trigger: return control_button::lt;
            case button::right_trigger: return control_button::rt;
            case button::left_stick: return control_button::left_stick_press;
            case button::right_stick: return control_button::right_stick_press;
            case button::dpad_up: return control_button::dpad_up;
            case button::dpad_down: return control_button::dpad_down;
            case button::dpad_left: return control_button::dpad_left;
            case button::dpad_right: return control_button::dpad_right;
            case button::view: return control_button::back;
            case button::menu: return control_button::start;
            case button::guide: return control_button::guide;
            case button::touchpad: return control_button::touchpad;
            case button::mute: return control_button::mute;
            case button::paddle_1: return control_button::paddle_1;
            case button::paddle_2: return control_button::paddle_2;
            case button::paddle_3: return control_button::paddle_3;
            case button::paddle_4: return control_button::paddle_4;
            default: return control_button::none;
            }
        }

        bool is_back(model::button which)
        {
            return which >= model::button::paddle_1 && which <= model::button::paddle_4;
        }

        engine::input_binding source_of(std::uint32_t code)
        {
            using engine::output_action;
            switch (code)
            {
            case VK_LBUTTON: return engine::mouse_input(output_action::mouse_left);
            case VK_RBUTTON: return engine::mouse_input(output_action::mouse_right);
            case VK_MBUTTON: return engine::mouse_input(output_action::mouse_middle);
            case VK_XBUTTON1: return engine::mouse_input(output_action::mouse_back);
            case VK_XBUTTON2: return engine::mouse_input(output_action::mouse_forward);
            case model::wheel_up_code: return engine::mouse_input(output_action::wheel_up);
            case model::wheel_down_code: return engine::mouse_input(output_action::wheel_down);
            default: return engine::keyboard_input(code);
            }
        }

        engine::binding_map build_bindings(const model::emulation_set& set)
        {
            using engine::control_button;
            using engine::output_action;
            engine::binding_map map;
            const auto bind = [&](const engine::input_binding& source, control_button target)
            {
                const output_action output = engine::xbox_output(target);
                if (output != output_action::none)
                    map.set(source, engine::binding{ { output }, true });
            };

            for (std::size_t i = 0; i < set.size(); ++i)
            {
                const auto which = static_cast<model::button>(i);
                if (which == model::button::dpad || which == model::button::guide || set[i].press == 0)
                    continue;
                bind(source_of(set[i].press), engine_button(model::needs_mirror(which) ? set[i].mirror : which));
            }

            constexpr control_button left[] = { control_button::left_stick_up, control_button::left_stick_down, control_button::left_stick_left, control_button::left_stick_right };
            constexpr control_button right[] = { control_button::right_stick_up, control_button::right_stick_down, control_button::right_stick_left, control_button::right_stick_right };
            constexpr output_action motion[] = { output_action::move_up, output_action::move_down, output_action::move_left, output_action::move_right };
            for (int stick = 0; stick < 2; ++stick)
            {
                const auto& entry = set[static_cast<std::size_t>(stick == 0 ? model::button::left_stick : model::button::right_stick)];
                const control_button* targets = stick == 0 ? left : right;
                for (std::size_t d = 0; d < 4; ++d)
                {
                    if (entry.mouse_motion)
                        bind(engine::mouse_input(motion[d]), targets[d]);
                    else if (entry.move[d] != 0)
                        bind(source_of(entry.move[d]), targets[d]);
                }
            }
            return map;
        }

        void text(ImDrawList* draw, ImFont* font, float size, ImVec2 position, ImU32 color, const char* value)
        {
            ImGui::PushFont(font, size);
            draw->AddText(ImVec2(std::round(position.x), std::round(position.y)), color, value);
            ImGui::PopFont();
        }

    }

    bool remap_view::create()
    {
        if (!power_watch_.joinable())
            power_watch_ = std::jthread([this](std::stop_token stop) { watch_power(stop); });
        return scene_.create(render::device());
    }

    void remap_view::open(std::shared_ptr<render::gpu_model> model, std::size_t catalog_index)
    {
        model_ = std::move(model);
        catalog_index_ = catalog_index;
        camera_ = default_camera;
        opened_at_ = ImGui::GetTime();
        turning_ = false;
        facing_back_ = false;
        hovered_ = -1;
        badge_hovered_ = -1;
        selected_ = -1;
        dragging_ = false;
        callouts_.clear();
        glow_.clear();
        press_.clear();
        hover_glow_.clear();
        if (!model_)
            return;

        callouts_.resize(model_->part_count());
        for (std::size_t i = 0; i < callouts_.size(); ++i)
            callouts_[i].part = i;
        glow_.assign(model_->part_count(), 0.0f);
        press_.assign(model_->part_count(), 0.0f);
        driver_opened_ = false;
        driver_checked_ = ImGui::GetTime();
        hover_glow_.assign(model_->part_count(), 0.0f);
        editor_.stop();
        editor_t_ = 0.0f;
        set_power_error({});
        editor_part_ = -1;
        bindings_path_ = model::emulation_path(model::catalog()[catalog_index_].file);
        bindings_ = model::default_emulation();
        model::load_emulation(bindings_path_, bindings_);
        apply_bindings(false);
        aim_path_ = std::filesystem::path(bindings_path_).replace_extension(L".aim");
        aim_ = aim_profile{};
        load_aim(aim_path_, aim_);
        if (engine_)
            engine_->set_aim(to_engine(aim_));
        tab_ = 0;
        const bool playstation = model::layout_family(model::catalog()[catalog_index_].file) == model::family::playstation;
        pad_kind_.store(playstation ? engine::virtual_pad_kind::dualsense : engine::virtual_pad_kind::xbox_elite);
        system::driver::reset();
        driver_done_at_ = -1.0;
        driver_ready_ = system::driver::installed();
        connect_pad();
        driver_fade_ = driver_ready_ ? 0.0f : 1.0f;
    }

    engine::pad_options remap_view::make_pad_options() const
    {
        engine::pad_options options;
        options.kind = pad_kind_.load();
        options.duo_library = system::driver::library();
        return options;
    }

    void remap_view::set_power_error(std::string message)
    {
        std::lock_guard lock(power_lock_);
        power_error_ = std::move(message);
    }

    std::string remap_view::power_error() const
    {
        std::lock_guard lock(power_lock_);
        return power_error_;
    }

    void remap_view::toggle_emulation()
    {
        if (!engine_)
            return;
        std::lock_guard toggling(toggle_lock_);
        if (engine_->state().active)
        {
            engine_->stop();
            set_power_error({});
            return;
        }
        if (power_code_.load() == 0)
        {
            set_power_error("Bind a power key first so you can turn it off in game.");
            return;
        }
        engine_->set_pad_options(make_pad_options());
        const bool started = engine_->start();
        std::string error = started ? std::string() : engine_->state().error;
        if (error.empty() && !engine_->state().active)
            error = "Could not start the virtual controller.";
        set_power_error(std::move(error));
    }

    void remap_view::watch_power(std::stop_token stop)
    {
        std::uint32_t last_code = 0;
        bool was_down = false;
        while (!stop.stop_requested())
        {
            const std::uint32_t code = power_code_.load();
            const bool down = code != 0 && model::is_held(code);
            if (code != last_code)
            {
                last_code = code;
                was_down = down;
            }
            else
            {
                if (down && !was_down && !capture_block_.load())
                    toggle_emulation();
                was_down = down;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }
    }

    void remap_view::apply_bindings(bool save)
    {
        if (save)
            model::save_emulation(bindings_path_, bindings_);
        power_code_.store(bindings_[static_cast<std::size_t>(model::button::guide)].press);
        if (engine_)
            engine_->set_bindings(build_bindings(bindings_));
    }

    void remap_view::close()
    {
        if (!model_)
            return;
        editor_.stop();
        for (std::size_t i = 0; i < model_->part_count(); ++i)
            model_->set_part(render::context(), i, { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f });
        model_.reset();
    }

    float remap_view::entrance(std::size_t part, float delay) const
    {
        const float elapsed = static_cast<float>(ImGui::GetTime() - opened_at_);
        return ease((elapsed - delay - static_cast<float>(part) * 0.03f) / 0.34f);
    }

    void remap_view::set_selected(int part)
    {
        selected_ = part;
        const bool back = part >= 0 && is_back(model_->part_button(static_cast<std::size_t>(part)));
        if (back && !facing_back_)
        {
            turn_target_ = render::camera{ std::numbers::pi_v<float>, 0.28f, camera_.zoom };
            turning_ = true;
            facing_back_ = true;
        }
        else if (!back && part >= 0 && facing_back_)
        {
            turn_target_ = default_camera;
            turning_ = true;
            facing_back_ = false;
        }
    }

    void remap_view::update_parts()
    {
        const ImVec4 accent = accent_of(catalog_index_);
        const float dt = ImGui::GetIO().DeltaTime;
        const float blend = 1.0f - std::exp(-14.0f * dt);
        const float follow = 1.0f - std::exp(-28.0f * dt);
        const float wheel = ImGui::GetIO().MouseWheel;
        if (wheel > 0.0f)
            wheel_pulse_[0] = 1.0f;
        else if (wheel < 0.0f)
            wheel_pulse_[1] = 1.0f;
        for (float& pulse : wheel_pulse_)
            pulse = std::max(pulse - dt * 5.0f, 0.0f);
        const auto held = [&](std::uint32_t key)
        {
            if (key == model::wheel_up_code)
                return wheel_pulse_[0] > 0.0f;
            if (key == model::wheel_down_code)
                return wheel_pulse_[1] > 0.0f;
            return model::is_held(key);
        };

        for (std::size_t i = 0; i < glow_.size(); ++i)
        {
            const int part = static_cast<int>(i);
            const model::button which = model_->part_button(i);
            DirectX::XMFLOAT3 offset{ 0.0f, 0.0f, 0.0f };
            float pressed = 0.0f;

            if (which != model::button::count)
            {
                const auto direction = [&](int index) { return held(model::slot(bindings_, which, index)) ? 1.0f : 0.0f; };
                const bool click = held(model::slot(bindings_, which, model::press_slot));
                const bool steer = direction(0) + direction(1) + direction(2) + direction(3) > 0.0f;

                switch (which)
                {
                case model::button::left_trigger:
                case model::button::right_trigger:
                    pressed = click ? 1.0f : 0.0f;
                    offset = { 0.0f, -0.016f * pressed, 0.012f * pressed };
                    break;
                case model::button::left_stick:
                case model::button::right_stick:
                case model::button::dpad:
                    pressed = click || steer ? 1.0f : 0.0f;
                    break;
                case model::button::left_bumper:
                case model::button::right_bumper:
                    pressed = click ? 1.0f : 0.0f;
                    offset = { 0.0f, -0.01f * pressed, 0.0f };
                    break;
                default:
                    pressed = click ? 1.0f : 0.0f;
                    offset = { 0.0f, 0.0f, -0.012f * pressed };
                    break;
                }
            }

            press_[i] += (pressed - press_[i]) * follow;
            const bool lit = part == hovered_ || part == badge_hovered_;
            const bool power = which == model::button::guide && emulating_;
            const float breathe = 0.5f + 0.18f * std::sin(static_cast<float>(ImGui::GetTime()) * 2.6f);
            const float target = part == selected_ ? 1.0f : power ? std::max(breathe, lit ? 0.75f : 0.0f) : lit ? 0.6f : 0.0f;
            glow_[i] += (target - glow_[i]) * blend;
            hover_glow_[i] += ((lit ? 1.0f : 0.0f) - hover_glow_[i]) * blend;
            const float shown = std::max(glow_[i], press_[i] * 0.85f);
            model_->set_part(render::context(), i, { accent.x, accent.y, accent.z, shown }, offset);
        }
    }

    void remap_view::draw_header(ImVec2 origin, ImVec2 size)
    {
        const float s = ImGui::GetStyle().FontScaleDpi;
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float height = 34.0f * s;
        const float back_width = widgets::arrow_button::width(height);
        const ImVec2 back_center{ origin.x + 28.0f * s + back_width * 0.5f, origin.y + 34.0f * s };
        if (back_.draw("##back", back_center, height, -1, accent_of(catalog_index_)))
            back_requested_ = true;
        const ImVec2 viewport_origin = ImGui::GetMainViewport()->Pos;
        window::add_drag_hole({ back_center.x - back_width * 0.5f - 6.0f * s - viewport_origin.x, back_center.y - height * 0.5f - 6.0f * s - viewport_origin.y, back_width + 12.0f * s, height + 12.0f * s });

        const float text_x = back_center.x + back_width * 0.5f + 16.0f * s;
        ImGui::PushFont(fonts::get(fonts::role::strong), 22.0f);
        const float title_height = ImGui::GetFontSize();
        ImGui::PopFont();
        text(draw, fonts::get(fonts::role::strong), 22.0f, ImVec2(text_x, back_center.y - title_height * 0.5f), paint::to_u32(text_primary), "Bindings");

        constexpr const char* tabs[] = { "Controller", "Curvature", "Scripts" };
        const float tabs_width = widgets::segmented_width(tabs, 3);
        const ImVec2 tabs_center{ origin.x + size.x * 0.5f, back_center.y };
        window::add_drag_hole({ tabs_center.x - tabs_width * 0.5f - 4.0f * s - viewport_origin.x, tabs_center.y - 21.0f * s - viewport_origin.y, tabs_width + 8.0f * s, 42.0f * s });
        if (widgets::segmented("##tabs", tabs, 3, tab_, tabs_center) && tab_ != 0)
            set_selected(-1);

        draw_driver_prompt(ImVec2(origin.x + size.x - 24.0f * s, origin.y + 34.0f * s));
    }

    void remap_view::draw_driver_prompt(ImVec2 right_center)
    {
        namespace driver = system::driver;
        const double now = ImGui::GetTime();
        const driver::status install = driver::state();
        const bool working = driver::busy();
        if (!driver_ready_ && !working && now - driver_checked_ > (driver_opened_ ? 1.0 : 4.0))
        {
            driver_checked_ = now;
            driver_ready_ = driver::installed();
        }
        if (install.current == driver::stage::done && driver_done_at_ < 0.0)
        {
            driver_done_at_ = now;
            driver_ready_ = driver::installed();
            connect_pad();
        }

        widgets::install_phase phase = widgets::install_phase::idle;
        float progress = 0.0f;
        switch (install.current)
        {
        case driver::stage::extracting:
            phase = widgets::install_phase::working;
            progress = 0.25f;
            break;
        case driver::stage::installing:
            phase = widgets::install_phase::working;
            progress = 0.75f;
            break;
        case driver::stage::done:
            phase = widgets::install_phase::done;
            progress = 1.0f;
            break;
        case driver::stage::failed:
            phase = widgets::install_phase::failed;
            break;
        default:
            break;
        }

        const bool celebrating = driver_done_at_ >= 0.0 && now - driver_done_at_ < 1.4;
        const bool visible = !driver_ready_ || working || celebrating;
        const float dt = ImGui::GetIO().DeltaTime;
        driver_fade_ += ((visible ? 1.0f : 0.0f) - driver_fade_) * (1.0f - std::exp(-7.0f * dt));
        if (driver_fade_ < 0.01f)
            return;

        const float s = ImGui::GetStyle().FontScaleDpi;
        const ImVec2 viewport_origin = ImGui::GetMainViewport()->Pos;
        window::add_drag_hole({ right_center.x - 260.0f * s - viewport_origin.x, right_center.y - 22.0f * s - viewport_origin.y, 264.0f * s, 44.0f * s });
        if (widgets::install_button("##driver", right_center, driver_fade_, phase, progress, accent_of(catalog_index_)))
        {
            driver_done_at_ = -1.0;
            driver::install();
            driver_opened_ = true;
            driver_checked_ = now;
        }
    }


    void remap_view::layout_callouts(ImVec2 image_min, ImVec2 image_size, ImVec2 area_min, ImVec2 area_max)
    {
        const float s = ImGui::GetStyle().FontScaleDpi;
        const auto family = model::layout_family(model::catalog()[catalog_index_].file);
        const auto to_screen = [&](const DirectX::XMFLOAT3& point, ImVec2& out)
        {
            DirectX::XMFLOAT2 uv;
            if (!scene_.project(point, uv))
                return false;
            out = ImVec2(image_min.x + uv.x * image_size.x, image_min.y + uv.y * image_size.y);
            return true;
        };

        const auto& low = model_->display_low();
        const auto& high = model_->display_high();
        ImVec2 sil_min{ FLT_MAX, FLT_MAX };
        ImVec2 sil_max{ -FLT_MAX, -FLT_MAX };
        for (int corner = 0; corner < 8; ++corner)
        {
            const DirectX::XMFLOAT3 point{ corner & 1 ? high.x : low.x, corner & 2 ? high.y : low.y, corner & 4 ? high.z : low.z };
            ImVec2 p;
            if (!to_screen(point, p))
                continue;
            sil_min = ImVec2(std::min(sil_min.x, p.x), std::min(sil_min.y, p.y));
            sil_max = ImVec2(std::max(sil_max.x, p.x), std::max(sil_max.y, p.y));
        }
        const float shrink_x = (sil_max.x - sil_min.x) * 0.05f;
        const float shrink_y = (sil_max.y - sil_min.y) * 0.08f;
        sil_min = ImVec2(sil_min.x + shrink_x, sil_min.y + shrink_y);
        sil_max = ImVec2(sil_max.x - shrink_x, sil_max.y - shrink_y);

        const float height = chip_height * s;
        const float radius = height * 0.5f;
        const float gap = 34.0f * s;
        const float row = height + 9.0f * s;

        struct group
        {
            std::vector<callout*> items;
            float anchor_sum = 0.0f;
        };
        group columns[4];
        const float front = std::clamp(std::cos(camera_.yaw) * 2.5f + 0.5f, 0.0f, 1.0f);

        for (auto& c : callouts_)
        {
            const model::button which = model_->part_button(c.part);
            const bool shoulder = which == model::button::left_bumper || which == model::button::right_bumper || which == model::button::left_trigger || which == model::button::right_trigger;
            c.fade = is_back(which) ? 1.0f - front : shoulder ? 1.0f : front;
            c.visible = c.fade > 0.02f && to_screen(model_->part_center(c.part), c.anchor);
            if (!c.visible)
                continue;
            const bool listening = editor_.capturing() && static_cast<int>(c.part) == selected_;
            if (listening)
                c.label = "Press a key";
            else if (which == model::button::guide)
                c.label = power_code_.load() == 0 ? std::string("Set power key") : model::input_name(power_code_.load()) + (emulating_ ? "  ·  On" : "  ·  Off");
            else
                c.label = model::describe(bindings_, which);
            if (c.label.empty())
                c.label = "Set key";
            c.width = widgets::callout_chip_width(which, family, c.label.c_str(), height);

            const float u = (c.anchor.x - sil_min.x) / std::max(sil_max.x - sil_min.x, 1.0f);
            const float v = (c.anchor.y - sil_min.y) / std::max(sil_max.y - sil_min.y, 1.0f);
            const bool center_band = std::abs(u - 0.5f) < 0.18f;
            if (center_band && (v < 0.32f || v > 0.42f))
            {
                c.side = v < 0.32f ? 2 : 3;
                c.badge = ImVec2(c.anchor.x, c.side == 2 ? sil_min.y - gap * 0.75f - radius : sil_max.y + gap * 0.75f + radius);
                columns[c.side].items.push_back(&c);
                columns[c.side].anchor_sum += c.anchor.x;
            }
            else
            {
                c.side = u < 0.5f ? 0 : 1;
                c.badge = ImVec2(c.side == 0 ? sil_min.x - gap - radius : sil_max.x + gap + radius, c.anchor.y);
                columns[c.side].items.push_back(&c);
                columns[c.side].anchor_sum += c.anchor.y;
            }
        }

        for (int side = 0; side < 2; ++side)
        {
            auto& items = columns[side].items;
            if (items.empty())
                continue;
            std::sort(items.begin(), items.end(), [](const callout* a, const callout* b) { return a->anchor.y < b->anchor.y; });
            const float span = row * static_cast<float>(items.size() - 1);
            const float mean = columns[side].anchor_sum / static_cast<float>(items.size());
            float first = std::clamp(mean - span * 0.5f, area_min.y + radius, std::max(area_min.y + radius, area_max.y - radius - span));
            for (std::size_t i = 0; i < items.size(); ++i)
            {
                callout& c = *items[i];
                c.badge.y = first + row * static_cast<float>(i);
                if (side == 0)
                    c.badge.x = std::max(c.badge.x, area_min.x + c.width - radius);
                else
                    c.badge.x = std::min(c.badge.x, area_max.x - c.width + radius);
            }
        }

        for (int side = 2; side < 4; ++side)
        {
            auto& items = columns[side].items;
            if (items.empty())
                continue;
            std::sort(items.begin(), items.end(), [](const callout* a, const callout* b) { return a->anchor.x < b->anchor.x; });
            const float spacing = 10.0f * s;
            float total = -spacing;
            for (const callout* c : items)
                total += c->width + spacing;
            const float mean = columns[side].anchor_sum / static_cast<float>(items.size());
            float x = std::clamp(mean - total * 0.5f, area_min.x, std::max(area_min.x, area_max.x - total));
            for (callout* c : items)
            {
                c->badge.x = x + radius;
                x += c->width + spacing;
            }
        }
    }

    void remap_view::draw_callouts()
    {
        const float s = ImGui::GetStyle().FontScaleDpi;
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec4 accent = accent_of(catalog_index_);
        const auto family = model::layout_family(model::catalog()[catalog_index_].file);
        const float height = chip_height * s;

        std::vector<ImVec2> ports(callouts_.size());
        badge_hovered_ = -1;
        for (const auto& c : callouts_)
        {
            if (!c.visible)
                continue;
            ImGui::PushID(static_cast<int>(c.part));
            const model::button which = model_->part_button(c.part);
            const bool power = which == model::button::guide;
            const float select = static_cast<int>(c.part) == selected_ || (power && emulating_) ? 1.0f : 0.0f;
            widgets::chip_style look;
            look.hover = hover_glow_[c.part];
            look.select = select;
            look.bound = power ? power_code_.load() != 0 : model::is_bound(bindings_, which);
            look.appear = entrance(c.part, 0.18f) * c.fade;
            const auto result = widgets::callout_chip("##chip", c.badge, static_cast<widgets::chip_side>(c.side), height, which, family,
                c.label.c_str(), look, accent);
            ImGui::PopID();
            ports[c.part] = result.port;
            if (result.hovered)
                badge_hovered_ = static_cast<int>(c.part);
            if (result.clicked)
                set_selected(selected_ == static_cast<int>(c.part) ? -1 : static_cast<int>(c.part));
        }

        for (const auto& c : callouts_)
        {
            if (!c.visible)
                continue;
            const float lit = std::max(hover_glow_[c.part], static_cast<int>(c.part) == selected_ || (emulating_ && model_->part_button(c.part) == model::button::guide) ? 1.0f : 0.0f);
            const ImU32 color = paint::to_u32(paint::mix(ImVec4(1.0f, 1.0f, 1.0f, 0.16f), ImVec4(accent.x, accent.y, accent.z, 0.95f), lit), c.fade);
            const ImVec2 start = ports[c.part];

            ImVec2 elbow;
            if (c.side <= 1)
            {
                const float direction = c.side == 0 ? 1.0f : -1.0f;
                const float reach = std::max(std::abs(c.anchor.x - start.x) - std::abs(c.anchor.y - start.y), 10.0f * s);
                elbow = ImVec2(start.x + direction * std::min(reach, std::abs(c.anchor.x - start.x)), start.y);
            }
            else
            {
                const float direction = c.side == 2 ? 1.0f : -1.0f;
                const float reach = std::max(std::abs(c.anchor.y - start.y) - std::abs(c.anchor.x - start.x), 8.0f * s);
                elbow = ImVec2(start.x, start.y + direction * std::min(reach, std::abs(c.anchor.y - start.y)));
            }

            const float grow = entrance(c.part, 0.32f);
            if (grow <= 0.001f)
                continue;
            const ImVec2 points[] = { start, elbow, c.anchor };
            const ImVec2 end = draw_partial(draw, points, grow, color, (1.0f + 0.4f * lit) * s);
            if (grow < 0.999f)
                continue;
            draw->AddCircleFilled(end, (2.2f + 1.0f * lit) * s, color, 16);
            if (lit > 0.01f)
                draw->AddCircle(c.anchor, 6.0f * s, paint::to_u32(ImVec4(accent.x, accent.y, accent.z, 0.5f * lit)), 24, 1.0f * s);
            if (static_cast<int>(c.part) == selected_)
            {
                const float wave = static_cast<float>(std::fmod(ImGui::GetTime() * 0.9, 1.0));
                draw->AddCircle(c.anchor, (6.0f + 14.0f * wave) * s, paint::to_u32(ImVec4(accent.x, accent.y, accent.z, 0.55f * (1.0f - wave))), 32, 1.2f * s);
            }
        }
    }

    void remap_view::draw_controller(ImVec2 min, ImVec2 max)
    {
        const float s = ImGui::GetStyle().FontScaleDpi;
        const ImGuiIO& io = ImGui::GetIO();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec2 area{ max.x - min.x, max.y - min.y };
        const float width = std::min(area.x * 0.98f, area.y * 1.16f * image_aspect);
        const ImVec2 extent{ width, width / image_aspect };
        const ImVec2 top_left{ min.x + (area.x - extent.x) * 0.5f, min.y + (area.y - extent.y) * 0.5f };
        const ImVec2 bottom_right{ top_left.x + extent.x, top_left.y + extent.y };

        ImGui::SetCursorScreenPos(top_left);
        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton("##controller", extent, ImGuiButtonFlags_MouseButtonLeft);
        const bool hovered = ImGui::IsItemHovered();

        if (ImGui::IsItemActivated())
            dragging_ = false;
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, drag_threshold * s))
        {
            dragging_ = true;
            turning_ = false;
            camera_.yaw = std::remainder(camera_.yaw - io.MouseDelta.x * 0.008f, 2.0f * std::numbers::pi_v<float>);
            camera_.pitch = std::clamp(camera_.pitch + io.MouseDelta.y * 0.008f, -1.2f, 1.2f);
        }
        if (ImGui::IsItemDeactivated() && !dragging_)
            set_selected(hovered_);
        if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            camera_ = default_camera;
            turning_ = false;
            facing_back_ = false;
        }

        if (turning_)
        {
            const float blend = 1.0f - std::exp(-7.0f * io.DeltaTime);
            const float yaw = std::remainder(turn_target_.yaw - camera_.yaw, 2.0f * std::numbers::pi_v<float>);
            camera_.yaw = std::remainder(camera_.yaw + yaw * blend, 2.0f * std::numbers::pi_v<float>);
            camera_.pitch += (turn_target_.pitch - camera_.pitch) * blend;
            camera_.zoom += (turn_target_.zoom - camera_.zoom) * blend;
            if (std::abs(yaw) < 0.002f && std::abs(turn_target_.pitch - camera_.pitch) < 0.002f)
                turning_ = false;
        }

        update_parts();

        const auto render_width = static_cast<std::uint32_t>(std::ceil(extent.x * render_scale));
        const auto render_height = static_cast<std::uint32_t>(std::ceil(extent.y * render_scale));
        const auto& entry = model::catalog()[catalog_index_];
        ID3D11ShaderResourceView* image = scene_.render(render::context(), *model_, entry.rotation, camera_,
            render::lighting{ entry.exposure, entry.ambient, entry.neutral_tone, entry.key_front }, render_width, render_height);
        if (!image)
            return;

        const float reveal = ease(static_cast<float>(ImGui::GetTime() - opened_at_) / 0.45f);
        const float zoom = 0.94f + 0.06f * reveal;
        const ImVec2 middle{ (top_left.x + bottom_right.x) * 0.5f, (top_left.y + bottom_right.y) * 0.5f };
        const ImVec2 half{ extent.x * 0.5f * zoom, extent.y * 0.5f * zoom };
        paint::render_image(draw, image, ImVec2(middle.x - half.x, middle.y - half.y), ImVec2(middle.x + half.x, middle.y + half.y), paint::rgba(1.0f, 1.0f, 1.0f, reveal));

        if (hovered && !ImGui::IsItemActive())
        {
            const float px = (io.MousePos.x - top_left.x) / extent.x * static_cast<float>(render_width);
            const float py = (io.MousePos.y - top_left.y) / extent.y * static_cast<float>(render_height);
            scene_.pick(render::context(), *model_, px, py);
            hovered_ = scene_.picked(render::context());
        }
        else if (!ImGui::IsItemActive())
        {
            hovered_ = -1;
        }

        if (hovered_ >= 0 && hovered && !dragging_)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        layout_callouts(top_left, extent, min, max);
        draw_callouts();
    }



    bool remap_view::draw()
    {
        if (!model_)
            return true;

        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 size = ImGui::GetContentRegionAvail();
        const float s = ImGui::GetStyle().FontScaleDpi;
        back_requested_ = false;
        const engine::status current = engine_ ? engine_->state() : engine::status{};
        emulating_ = current.active;
        update_toast(current);
        capture_block_.store(editor_.capturing());

        draw_header(origin, size);
        if (selected_ >= 0)
            editor_part_ = selected_;
        const float target = selected_ >= 0 ? 1.0f : 0.0f;
        editor_t_ += (target - editor_t_) * (1.0f - std::exp(-12.0f * ImGui::GetIO().DeltaTime));
        if (selected_ < 0 && editor_t_ < 0.02f)
        {
            editor_t_ = 0.0f;
            editor_part_ = -1;
            editor_.stop();
        }
        const float open = ease(editor_t_);

        const float right = origin.x + size.x - 24.0f * s;
        const float content_top = origin.y + 78.0f * s;
        const float content_bottom = origin.y + size.y - 24.0f * s;
        float toast_center = (origin.x + size.x * 0.5f);
        if (tab_ == 0)
        {
            const float top = origin.y + 80.0f * s;
            const float bottom = origin.y + size.y - 70.0f * s;
            const float editor_left = right - 300.0f * s;
            const float column_left = right + 18.0f * s + (editor_left - right - 18.0f * s) * open;
            draw_controller(ImVec2(origin.x + 20.0f * s, top), ImVec2(column_left - 18.0f * s, bottom));
            if (editor_part_ >= 0)
                draw_editor(ImVec2(editor_left, content_top), ImVec2(right, content_bottom), open);
            toast_center = (origin.x + 20.0f * s + column_left - 18.0f * s) * 0.5f;
        }
        else if (tab_ == 1)
        {
            if (curve_.draw(ImVec2(origin.x + 28.0f * s, content_top), ImVec2(right, content_bottom), aim_, accent_of(catalog_index_)))
            {
                save_aim(aim_path_, aim_);
                if (engine_)
                    engine_->set_aim(to_engine(aim_));
            }
        }
        else
        {
            draw_scripts(ImVec2(origin.x + 28.0f * s, content_top), ImVec2(right, content_bottom));
        }
        draw_toast(ImVec2(toast_center, origin.y + size.y - 34.0f * s));
        return back_requested_;
    }

    void remap_view::draw_scripts(ImVec2 min, ImVec2 max)
    {
        const float s = ImGui::GetStyle().FontScaleDpi;
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float corner = 18.0f * s;
        draw->AddRectFilled(min, max, paint::rgba(0.05f, 0.05f, 0.058f, 0.84f), corner);
        draw->AddRect(min, max, paint::rgba(1.0f, 1.0f, 1.0f, 0.075f), corner, 0, 1.0f);
        const ImVec2 center{ (min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f };
        const float arm = 12.0f * s;
        const ImU32 glyph = paint::rgba(1.0f, 1.0f, 1.0f, 0.22f);
        const ImVec2 left[] = { ImVec2(center.x - arm * 1.2f, center.y - 24.0f * s - arm), ImVec2(center.x - arm * 2.2f, center.y - 24.0f * s), ImVec2(center.x - arm * 1.2f, center.y - 24.0f * s + arm) };
        const ImVec2 right[] = { ImVec2(center.x + arm * 1.2f, center.y - 24.0f * s - arm), ImVec2(center.x + arm * 2.2f, center.y - 24.0f * s), ImVec2(center.x + arm * 1.2f, center.y - 24.0f * s + arm) };
        paint::round_polyline(draw, left, 3, glyph, 2.2f * s);
        paint::round_polyline(draw, right, 3, glyph, 2.2f * s);
        const ImVec2 slash[] = { ImVec2(center.x + arm * 0.4f, center.y - 24.0f * s - arm * 1.1f), ImVec2(center.x - arm * 0.4f, center.y - 24.0f * s + arm * 1.1f) };
        paint::round_polyline(draw, slash, 2, glyph, 2.2f * s);
        ImGui::PushFont(nullptr, 13.0f);
        const char* label = "Scripts are coming next";
        const ImVec2 extent = ImGui::CalcTextSize(label);
        draw->AddText(ImVec2(std::round(center.x - extent.x * 0.5f), std::round(center.y + 10.0f * s)), paint::rgba(0.5f, 0.5f, 0.54f, 1.0f), label);
        ImGui::PopFont();
    }

    void remap_view::connect_pad()
    {
        if (!engine_)
            return;
        engine_->set_pad_options(make_pad_options());
        if (driver_ready_)
            engine_->prepare_pad();
    }

    void remap_view::update_toast(const engine::status& current)
    {
        const double now = ImGui::GetTime();
        if (current.pad != pad_seen_)
        {
            pad_seen_ = current.pad;
            if (current.pad == engine::pad_connection::connected)
            {
                const engine::virtual_pad_kind kind = pad_kind_.load();
                toast_name_ = kind == engine::virtual_pad_kind::dualsense ? "DualSense Wireless Controller" : "Xbox Elite Wireless Controller";
                toast_kind_ = 2;
                toast_at_ = now;
            }
            else if (current.pad == engine::pad_connection::connecting)
            {
                toast_kind_ = 1;
                toast_at_ = now;
            }
            else if (current.pad == engine::pad_connection::failed)
            {
                toast_kind_ = 3;
                toast_at_ = now;
            }
            else if (current.pad == engine::pad_connection::none)
            {
                toast_kind_ = 0;
            }
        }
    }

    void remap_view::draw_toast(ImVec2 center)
    {
        const double now = ImGui::GetTime();
        const float age = static_cast<float>(now - toast_at_);
        float target = 0.0f;
        if (toast_kind_ == 1)
            target = 1.0f;
        else if (toast_kind_ == 2)
            target = age < 3.2f ? 1.0f : 0.0f;
        else if (toast_kind_ == 3)
            target = age < 5.0f ? 1.0f : 0.0f;
        const float dt = ImGui::GetIO().DeltaTime;
        toast_shown_ += (target - toast_shown_) * (1.0f - std::exp(-(target > toast_shown_ ? 10.0f : 6.0f) * dt));
        if (toast_shown_ < 0.01f || toast_kind_ == 0)
            return;

        const float s = ImGui::GetStyle().FontScaleDpi;
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec4 accent = accent_of(catalog_index_);
        const auto family = model::layout_family(model::catalog()[catalog_index_].file);
        const float alpha = toast_shown_;
        const float lift = (1.0f - ease(toast_shown_)) * 12.0f * s;

        const char* title = toast_kind_ == 1 ? "Connecting" : toast_kind_ == 3 ? "Could not connect" : toast_name_.c_str();
        const char* note = toast_kind_ == 2 ? "Connected" : nullptr;
        ImGui::PushFont(fonts::get(fonts::role::strong), 13.0f);
        const ImVec2 title_size = ImGui::CalcTextSize(title);
        ImGui::PopFont();
        ImGui::PushFont(nullptr, 12.0f);
        const ImVec2 note_size = note ? ImGui::CalcTextSize(note) : ImVec2(0.0f, 0.0f);
        ImGui::PopFont();

        const float height = 40.0f * s;
        const float icon_slot = 34.0f * s;
        const float width = 12.0f * s + icon_slot + title_size.x + (note ? 10.0f * s + note_size.x : 0.0f) + 18.0f * s;
        const ImVec2 min{ std::round(center.x - width * 0.5f), std::round(center.y - height * 0.5f + lift) };
        const ImVec2 max{ min.x + width, min.y + height };
        const float radius = height * 0.5f;

        paint::soft_ellipse(draw, ImVec2(center.x, max.y + 2.0f * s), ImVec2(width * 0.58f, height * 0.7f), ImVec4(0.0f, 0.0f, 0.0f, 0.45f * alpha), 8, 40);
        draw->AddRectFilled(min, max, paint::rgba(0.075f, 0.075f, 0.085f, 0.94f * alpha), radius);
        const ImVec4 edge = toast_kind_ == 3 ? ImVec4(1.0f, 0.45f, 0.4f, 0.55f) : toast_kind_ == 2 ? ImVec4(accent.x, accent.y, accent.z, 0.55f) : ImVec4(1.0f, 1.0f, 1.0f, 0.09f);
        draw->AddRect(min, max, paint::to_u32(edge, alpha), radius, 0, 1.0f * s);

        const ImVec2 icon{ min.x + 12.0f * s + icon_slot * 0.5f - 2.0f * s, (min.y + max.y) * 0.5f };
        if (toast_kind_ == 1)
        {
            const float spin = static_cast<float>(now) * 5.5f;
            draw->PathArcTo(icon, 8.0f * s, spin, spin + std::numbers::pi_v<float> * 1.35f, 20);
            draw->PathStroke(paint::to_u32(ImVec4(accent.x, accent.y, accent.z, 1.0f), alpha), 0, 2.0f * s);
        }
        else if (toast_kind_ == 3)
        {
            draw->AddCircleFilled(icon, 5.0f * s, paint::rgba(1.0f, 0.45f, 0.4f, alpha), 20);
        }
        else
        {
            if (const icons::icon* image = icons::get(model::button::guide, family, icons::style::solid))
                icons::draw(draw, *image, icon, 22.0f * s, paint::rgba(0.97f, 0.97f, 0.98f, alpha));
            const float pop = std::clamp(age / 0.35f, 0.0f, 1.0f);
            draw->AddCircleFilled(ImVec2(icon.x + 9.0f * s, icon.y + 8.0f * s), 4.0f * s * (0.6f + 0.4f * pop), paint::to_u32(ImVec4(accent.x, accent.y, accent.z, 1.0f), alpha), 16);
        }

        const float text_x = min.x + 12.0f * s + icon_slot;
        text(draw, fonts::get(fonts::role::strong), 13.0f, ImVec2(text_x, (min.y + max.y) * 0.5f - title_size.y * 0.5f), paint::rgba(0.96f, 0.96f, 0.97f, alpha), title);
        if (note)
            text(draw, nullptr, 12.0f, ImVec2(text_x + title_size.x + 10.0f * s, (min.y + max.y) * 0.5f - note_size.y * 0.5f), paint::to_u32(ImVec4(accent.x, accent.y, accent.z, 1.0f), alpha), note);
    }

    void remap_view::draw_editor(ImVec2 min, ImVec2 max, float appear)
    {
        const model::button which = model_->part_button(static_cast<std::size_t>(editor_part_));
        const auto family = model::layout_family(model::catalog()[catalog_index_].file);
        ImGui::PushID("##editor");
        const power_state power{ power_error() };
        const editor_result result = editor_.draw(min, max, which, family, bindings_, nullptr, which == model::button::guide ? &power : nullptr, accent_of(catalog_index_), appear);
        ImGui::PopID();
        if (result.changed)
            apply_bindings(true);
        if (result.close)
            set_selected(-1);
    }
}
