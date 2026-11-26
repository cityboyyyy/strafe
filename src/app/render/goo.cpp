#include "goo.h"
#include "device.h"

#include <goo_ps.h>

#include <backends/imgui_impl_dx11.h>
#include <imgui_internal.h>

#include <cstring>

namespace menu::render::goo
{
    namespace
    {
        struct constants
        {
            float c[8][4];
        };

        ID3D11PixelShader* g_shader = nullptr;
        ID3D11Buffer* g_buffer = nullptr;
        bool g_tried = false;

        bool ensure()
        {
            if (g_shader && g_buffer)
                return true;
            if (g_tried)
                return false;
            g_tried = true;
            ID3D11Device* device = render::device();
            if (!device)
                return false;
            if (FAILED(device->CreatePixelShader(g_goo_ps, sizeof(g_goo_ps), nullptr, &g_shader)))
                return false;
            D3D11_BUFFER_DESC desc{};
            desc.ByteWidth = sizeof(constants);
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            return SUCCEEDED(device->CreateBuffer(&desc, nullptr, &g_buffer));
        }

        void store(float out[4], ImU32 color)
        {
            const ImVec4 value = ImGui::ColorConvertU32ToFloat4(color);
            out[0] = value.x;
            out[1] = value.y;
            out[2] = value.z;
            out[3] = value.w;
        }

        void store(float out[4], float x, float y, float z, float w)
        {
            out[0] = x;
            out[1] = y;
            out[2] = z;
            out[3] = w;
        }

        void bind(const ImDrawList*, const ImDrawCmd* command)
        {
            const auto* data = static_cast<const constants*>(command->UserCallbackData);
            auto* state = static_cast<ImGui_ImplDX11_RenderState*>(ImGui::GetPlatformIO().Renderer_RenderState);
            ID3D11DeviceContext* context = state->DeviceContext;
            D3D11_MAPPED_SUBRESOURCE mapped{};
            if (FAILED(context->Map(g_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
                return;
            std::memcpy(mapped.pData, data, sizeof(constants));
            context->Unmap(g_buffer, 0);
            context->PSSetShader(g_shader, nullptr, 0);
            context->PSSetConstantBuffers(0, 1, &g_buffer);
        }
    }

    bool available()
    {
        return ensure();
    }

    void draw(ImDrawList* draw_list, const shape& g)
    {
        const ImVec2 o = ImGui::GetMainViewport()->Pos;
        if (!ensure())
        {
            draw_list->AddRectFilled(g.pill_min, g.pill_max, g.pill_color, g.pill_radius);
            if (g.bubble_radius > 0.5f)
                draw_list->AddCircleFilled(g.bubble_center, g.bubble_radius, g.bubble_color, 32);
            return;
        }

        constants data{};
        auto& c = data.c;
        store(c[0], g.pill_min.x - o.x, g.pill_min.y - o.y, g.pill_max.x - o.x, g.pill_max.y - o.y);
        store(c[1], g.bubble_center.x - o.x, g.bubble_center.y - o.y, g.bubble_radius, g.pill_radius);
        store(c[2], g.pill_color);
        store(c[3], g.bubble_color);
        store(c[4], g.glow_color);
        store(c[5], g.glow_start - o.x, g.glow_edge - o.x, g.glow_feather, g.glow_alpha);
        store(c[6], g.shadow_alpha, g.shadow_blur, g.shadow_offset, 0.0f);
        store(c[7], g.alpha, g.highlight, g.smoothness, 0.0f);

        const float margin = g.shadow_blur * 3.0f + g.shadow_offset + 2.0f;
        const float right = g.bubble_radius > 0.0f ? ImMax(g.pill_max.x, g.bubble_center.x + g.bubble_radius) : g.pill_max.x;
        draw_list->PushClipRectFullScreen();
        draw_list->AddCallback(bind, &data, sizeof(data));
        draw_list->AddRectFilled(ImVec2(g.pill_min.x - margin, g.pill_min.y - margin), ImVec2(right + margin, g.pill_max.y + margin), IM_COL32_WHITE);
        draw_list->AddCallback(ImGui::GetPlatformIO().DrawCallback_ResetRenderState, nullptr);
        draw_list->PopClipRect();
    }

    void release()
    {
        if (g_shader)
            g_shader->Release();
        if (g_buffer)
            g_buffer->Release();
        g_shader = nullptr;
        g_buffer = nullptr;
        g_tried = false;
    }
}
