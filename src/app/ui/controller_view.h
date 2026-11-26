#pragma once

#include "../model/library.h"
#include "../render/gpu_model.h"
#include "../render/scene.h"
#include "widgets/arrow_button.h"
#include "widgets/confirm_button.h"

#include <imgui.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

namespace menu::ui
{
    class controller_view
    {
    public:
        controller_view();

        bool create();
        void draw();
        std::optional<std::size_t> confirmed_model() const { return confirmed_model_; }
        void reset_confirmation() { confirmed_model_.reset(); }
        std::shared_ptr<render::gpu_model> model(std::size_t index) const { return index < slots_.size() ? slots_[index].model : nullptr; }

    private:
        struct layout
        {
            float x;
            float y;
            float scale;
        };

        struct slot
        {
            render::scene scene;
            std::shared_ptr<render::gpu_model> model;
            layout pose{};
            layout transition_from{};
            render::camera view{};
            ID3D11ShaderResourceView* image = nullptr;
            render::camera rendered_camera{};
            std::uint32_t width = 0;
            std::uint32_t height = 0;
        };

        void select(int direction);
        void select_model(std::size_t index);
        void handle_input(ImVec2 origin, ImVec2 size);
        void update_poses();
        void draw_backdrop(ImVec2 origin, ImVec2 size);
        void draw_header(ImVec2 origin, ImVec2 size);
        void draw_models(ImVec2 origin, ImVec2 size);
        void draw_caption(ImVec2 origin, ImVec2 size);
        void draw_controls(ImVec2 origin, ImVec2 size);
        layout target_layout(std::size_t index) const;
        float focus_amount(std::size_t index) const;
        ImVec4 accent() const;
        void load_model(std::size_t index);
        ID3D11ShaderResourceView* render_model(std::size_t index, const render::camera& view, std::uint32_t width, std::uint32_t height);

        model::library library_;
        std::vector<slot> slots_;
        std::vector<std::size_t> draw_order_;
        std::size_t selected_ = 1;
        std::size_t previous_ = 1;
        float transition_ = 1.0f;
        int side_ = 1;
        int pressed_side_ = 0;
        bool dragged_ = false;
        double last_interaction_ = 0.0;
        widgets::arrow_button previous_arrow_;
        widgets::arrow_button next_arrow_;
        widgets::confirm_button confirm_;
        std::optional<std::size_t> confirmed_model_;
    };
}
