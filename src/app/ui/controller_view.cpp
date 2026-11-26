#include "controller_view.h"
#include "fonts.h"
#include "paint.h"

#include "../model/catalog.h"
#include "../model/layout.h"
#include "../model/parts.h"
#include "../render/background.h"
#include "../render/device.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <numbers>
#include <numeric>
#include <string>

namespace menu::ui
{
    namespace
    {
        constexpr float min_pitch = -1.45f;
        constexpr float max_pitch = 1.45f;
        constexpr float focus_scale = 1.2f;
        constexpr float side_scale = 0.52f;
        constexpr float transition_seconds = 0.62f;
        constexpr float render_scale = 2.0f;
        constexpr render::camera default_camera{ 0.19f, 0.27f, 0.86f };
        constexpr float idle_delay = 2.0f;
        constexpr float sway_amount = 0.16f;

        const ImVec4 text_primary{ 0.965f, 0.965f, 0.97f, 1.0f };
        const ImVec4 text_muted{ 0.53f, 0.53f, 0.56f, 1.0f };

        float ease(float progress)
        {
            return progress * progress * progress * (progress * (progress * 6.0f - 15.0f) + 10.0f);
        }

        float ramp(float elapsed, float start, float duration)
        {
            return ease(std::clamp((elapsed - start) / duration, 0.0f, 1.0f));
        }

        float sway(double now)
        {
            return sway_amount * static_cast<float>(std::sin(now * 0.55));
        }

        ImVec4 accent_of(std::size_t index)
        {
            const auto& a = model::catalog()[index].accent;
            return ImVec4(a.x, a.y, a.z, 1.0f);
        }

        void text_centered(ImDrawList* draw, float center_x, float top, float size, ImU32 color, const char* text)
        {
            ImGui::PushFont(nullptr, size);
            const float width = ImGui::CalcTextSize(text).x;
            draw->AddText(ImVec2(std::round(center_x - width * 0.5f), std::round(top)), color, text);
            ImGui::PopFont();
        }
    }

    controller_view::controller_view()
        : library_(model::catalog().size())
        , slots_(model::catalog().size())
        , draw_order_(slots_.size())
    {
        std::iota(draw_order_.begin(), draw_order_.end(), std::size_t{ 0 });
        selected_ = std::min(selected_, slots_.size() - 1);
        previous_ = selected_;
    }

    bool controller_view::create()
    {
        for (std::size_t index = 0; index < slots_.size(); ++index)
            library_.request(index);

        for (std::size_t index = 0; index < slots_.size(); ++index)
        {
            if (!slots_[index].scene.create(render::device()))
                return false;
            slots_[index].pose = target_layout(index);
            slots_[index].transition_from = slots_[index].pose;
            slots_[index].view = default_camera;
            load_model(index);
        }
        return true;
    }

    void controller_view::select(int direction)
    {
        const auto count = static_cast<int>(slots_.size());
        select_model(static_cast<std::size_t>((static_cast<int>(selected_) + direction + count) % count));
    }

    void controller_view::select_model(std::size_t index)
    {
        if (index == selected_ || index >= slots_.size())
            return;

        for (auto& item : slots_)
            item.transition_from = item.pose;
        previous_ = selected_;
        selected_ = index;
        side_ = -side_;
        transition_ = 0.0f;
        last_interaction_ = ImGui::GetTime();
    }

    controller_view::layout controller_view::target_layout(std::size_t index) const
    {
        if (index == selected_)
            return { 0.50f, 0.465f, focus_scale };
        return { side_ > 0 ? 0.915f : 0.085f, 0.48f, side_scale };
    }

    float controller_view::focus_amount(std::size_t index) const
    {
        return std::clamp((slots_[index].pose.scale - side_scale) / (focus_scale - side_scale), 0.0f, 1.0f);
    }

    ImVec4 controller_view::accent() const
    {
        return paint::mix(accent_of(previous_), accent_of(selected_), ease(transition_));
    }

    void controller_view::load_model(std::size_t index)
    {
        auto& item = slots_[index];
        auto loaded = library_.take(index);
        if (!loaded)
            return;

        if (const float glass = model::catalog()[index].glass_opacity; glass > 0.0f)
        {
            for (auto& m : loaded->materials)
            {
                if (m.alpha == model::alpha_mode::blend)
                {
                    m.split_alpha = true;
                    m.glass_opacity = glass;
                }
            }
        }

        const auto& entry = model::catalog()[index];
        const DirectX::XMMATRIX transform = model::display_transform(*loaded, entry.rotation);
        const auto parts = model::assign_parts(*loaded, model::layout_rules(entry.file), transform);
        auto gpu = std::make_shared<render::gpu_model>();
        if (!gpu->upload(render::device(), render::context(), *loaded, parts))
            return;

        DirectX::XMVECTOR low = DirectX::XMVectorReplicate(FLT_MAX);
        DirectX::XMVECTOR high = DirectX::XMVectorReplicate(-FLT_MAX);
        for (const auto& v : loaded->vertices)
        {
            const DirectX::XMVECTOR p = DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&v.position), transform);
            low = DirectX::XMVectorMin(low, p);
            high = DirectX::XMVectorMax(high, p);
        }
        DirectX::XMFLOAT3 display_low;
        DirectX::XMFLOAT3 display_high;
        DirectX::XMStoreFloat3(&display_low, low);
        DirectX::XMStoreFloat3(&display_high, high);
        gpu->set_display_bounds(display_low, display_high);
        item.model = std::move(gpu);
    }

    ID3D11ShaderResourceView* controller_view::render_model(std::size_t index, const render::camera& view, std::uint32_t width, std::uint32_t height)
    {
        auto& item = slots_[index];
        if (!item.model)
        {
            load_model(index);
            if (!item.model)
                return nullptr;
        }

        const auto& previous = item.rendered_camera;
        if (item.image && item.width == width && item.height == height &&
            view.yaw == previous.yaw && view.pitch == previous.pitch && view.zoom == previous.zoom)
            return item.image;

        item.image = item.scene.render(render::context(), *item.model, model::catalog()[index].rotation, view,
            render::lighting{ model::catalog()[index].exposure, model::catalog()[index].ambient, model::catalog()[index].neutral_tone, model::catalog()[index].key_front }, width, height);
        item.rendered_camera = view;
        item.width = width;
        item.height = height;
        return item.image;
    }

    void controller_view::handle_input(ImVec2 origin, ImVec2 size)
    {
        const ImGuiIO& io = ImGui::GetIO();
        const double now = ImGui::GetTime();

        ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + size.y * 0.17f));
        ImGui::SetNextItemAllowOverlap();
        const bool clicked = ImGui::InvisibleButton("##carousel", ImVec2(size.x, size.y * 0.51f), ImGuiButtonFlags_MouseButtonLeft);
        const float x = (io.MousePos.x - origin.x) / size.x;
        const bool over_side = side_ < 0 ? x < 0.22f : x > 0.78f;
        const int side = over_side ? side_ : 0;
        auto& view = slots_[selected_].view;

        if (ImGui::IsItemActivated())
        {
            pressed_side_ = side;
            dragged_ = false;
            const float idle = ramp(static_cast<float>(now - last_interaction_), idle_delay, 1.2f);
            view.yaw += sway(now) * idle;
            last_interaction_ = now;
        }

        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            dragged_ = true;
            last_interaction_ = now;
            if (pressed_side_ == 0)
            {
                view.yaw = std::remainder(view.yaw - io.MouseDelta.x * 0.008f, 2.0f * std::numbers::pi_v<float>);
                view.pitch = std::clamp(view.pitch + io.MouseDelta.y * 0.008f, min_pitch, max_pitch);
            }
        }

        if (ImGui::IsItemHovered())
            ImGui::SetMouseCursor(side != 0 ? ImGuiMouseCursor_Hand : ImGuiMouseCursor_ResizeAll);
        if (ImGui::IsItemHovered() && side == 0 && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            view = default_camera;
            last_interaction_ = now;
        }

        int step = 0;
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false))
            step = -1;
        else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, false))
            step = 1;
        else if (clicked && !dragged_ && side != 0 && side == pressed_side_)
            step = side;

        if (step != 0)
        {
            (step < 0 ? previous_arrow_ : next_arrow_).trigger();
            select(step);
        }
    }

    void controller_view::update_poses()
    {
        transition_ = std::min(transition_ + ImGui::GetIO().DeltaTime / transition_seconds, 1.0f);
        const float blend = ease(transition_);
        for (std::size_t index = 0; index < slots_.size(); ++index)
        {
            auto& item = slots_[index];
            const auto& from = item.transition_from;
            const layout target = target_layout(index);
            item.pose = { std::lerp(from.x, target.x, blend), std::lerp(from.y, target.y, blend), std::lerp(from.scale, target.scale, blend) };

            if (index != selected_)
            {
                const float settle = 1.0f - std::exp(-5.0f * ImGui::GetIO().DeltaTime);
                item.view.yaw = std::lerp(item.view.yaw, default_camera.yaw, settle);
                item.view.pitch = std::lerp(item.view.pitch, default_camera.pitch, settle);
                item.view.zoom = std::lerp(item.view.zoom, default_camera.zoom, settle);
            }
        }
    }

    void controller_view::draw_backdrop(ImVec2, ImVec2)
    {
        const ImVec4 tint = accent();
        render::background::set_tint(tint.x, tint.y, tint.z, 1.0f);
    }

    void controller_view::draw_header(ImVec2 origin, ImVec2 size)
    {
        const float s = ImGui::GetStyle().FontScaleDpi;
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float center_x = origin.x + size.x * 0.5f;
        float y = origin.y + 40.0f * s;

        ImGui::PushFont(fonts::get(fonts::role::strong), 28.0f);
        const char* title = "Choose your controller";
        const float tracking = -0.35f * s;
        const ImVec2 title_size = paint::spaced_text_size(title, tracking);
        paint::spaced_text(draw, ImVec2(std::round(center_x - title_size.x * 0.5f), y), tracking, paint::to_u32(text_primary), title);
        ImGui::PopFont();

        y += 40.0f * s;
        text_centered(draw, center_x, y, 14.5f, paint::to_u32(text_muted), "You can switch anytime in settings.");
    }

    void controller_view::draw_models(ImVec2 origin, ImVec2 size)
    {
        const float s = ImGui::GetStyle().FontScaleDpi;
        const double now = ImGui::GetTime();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float span = std::min(size.x, size.y * 1.25f);
        const auto width = static_cast<std::uint32_t>(std::ceil(span * 0.61f * focus_scale * render_scale));
        const auto height = static_cast<std::uint32_t>(std::ceil(span * 0.49f * focus_scale * render_scale));
        const float idle = ramp(static_cast<float>(now - last_interaction_), idle_delay, 1.2f);

        std::sort(draw_order_.begin(), draw_order_.end(), [&](std::size_t left, std::size_t right)
        {
            return slots_[left].pose.scale < slots_[right].pose.scale;
        });

        for (const std::size_t index : draw_order_)
        {
            const auto& item = slots_[index];
            const float focus = focus_amount(index);
            const ImVec2 extent{ span * 0.61f * item.pose.scale, span * 0.49f * item.pose.scale };
            const float bob = static_cast<float>(std::sin(now * 1.1)) * 3.5f * s * focus;
            const ImVec2 center{ origin.x + size.x * item.pose.x, origin.y + size.y * item.pose.y + bob };

            const ImVec2 shadow_center{ center.x, origin.y + size.y * item.pose.y + extent.y * 0.27f };
            const float lift = 1.0f - (bob / (3.5f * s + 1e-3f)) * 0.08f;
            paint::soft_ellipse(draw, shadow_center, ImVec2(extent.x * 0.31f * lift, extent.y * 0.05f * lift), ImVec4(0.0f, 0.0f, 0.02f, std::lerp(0.25f, 0.62f, focus)));

            render::camera view = item.view;
            if (index == selected_)
                view.yaw += sway(now) * idle;

            const ImVec2 top_left{ center.x - extent.x * 0.5f, center.y - extent.y * 0.5f };
            const ImVec2 bottom_right{ top_left.x + extent.x, top_left.y + extent.y };
            if (ID3D11ShaderResourceView* image = render_model(index, view, width, height))
            {
                const float shade = std::lerp(0.62f, 1.0f, focus);
                const ImU32 tint = paint::rgba(shade, shade * 1.02f, shade * 1.06f, std::lerp(0.55f, 1.0f, focus));
                paint::render_image(draw, image, top_left, bottom_right, tint);
            }
            else if (index == selected_)
            {
                const float pulse = 0.5f + 0.5f * static_cast<float>(std::sin(now * 3.0));
                text_centered(draw, center.x, center.y - 8.0f * s, 14.0f, paint::to_u32(text_muted, 0.5f + 0.4f * pulse), "Loading controller");
            }
        }
    }

    void controller_view::draw_caption(ImVec2 origin, ImVec2 size)
    {
        const float s = ImGui::GetStyle().FontScaleDpi;
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float center_x = origin.x + size.x * 0.5f;
        const float blend = ease(transition_);
        const auto& entry = model::catalog()[selected_];

        const float name_top = origin.y + size.y * 0.712f;
        const float slide = (1.0f - blend) * 10.0f * s;
        ImGui::PushFont(fonts::get(fonts::role::strong), 30.0f);
        const float name_height = ImGui::GetFontSize();
        const float name_width = ImGui::CalcTextSize(entry.label).x;
        draw->AddText(ImVec2(std::round(center_x - name_width * 0.5f), std::round(name_top + slide)), paint::to_u32(text_primary, std::lerp(0.0f, 1.0f, blend)), entry.label);
        ImGui::PopFont();

        const float subtitle_top = name_top + name_height + 6.0f * s;
        text_centered(draw, center_x, subtitle_top + slide * 0.6f, 13.5f, paint::to_u32(text_muted, blend), entry.subtitle);

        const float dots_y = subtitle_top + 34.0f * s;
        const float gap = 8.0f * s;
        float total = 0.0f;
        for (std::size_t index = 0; index < slots_.size(); ++index)
            total += std::lerp(6.0f, 22.0f, focus_amount(index)) * s + (index ? gap : 0.0f);

        float x = center_x - total * 0.5f;
        const ImVec4 tint = accent();
        for (std::size_t index = 0; index < slots_.size(); ++index)
        {
            const float focus = focus_amount(index);
            const float dot_width = std::lerp(6.0f, 22.0f, focus) * s;
            const float dot_height = 6.0f * s;
            ImGui::PushID(static_cast<int>(index));
            ImGui::SetCursorScreenPos(ImVec2(x - gap * 0.5f, dots_y - 10.0f * s));
            if (ImGui::InvisibleButton("##page", ImVec2(dot_width + gap, 20.0f * s)))
                select_model(index);
            if (ImGui::IsItemHovered())
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            ImGui::PopID();

            const ImVec4 off{ 1.0f, 1.0f, 1.0f, 0.22f };
            draw->AddRectFilled(ImVec2(x, dots_y - dot_height * 0.5f), ImVec2(x + dot_width, dots_y + dot_height * 0.5f),
                paint::to_u32(paint::mix(off, tint, focus)), dot_height * 0.5f);
            x += dot_width + gap;
        }
    }

    void controller_view::draw_controls(ImVec2 origin, ImVec2 size)
    {
        const float s = ImGui::GetStyle().FontScaleDpi;
        const float height = 46.0f * s;
        const float middle_y = origin.y + size.y * 0.465f;
        const float offset = size.x * 0.285f;

        if (previous_arrow_.draw("##previous", ImVec2(origin.x + size.x * 0.5f - offset, middle_y), height, -1, accent()))
            select(-1);
        if (next_arrow_.draw("##next", ImVec2(origin.x + size.x * 0.5f + offset, middle_y), height, 1, accent()))
            select(1);

        if (confirm_.draw("##confirm", ImVec2(origin.x + size.x * 0.5f, origin.y + size.y * 0.885f), height))
            confirmed_model_ = selected_;
    }

    void controller_view::draw()
    {
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 size = ImGui::GetContentRegionAvail();
        if (size.x < 1.0f || size.y < 1.0f)
            return;

        if (last_interaction_ == 0.0)
            last_interaction_ = ImGui::GetTime();

        handle_input(origin, size);
        update_poses();
        draw_backdrop(origin, size);
        draw_header(origin, size);
        draw_models(origin, size);
        draw_caption(origin, size);
        draw_controls(origin, size);
    }
}
