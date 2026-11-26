#include "title_bar.h"
#include "paint.h"

#include "../window/window.h"

#include <imgui.h>

#include <algorithm>

namespace menu::ui
{
    namespace
    {
        const ImVec4 ink{ 0.88f, 0.89f, 0.92f, 1.0f };

        bool segment(const char* id, ImVec2 min, ImVec2 size)
        {
            ImGui::SetCursorScreenPos(min);
            const bool pressed = ImGui::InvisibleButton(id, size);
            if (ImGui::IsItemHovered())
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            return pressed;
        }
    }

    void draw_title_bar()
    {
        const float s = ImGui::GetStyle().FontScaleDpi;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImDrawList* draw = ImGui::GetWindowDrawList();

        const auto area = window::controls();
        const float height = area.height;
        const float half = area.width * 0.5f;
        const float radius = height * 0.5f;
        const ImVec2 min{ viewport->Pos.x + area.x, viewport->Pos.y + area.y };
        const ImVec2 max{ min.x + area.width, min.y + height };
        const float middle = min.x + half;

        const bool minimize = segment("##minimize", min, ImVec2(half, height));
        const bool close = segment("##close", ImVec2(middle, min.y), ImVec2(half, height));

        draw->AddLine(ImVec2(middle, min.y + height * 0.26f), ImVec2(middle, max.y - height * 0.26f), paint::rgba(1.0f, 1.0f, 1.0f, 0.12f), 1.0f);
        draw->AddRect(ImVec2(min.x + 0.5f, min.y + 0.5f), ImVec2(max.x - 0.5f, max.y - 0.5f), paint::rgba(1.0f, 1.0f, 1.0f, 0.11f), radius, 0, 1.0f);

        const float stroke = std::max(1.6f * s, height * 0.068f);
        const float center_y = min.y + height * 0.5f;
        const ImU32 color = paint::to_u32(ink);

        const float minimize_x = min.x + half * 0.5f + 1.5f * s;
        const float dash = height * 0.17f;
        const ImVec2 dash_points[] = { ImVec2(minimize_x - dash, center_y), ImVec2(minimize_x + dash, center_y) };
        paint::round_polyline(draw, dash_points, 2, color, stroke);

        const float close_x = middle + half * 0.5f - 1.5f * s;
        const float arm = height * 0.145f;
        const ImVec2 first[] = { ImVec2(close_x - arm, center_y - arm), ImVec2(close_x + arm, center_y + arm) };
        const ImVec2 second[] = { ImVec2(close_x - arm, center_y + arm), ImVec2(close_x + arm, center_y - arm) };
        paint::round_polyline(draw, first, 2, color, stroke);
        paint::round_polyline(draw, second, 2, color, stroke);

        if (minimize)
            window::minimize();
        if (close)
            window::close();
    }
}
