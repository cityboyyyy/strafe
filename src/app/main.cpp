#include "window/window.h"
#include "window/layout.h"
#include "render/device.h"
#include "render/background.h"
#include "ui/ui.h"

#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx11.h>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    if (!menu::window::create(L"Strafe", 1000, 640))
        return 1;

    if (!menu::render::create(menu::window::handle()))
    {
        menu::render::destroy();
        menu::window::destroy();
        return 1;
    }

    menu::render::background::create(menu::render::device());

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplWin32_Init(menu::window::handle());
    ImGui_ImplDX11_Init(menu::render::device(), menu::render::context());

    if (!menu::ui::setup(menu::window::dpi_scale()))
    {
        menu::ui::shutdown();
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        menu::render::destroy();
        menu::window::destroy();
        return 1;
    }

    constexpr float clear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    bool first_frame = true;

    while (menu::window::pump())
    {
        if (menu::render::occluded())
        {
            Sleep(10);
            continue;
        }

        std::uint32_t width = 0;
        std::uint32_t height = 0;
        if (menu::window::take_resize(width, height))
            menu::render::resize(width, height);

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        menu::ui::draw();

        ImGui::Render();
        menu::render::begin(clear);
        const auto panel = menu::window::panel();
        const auto controls = menu::window::controls();
        const menu::render::background::area field{ panel.x, panel.y, panel.width, panel.height };
        const menu::render::background::area pill{ controls.x, controls.y, controls.width, controls.height };
        const float time = static_cast<float>(ImGui::GetTime());
        menu::render::background::draw(menu::render::context(), time, field, field, menu::window::layout::panel_radius * menu::window::dpi_scale());
        menu::render::background::draw(menu::render::context(), time, field, pill, controls.height * 0.5f);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        if (first_frame)
        {
            menu::window::show();
            first_frame = false;
        }
        menu::render::present();
    }

    ShowWindow(menu::window::handle(), SW_HIDE);
    menu::ui::shutdown();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    menu::render::background::destroy();
    menu::render::destroy();
    menu::window::destroy();
    return 0;
}
