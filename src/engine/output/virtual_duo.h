#pragma once

#include "virtual_pad.h"

#include <engine/types.h>

#include <windows.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace engine::output
{
    class virtual_duo final : public virtual_pad
    {
    public:
        ~virtual_duo() override;

        virtual_duo(const virtual_duo&) = delete;
        virtual_duo& operator=(const virtual_duo&) = delete;

        static std::unique_ptr<virtual_duo> open(const std::wstring& library, virtual_pad_kind kind, std::string& error);

        bool set_buttons(control_button buttons, xbox_motion motion = {}) override;
        void dispose() override;

    private:
        using initialize_fn = HRESULT(WINAPI*)();
        using create_fn = HRESULT(WINAPI*)(int, void*, void*, void**);
        using remove_fn = HRESULT(WINAPI*)(void*);
        using send_fn = HRESULT(WINAPI*)(void*, void*);

        virtual_duo() = default;

        HMODULE module_ = nullptr;
        initialize_fn uninitialize_ = nullptr;
        remove_fn remove_ = nullptr;
        send_fn send_ = nullptr;
        void* controller_ = nullptr;
        virtual_pad_kind kind_ = virtual_pad_kind::xbox_elite;
        std::array<std::uint8_t, 128> report_{};
        control_button last_buttons_ = control_button::none;
        xbox_motion last_motion_{};
        unsigned long long last_sent_ = 0;
        bool has_last_ = false;
    };
}
