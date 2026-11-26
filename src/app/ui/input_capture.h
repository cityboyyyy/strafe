#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace menu::ui
{
    class input_capture
    {
    public:
        void begin(int target);
        void cancel();
        bool active() const { return active_; }
        int target() const { return target_; }
        double started() const { return started_; }
        std::optional<std::uint32_t> poll(bool allow_left_click);

    private:
        bool active_ = false;
        bool armed_ = false;
        int target_ = -1;
        double started_ = 0.0;
        std::array<bool, 256> previous_{};
    };
}
