#include "fonts.h"

#include "../model/asset_store.h"

#include <array>
#include <filesystem>

namespace menu::ui::fonts
{
    namespace
    {
        std::array<ImFont*, 3> g_fonts{};

        ImFont* add(const wchar_t* file)
        {
            const auto bytes = model::asset_bytes(std::filesystem::path(L"fonts") / file);
            if (bytes.empty())
                return nullptr;

            ImFontConfig config;
            config.FontDataOwnedByAtlas = false;
            return ImGui::GetIO().Fonts->AddFontFromMemoryTTF(const_cast<std::uint8_t*>(bytes.data()), static_cast<int>(bytes.size()), 16.0f, &config);
        }
    }

    void load()
    {
        g_fonts[static_cast<int>(role::body)] = add(L"Geist-Medium.ttf");
        g_fonts[static_cast<int>(role::strong)] = add(L"Geist-SemiBold.ttf");
        g_fonts[static_cast<int>(role::mono)] = add(L"GeistMono-Medium.ttf");

        if (ImFont* body = g_fonts[static_cast<int>(role::body)])
            ImGui::GetIO().FontDefault = body;
    }

    ImFont* get(role which)
    {
        ImFont* font = g_fonts[static_cast<int>(which)];
        return font ? font : g_fonts[static_cast<int>(role::body)];
    }
}
