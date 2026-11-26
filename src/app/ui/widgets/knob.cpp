#include "knob.h"
#include "../fonts.h"
#include "../paint.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>

namespace menu::ui::widgets
{
    bool knob(const char* id, const char* label, ImVec2 center, float radius, float& value, const ImVec4& accent)
    {
        constexpr float pi = std::numbers::pi_v<float>;
        constexpr float start = pi * 0.75f;
        constexpr float sweep = pi * 1.5f;
        constexpr int ticks = 36;

        const float s = ImGui::GetStyle().FontScaleDpi;
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImGuiIO& io = ImGui::GetIO();

        ImGui::SetCursorScreenPos(ImVec2(center.x - radius, center.y - radius));
        ImGui::InvisibleButton(id, ImVec2(radius * 2.0f, radius * 2.0f));
        const ImGuiID key = ImGui::GetItemID();
        const bool hovered = ImGui::IsItemHovered();
        const bool active = ImGui::IsItemActive();
        bool changed = false;
        if (active && io.MouseDelta.y != 0.0f)
        {
            value = std::clamp(value - io.MouseDelta.y * 0.004f, 0.0f, 1.0f);
            changed = true;
        }
        if (hovered && io.MouseWheel != 0.0f)
        {
            value = std::clamp(value + io.MouseWheel * 0.02f, 0.0f, 1.0f);
            changed = true;
        }
        if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            value = 0.5f;
            changed = true;
        }
        if (hovered || active)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);

        ImGuiStorage* storage = ImGui::GetStateStorage();
        const float follow = 1.0f - std::exp(-16.0f * io.DeltaTime);
        float shown = storage->GetFloat(key, value);
        shown += (value - shown) * follow;
        storage->SetFloat(key, shown);

        const ImGuiID focus_key = key + 1;
        float focus = storage->GetFloat(focus_key, 0.0f);
        focus += ((active ? 1.0f : hovered ? 0.65f : 0.0f) - focus) * (1.0f - std::exp(-12.0f * io.DeltaTime));
        storage->SetFloat(focus_key, focus);

        ImGui::PushFont(fonts::get(fonts::role::strong), 11.5f);
        const ImVec2 title = ImGui::CalcTextSize(label);
        draw->AddText(ImVec2(std::round(center.x - title.x * 0.5f), std::round(center.y - radius - 8.0f * s - title.y)),
            paint::rgba(0.9f, 0.9f, 0.93f, 0.75f + 0.25f * focus), label);
        ImGui::PopFont();

        if (focus > 0.01f)
            paint::soft_ellipse(draw, center, ImVec2(radius * 1.35f, radius * 1.35f), ImVec4(accent.x, accent.y, accent.z, 0.22f * focus), 8, 48);

        const float inner = radius * 0.6f;
        for (int i = 0; i <= ticks; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(ticks);
            const float angle = start + sweep * t;
            const ImVec2 dir{ std::cos(angle), std::sin(angle) };
            const float lit = std::clamp((shown - t) * static_cast<float>(ticks) + 1.0f, 0.0f, 1.0f);
            const float near_tip = std::clamp(1.0f - std::abs(shown - t) * static_cast<float>(ticks) * 0.5f, 0.0f, 1.0f);
            const ImVec4 off{ 1.0f, 1.0f, 1.0f, 0.13f };
            const ImVec4 on = paint::mix(ImVec4(1.0f, 1.0f, 1.0f, 0.95f), ImVec4(accent.x, accent.y, accent.z, 1.0f), near_tip * focus);
            const float outer = radius * (0.95f + 0.06f * lit + 0.05f * near_tip * lit);
            draw->AddLine(ImVec2(center.x + dir.x * radius * 0.76f, center.y + dir.y * radius * 0.76f), ImVec2(center.x + dir.x * outer, center.y + dir.y * outer),
                paint::to_u32(paint::mix(off, on, lit)), (1.5f + 0.4f * lit) * s);
        }

        paint::soft_ellipse(draw, ImVec2(center.x, center.y + inner * 0.18f), ImVec2(inner * 1.25f, inner * 1.25f), ImVec4(0.0f, 0.0f, 0.0f, 0.55f), 8, 40);
        const float press = active ? 0.96f : 1.0f;
        draw->AddCircleFilled(center, inner * press, paint::rgba(0.97f, 0.97f, 0.98f, 1.0f), 48);
        draw->AddCircle(center, inner * press - 0.5f, paint::rgba(0.0f, 0.0f, 0.0f, 0.12f), 48, 1.0f);

        const float angle = start + sweep * shown;
        const ImVec2 dir{ std::cos(angle), std::sin(angle) };
        const ImVec2 points[] = { ImVec2(center.x + dir.x * inner * 0.22f, center.y + dir.y * inner * 0.22f), ImVec2(center.x + dir.x * inner * 0.7f, center.y + dir.y * inner * 0.7f) };
        paint::round_polyline(draw, points, 2, paint::rgba(0.06f, 0.06f, 0.07f, 1.0f), 2.6f * s);

        char text[16];
        std::snprintf(text, sizeof(text), "%d%%", static_cast<int>(std::round(shown * 100.0f)));
        ImGui::PushFont(nullptr, 11.5f);
        const ImVec2 size = ImGui::CalcTextSize(text);
        draw->AddText(ImVec2(std::round(center.x - size.x * 0.5f), std::round(center.y + radius + 6.0f * s)), paint::rgba(0.58f, 0.58f, 0.62f, 0.8f + 0.2f * focus), text);
        ImGui::PopFont();
        return changed;
    }
}
