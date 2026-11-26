#pragma once

#include "model.h"

#include <cstddef>
#include <future>
#include <optional>
#include <string>
#include <vector>

namespace menu::model
{
    enum class load_state
    {
        idle,
        loading,
        ready,
        failed,
        taken
    };

    class library
    {
    public:
        explicit library(std::size_t count);

        void request(std::size_t index);
        load_state state(std::size_t index);
        std::optional<model> take(std::size_t index);
        const std::string& error(std::size_t index) const;

    private:
        struct result
        {
            bool ok = false;
            model data;
            std::string error;
        };

        struct slot
        {
            load_state state = load_state::idle;
            std::future<result> pending;
            std::optional<model> data;
            std::string error;
        };

        std::vector<slot> slots_;
    };
}
