#pragma once

#include "controller.h"
#include "raw_input.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace engine::input
{
    enum class playstation_model
    {
        dual_shock_4,
        dual_sense,
        dual_sense_edge
    };

    class playstation_input
    {
    public:
        struct selection
        {
            HANDLE handle = nullptr;
            std::optional<std::string> name;
            bool ready = false;
            control_button pressed = control_button::none;
            gamepad raw;
        };

        struct device_view
        {
            HANDLE handle = nullptr;
            std::string name;
            bool bluetooth = false;
            bool ready = false;
        };

        void refresh(const std::vector<raw_input::device_entry>& devices_in_windows);
        void remove(HANDLE handle);
        void read(const raw_input& raw);
        selection selected();
        std::vector<device_view> devices() const;

        static std::optional<playstation_model> model(DWORD product);
        static bool try_decode(playstation_model model, bool bluetooth, const std::uint8_t* data, std::size_t length,
            std::size_t offset, std::size_t size, control_button previous, control_button& pressed, gamepad& raw);

    private:
        struct device
        {
            playstation_model model = playstation_model::dual_sense;
            bool bluetooth = false;
            bool ready = false;
            control_button pressed = control_button::none;
            gamepad raw;

            std::string name() const;
        };

        std::optional<device>* find(HANDLE handle);
        device* identify(HANDLE handle);

        std::vector<std::pair<HANDLE, std::optional<device>>> devices_;
        HANDLE selected_ = nullptr;
    };
}
