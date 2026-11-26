#include "curve_view.h"
#include "fonts.h"
#include "paint.h"
#include "widgets/knob.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>

namespace menu::ui
{
    namespace
    {
        constexpr int samples = 64;

        struct preset
        {
            const char* label;
            std::array<float, 6> points;
        };

        constexpr preset presets[] = {
            { "Linear", { 0.0f, 0.2f, 0.4f, 0.6f, 0.8f, 1.0f } },
            { "Smooth", { 0.0f, 0.1f, 0.24f, 0.43f, 0.68f, 1.0f } },
            { "Precise", { 0.0f, 0.06f, 0.16f, 0.32f, 0.58f, 1.0f } },
            { "Snappy", { 0.0f, 0.34f, 0.58f, 0.76f, 0.9f, 1.0f } },
        };

        float dpi()
        {
            return ImGui::GetStyle().FontScaleDpi;
        }

        float animate(ImGuiID key, float target, float speed = 14.0f)
        {
            ImGuiStorage* storage = ImGui::GetStateStorage();
            float value = storage->GetFloat(key, target);
            value += (target - value) * (1.0f - std::exp(-speed * ImGui::GetIO().DeltaTime));
            storage->SetFloat(key, value);
            return value;
        }

        float evaluate(const std::array<float, 6>& p, float x)
        {
            constexpr int n = 6;
            constexpr float h = 0.2f;
            float delta[n - 1];
            for (int i = 0; i < n - 1; ++i)
                delta[i] = (p[static_cast<std::size_t>(i + 1)] - p[static_cast<std::size_t>(i)]) / h;
            float m[n];
            m[0] = delta[0];
            m[n - 1] = delta[n - 2];
            for (int i = 1; i < n - 1; ++i)
                m[i] = delta[i - 1] * delta[i] <= 0.0f ? 0.0f : (delta[i - 1] + delta[i]) * 0.5f;
            for (int i = 0; i < n - 1; ++i)
            {
                if (std::abs(delta[i]) < 1e-6f)
                {
                    m[i] = m[i + 1] = 0.0f;
                    continue;
                }
                const float a = m[i] / delta[i];
                const float b = m[i + 1] / delta[i];
                const float r = a * a + b * b;
                if (r > 9.0f)
                {
                    const float t = 3.0f / std::sqrt(r);
                    m[i] = t * a * delta[i];
                    m[i + 1] = t * b * delta[i];
                }
            }
            x = std::clamp(x, 0.0f, 1.0f);
            const int i = std::min(static_cast<int>(x / h), n - 2);
            const float t = (x - static_cast<float>(i) * h) / h;
            const float t2 = t * t;
            const float t3 = t2 * t;
            const float y0 = p[static_cast<std::size_t>(i)];
            const float y1 = p[static_cast<std::size_t>(i + 1)];
            return std::clamp((2.0f * t3 - 3.0f * t2 + 1.0f) * y0 + (t3 - 2.0f * t2 + t) * h * m[i] + (-2.0f * t3 + 3.0f * t2) * y1 + (t3 - t2) * h * m[i + 1], 0.0f, 1.0f);
        }

        void section(ImDrawList* draw, ImVec2 position, const char* label)
        {
            ImGui::PushFont(fonts::get(fonts::role::strong), 10.5f);
            paint::spaced_text(draw, ImVec2(std::round(position.x), std::round(position.y)), 1.3f * dpi(), paint::rgba(0.52f, 0.52f, 0.56f, 1.0f), label);
            ImGui::PopFont();
        }
    }

    engine::aim_settings to_engine(const aim_profile& profile)
    {
        engine::aim_settings settings;
        settings.sensitivity_x = profile.sensitivity_x;
        settings.sensitivity_y = profile.sensitivity_y;
        settings.anti_deadzone = profile.anti_deadzone * 0.4f;
        settings.smoothing = profile.smoothing;
        for (int i = 0; i < 17; ++i)
            settings.curve[i] = evaluate(profile.points, static_cast<float>(i) / 16.0f);
        return settings;
    }

    bool load_aim(const std::filesystem::path& path, aim_profile& profile)
    {
        std::ifstream file(path);
        std::string header;
        if (!file || !std::getline(file, header) || header != "controller-aim 1")
            return false;
        aim_profile loaded;
        file >> loaded.sensitivity_x >> loaded.sensitivity_y >> loaded.anti_deadzone >> loaded.smoothing;
        for (float& point : loaded.points)
            file >> point;
        if (!file)
            return false;
        loaded.points[0] = 0.0f;
        profile = loaded;
        return true;
    }

    bool save_aim(const std::filesystem::path& path, const aim_profile& profile)
    {
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        std::ofstream file(path, std::ios::trunc);
        if (!file)
            return false;
        file << "controller-aim 1\n" << profile.sensitivity_x << ' ' << profile.sensitivity_y << ' ' << profile.anti_deadzone << ' ' << profile.smoothing;
        for (float point : profile.points)
            file << ' ' << point;
        file << '\n';
        return static_cast<bool>(file);
    }

    bool curve_view::pill(const char* id, ImVec2 min, ImVec2 max, const char* label, bool active, const ImVec4& accent)
    {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float s = dpi();
        ImGui::SetCursorScreenPos(min);
        const bool clicked = ImGui::InvisibleButton(id, ImVec2(max.x - min.x, max.y - min.y));
        const bool hovered = ImGui::IsItemHovered();
        if (hovered)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        const ImGuiID key = ImGui::GetItemID();
        const float hover = animate(key, hovered ? 1.0f : 0.0f);
        const float on = animate(key + 1, active ? 1.0f : 0.0f);
        const float radius = (max.y - min.y) * 0.5f;
        ImVec4 fill = paint::mix(ImVec4(1.0f, 1.0f, 1.0f, 0.04f), ImVec4(1.0f, 1.0f, 1.0f, 0.08f), hover);
        fill = paint::mix(fill, ImVec4(accent.x, accent.y, accent.z, 0.16f), on);
        ImVec4 edge = paint::mix(ImVec4(1.0f, 1.0f, 1.0f, 0.06f), ImVec4(1.0f, 1.0f, 1.0f, 0.14f), hover);
        edge = paint::mix(edge, ImVec4(accent.x, accent.y, accent.z, 0.8f), on);
        draw->AddRectFilled(min, max, paint::to_u32(fill), radius);
        draw->AddRect(min, max, paint::to_u32(edge), radius, 0, 1.0f * s);
        ImGui::PushFont(fonts::get(fonts::role::strong), 12.0f);
        const ImVec2 size = ImGui::CalcTextSize(label);
        const ImVec4 color = paint::mix(paint::mix(ImVec4(0.72f, 0.72f, 0.76f, 1.0f), ImVec4(0.97f, 0.97f, 0.98f, 1.0f), hover), ImVec4(accent.x, accent.y, accent.z, 1.0f), on * 0.35f);
        draw->AddText(ImVec2(std::round((min.x + max.x - size.x) * 0.5f), std::round((min.y + max.y - size.y) * 0.5f)), paint::to_u32(color), label);
        ImGui::PopFont();
        return clicked;
    }

    bool curve_view::draw(ImVec2 min, ImVec2 max, aim_profile& profile, const ImVec4& accent)
    {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImGuiIO& io = ImGui::GetIO();
        const float s = dpi();
        bool changed = false;
        if (opened_at_ < 0.0)
            opened_at_ = ImGui::GetTime();
        const float reveal = std::clamp(static_cast<float>(ImGui::GetTime() - opened_at_) / 0.35f, 0.0f, 1.0f);
        const float sweep = 1.0f - (1.0f - reveal) * (1.0f - reveal) * (1.0f - reveal);

        const float column = 250.0f * s;
        const float gap = 22.0f * s;
        const ImVec2 card_min = min;
        const ImVec2 card_max{ max.x - column - gap, max.y };
        const float pad = 34.0f * s;
        const ImVec2 plot_min{ card_min.x + pad + 8.0f * s, card_min.y + pad };
        const ImVec2 plot_max{ card_max.x - pad, card_max.y - pad - 10.0f * s };
        const ImVec2 plot_size{ plot_max.x - plot_min.x, plot_max.y - plot_min.y };
        const auto to_screen = [&](float x, float y) { return ImVec2(plot_min.x + x * plot_size.x, plot_max.y - y * plot_size.y); };

        for (int i = 0; i <= 4; ++i)
        {
            const float t = static_cast<float>(i) / 4.0f;
            const ImU32 line = paint::rgba(1.0f, 1.0f, 1.0f, i == 0 ? 0.1f : 0.035f);
            draw->AddLine(to_screen(t, 0.0f), to_screen(t, 1.0f), line, 1.0f);
            draw->AddLine(to_screen(0.0f, t), to_screen(1.0f, t), line, 1.0f);
        }
        const ImVec2 diagonal[] = { to_screen(0.0f, 0.0f), to_screen(1.0f, 1.0f) };
        paint::dashed_polyline(draw, diagonal, 2, paint::rgba(1.0f, 1.0f, 1.0f, 0.08f), 1.0f * s, 3.0f * s, 5.0f * s);

        std::array<ImVec2, samples + 1> curve{};
        const int visible = static_cast<int>(std::round(sweep * samples));
        for (int i = 0; i <= samples; ++i)
        {
            const float x = static_cast<float>(i) / samples;
            curve[static_cast<std::size_t>(i)] = to_screen(x, evaluate(profile.points, x));
        }
        if (visible > 0)
            paint::round_polyline(draw, curve.data(), visible + 1, paint::to_u32(ImVec4(accent.x, accent.y, accent.z, 1.0f)), 2.0f * s);

        int label_point = -1;
        for (int i = 1; i < 6; ++i)
        {
            const std::size_t at = static_cast<std::size_t>(i);
            const float x = static_cast<float>(i) / 5.0f;
            const ImVec2 point = to_screen(x, profile.points[at]);
            const float hit = 13.0f * s;
            ImGui::PushID(i);
            ImGui::SetCursorScreenPos(ImVec2(point.x - hit, point.y - hit));
            ImGui::InvisibleButton("##point", ImVec2(hit * 2.0f, hit * 2.0f));
            const bool hovered = ImGui::IsItemHovered();
            if (ImGui::IsItemActivated())
                dragging_ = i;
            if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                const float low = profile.points[at - 1];
                const float high = i < 5 ? profile.points[at + 1] : 1.0f;
                profile.points[at] = std::clamp(x, low, high);
                changed = true;
            }
            else if (ImGui::IsItemActive() && dragging_ == i)
            {
                const float y = std::clamp((plot_max.y - io.MousePos.y) / plot_size.y, 0.0f, 1.0f);
                const float low = profile.points[at - 1];
                const float high = i < 5 ? profile.points[at + 1] : 1.0f;
                const float value = std::round(std::clamp(y, low, high) * 100.0f) / 100.0f;
                if (value != profile.points[at])
                {
                    profile.points[at] = value;
                    changed = true;
                }
            }
            if (ImGui::IsItemDeactivated())
                dragging_ = -1;
            if (hovered || ImGui::IsItemActive())
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                label_point = i;
            }
            const float focus = animate(ImGui::GetItemID(), ImGui::IsItemActive() ? 1.0f : hovered ? 0.5f : 0.0f, 20.0f);
            ImGui::PopID();

            const float appear = std::clamp(sweep * 6.0f - static_cast<float>(i), 0.0f, 1.0f);
            if (appear <= 0.0f)
                continue;
            const float radius = (4.5f + 1.5f * focus) * s * appear;
            draw->AddCircleFilled(point, radius + 2.5f * s, paint::rgba(0.05f, 0.05f, 0.058f, 1.0f), 24);
            draw->AddCircleFilled(point, radius, paint::to_u32(paint::mix(ImVec4(0.95f, 0.95f, 0.96f, 1.0f), ImVec4(accent.x, accent.y, accent.z, 1.0f), focus), appear), 24);
        }

        if (label_point > 0)
        {
            const std::size_t at = static_cast<std::size_t>(label_point);
            const ImVec2 point = to_screen(static_cast<float>(label_point) / 5.0f, profile.points[at]);
            char value[32];
            std::snprintf(value, sizeof(value), "%d%%  \xC2\xB7  %d%%", label_point * 20, static_cast<int>(std::round(profile.points[at] * 100.0f)));
            ImGui::PushFont(fonts::get(fonts::role::strong), 11.5f);
            const ImVec2 size = ImGui::CalcTextSize(value);
            const float lx = std::clamp(point.x - size.x * 0.5f, plot_min.x, plot_max.x - size.x);
            draw->AddText(ImVec2(std::round(lx), std::round(point.y - 22.0f * s - size.y * 0.5f)), paint::rgba(0.9f, 0.9f, 0.93f, 1.0f), value);
            ImGui::PopFont();
        }


        const float left = max.x - column;
        float y = min.y + 6.0f * s;
        section(draw, ImVec2(left, y), "PRESET");
        y += 22.0f * s;
        const float pill_w = (column - 8.0f * s) * 0.5f;
        const float pill_h = 32.0f * s;
        for (std::size_t i = 0; i < std::size(presets); ++i)
        {
            const float px = left + static_cast<float>(i % 2) * (pill_w + 8.0f * s);
            const float py = y + static_cast<float>(i / 2) * (pill_h + 8.0f * s);
            bool active = true;
            for (std::size_t p = 0; p < 6; ++p)
                active = active && std::abs(profile.points[p] - presets[i].points[p]) < 0.002f;
            ImGui::PushID(static_cast<int>(i) + 100);
            if (pill("##preset", ImVec2(px, py), ImVec2(px + pill_w, py + pill_h), presets[i].label, active, accent) && !active)
            {
                profile.points = presets[i].points;
                opened_at_ = ImGui::GetTime() - 0.15;
                changed = true;
            }
            ImGui::PopID();
        }
        y += (pill_h + 8.0f * s) * 2.0f + 22.0f * s;

        section(draw, ImVec2(left, y), "TUNING");
        y += 22.0f * s;
        struct dial
        {
            const char* label;
            float* value;
        };
        const dial dials[] = {
            { "Sens X", &profile.sensitivity_x },
            { "Sens Y", &profile.sensitivity_y },
            { "Deadzone", &profile.anti_deadzone },
            { "Smoothing", &profile.smoothing },
        };
        const float radius = 25.0f * s;
        const float cell_h = std::min(118.0f * s, (max.y - y) * 0.5f);
        for (std::size_t i = 0; i < std::size(dials); ++i)
        {
            const ImVec2 center{ left + column * (i % 2 == 0 ? 0.25f : 0.75f), y + cell_h * (static_cast<float>(i / 2) + 0.5f) + 4.0f * s };
            ImGui::PushID(static_cast<int>(i) + 200);
            if (widgets::knob("##dial", dials[i].label, center, radius, *dials[i].value, accent))
                changed = true;
            ImGui::PopID();
        }
        return changed;
    }
}
