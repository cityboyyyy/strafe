#pragma once

#include <engine/types.h>

#include <memory>

namespace engine::input
{
    class elite_input
    {
    public:
        elite_input();
        ~elite_input();

        elite_input(const elite_input&) = delete;
        elite_input& operator=(const elite_input&) = delete;

        void refresh();
        control_button paddles();
        bool available() const;

    private:
        struct state;
        std::unique_ptr<state> state_;
    };
}
