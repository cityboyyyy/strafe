#include "ui.h"
#include "controller_view.h"
#include "remap_view.h"

#include <engine/engine.h>
#include "fonts.h"
#include "icons.h"
#include "title_bar.h"
#include "../window/window.h"
#include "../render/background.h"
#include "../model/catalog.h"
#include "../system/driver_installer.h"

#include <imgui.h>

#include <memory>

namespace menu::ui
{
    namespace
    {
        enum class screen
        {
            picker,
            remap
        };

        std::unique_ptr<controller_view> g_controller_view;
        std::unique_ptr<remap_view> g_remap_view;
        std::unique_ptr<engine::remapper> g_engine;
        screen g_screen = screen::picker;
        double g_confirmed_at = -1.0;

        void draw_screen()
        {
            if (g_screen == screen::remap)
            {
                if (g_remap_view->draw())
                {
                    g_remap_view->close();
                    g_screen = screen::picker;
                }
                return;
            }

            g_controller_view->draw();
            const auto chosen = g_controller_view->confirmed_model();
            if (!chosen)
                return;

            const double now = ImGui::GetTime();
            if (g_confirmed_at < 0.0)
                g_confirmed_at = now;
            else if (now - g_confirmed_at > 0.9)
            {
                g_remap_view->open(g_controller_view->model(*chosen), *chosen);
                g_controller_view->reset_confirmation();
                g_confirmed_at = -1.0;
                g_screen = screen::remap;
            }
        }
    }

    bool setup(float dpi_scale)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;

        fonts::load();
        icons::load();
        render::background::set_scale(dpi_scale);

        ImGuiStyle& style = ImGui::GetStyle();
        ImGui::StyleColorsDark();
        style.FontSizeBase = 16.0f;
        style.WindowBorderSize = 0.0f;
        style.WindowRounding = 0.0f;
        style.FrameRounding = 6.0f;
        style.GrabRounding = 6.0f;
        style.ScaleAllSizes(dpi_scale);
        style.FontScaleDpi = dpi_scale;

        g_engine = std::make_unique<engine::remapper>();
        window::set_activate_handler([](bool active)
        {
            if (g_engine)
                g_engine->set_editor_focused(active);
        });

        g_controller_view = std::make_unique<controller_view>();
        g_remap_view = std::make_unique<remap_view>();
        g_remap_view->set_engine(g_engine.get());
        return g_controller_view->create() && g_remap_view->create();
    }

    void draw()
    {
        window::clear_drag_holes();

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const auto panel = window::panel();
        ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + panel.x, viewport->Pos.y + panel.y));
        ImGui::SetNextWindowSize(ImVec2(panel.width, panel.height));

        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        if (ImGui::Begin("##root", nullptr, flags))
        {
            draw_screen();
        }
        ImGui::End();

        const auto controls = window::controls();
        ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + controls.x, viewport->Pos.y + controls.y));
        ImGui::SetNextWindowSize(ImVec2(controls.width, controls.height));
        if (ImGui::Begin("##chrome", nullptr, flags))
            draw_title_bar();
        ImGui::End();
        ImGui::PopStyleVar();
    }

    void shutdown()
    {
        window::set_activate_handler(nullptr);
        system::driver::shutdown();
        g_remap_view.reset();
        g_controller_view.reset();
        icons::release();
        g_engine.reset();
    }
}
