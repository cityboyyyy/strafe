#include "icons.h"
#include "paint.h"

#include "../model/asset_store.h"
#include "../model/image_decoder.h"
#include "../render/device.h"

#include <Windows.h>
#include <objbase.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <array>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace menu::ui::icons
{
    namespace
    {
        constexpr std::size_t button_count = static_cast<std::size_t>(model::button::count);

        constexpr std::array<const wchar_t*, button_count> file_names{
            L"south", L"east", L"west", L"north",
            L"left_bumper", L"right_bumper", L"left_trigger", L"right_trigger",
            L"dpad", L"dpad_up", L"dpad_down", L"dpad_left", L"dpad_right",
            L"left_stick", L"right_stick",
            L"view", L"menu", L"guide", L"touchpad", L"mute",
            L"paddle_1", L"paddle_2", L"paddle_3", L"paddle_4",
        };

        struct slot
        {
            ComPtr<ID3D11ShaderResourceView> view;
            icon value;
        };

        std::array<std::array<std::array<slot, button_count>, 2>, 2> g_slots;

        ComPtr<ID3D11ShaderResourceView> upload(const model::image& image)
        {
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = image.width;
            desc.Height = image.height;
            desc.MipLevels = 0;
            desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
            desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

            ComPtr<ID3D11Texture2D> texture;
            if (FAILED(render::device()->CreateTexture2D(&desc, nullptr, &texture)))
                return nullptr;
            render::context()->UpdateSubresource(texture.Get(), 0, nullptr, image.pixels.data(), image.width * 4, 0);

            ComPtr<ID3D11ShaderResourceView> view;
            if (FAILED(render::device()->CreateShaderResourceView(texture.Get(), nullptr, &view)))
                return nullptr;
            render::context()->GenerateMips(view.Get());
            return view;
        }

        bool load_file(const std::filesystem::path& relative, slot& out)
        {
            const auto bytes = model::asset_bytes(relative);
            model::image image;
            if (bytes.empty() || !model::decode_image(bytes.data(), bytes.size(), 512, image) || image.width < 2)
                return false;
            for (std::size_t i = 0; i + 3 < image.pixels.size(); i += 4)
            {
                image.pixels[i] = 255;
                image.pixels[i + 1] = 255;
                image.pixels[i + 2] = 255;
            }

            out.view = upload(image);
            if (!out.view)
                return false;
            out.value.texture = out.view.Get();
            out.value.aspect = static_cast<float>(image.width) / static_cast<float>(image.height);
            return true;
        }
    }

    bool load()
    {
        const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        constexpr const wchar_t* families[] = { L"xbox", L"playstation" };
        constexpr const wchar_t* looks[] = { L"outline", L"solid" };
        for (std::size_t f = 0; f < 2; ++f)
        {
            for (std::size_t l = 0; l < 2; ++l)
            {
                for (std::size_t b = 0; b < button_count; ++b)
                {
                    const auto path = std::filesystem::path(L"icons") / families[f] / looks[l] / (std::wstring(file_names[b]) + L".png");
                    load_file(path, g_slots[f][l][b]);
                }
            }
        }
        if (SUCCEEDED(com))
            CoUninitialize();
        return true;
    }

    void release()
    {
        for (auto& family : g_slots)
            for (auto& look : family)
                for (auto& s : look)
                    s = slot{};
    }

    const icon* get(model::button which, model::family family, style look)
    {
        const auto b = static_cast<std::size_t>(which);
        if (b >= button_count)
            return nullptr;
        const slot& s = g_slots[family == model::family::playstation ? 1 : 0][look == style::solid ? 1 : 0][b];
        return s.view ? &s.value : nullptr;
    }

    void draw(ImDrawList* draw, const icon& image, ImVec2 center, float height, ImU32 tint)
    {
        const float width = height * image.aspect;
        paint::render_image(draw, image.texture, ImVec2(center.x - width * 0.5f, center.y - height * 0.5f), ImVec2(center.x + width * 0.5f, center.y + height * 0.5f), tint);
    }
}
