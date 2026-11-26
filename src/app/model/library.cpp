#include "library.h"
#include "catalog.h"
#include "loader.h"
#include "asset_store.h"

namespace menu::model
{
    library::library(std::size_t count)
        : slots_(count)
    {
    }

    void library::request(std::size_t index)
    {
        slot& s = slots_[index];
        if (s.state != load_state::idle)
            return;

        const auto& entry = catalog()[index];
        const auto path = model_asset(entry);
        const auto max_texture_size = entry.max_texture_size;
        s.state = load_state::loading;
        s.pending = std::async(std::launch::async, [path, max_texture_size]
        {
            result r;
            r.ok = load(asset_bytes(path), max_texture_size, r.data, r.error);
            return r;
        });
    }

    load_state library::state(std::size_t index)
    {
        slot& s = slots_[index];
        if (s.state == load_state::loading && s.pending.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            result r = s.pending.get();
            if (r.ok)
            {
                s.data = std::move(r.data);
                s.state = load_state::ready;
            }
            else
            {
                s.error = std::move(r.error);
                s.state = load_state::failed;
            }
        }
        return s.state;
    }

    std::optional<model> library::take(std::size_t index)
    {
        slot& s = slots_[index];
        if (s.state == load_state::loading)
            s.pending.wait();
        if (state(index) != load_state::ready)
            return std::nullopt;

        s.state = load_state::taken;
        std::optional<model> out = std::move(s.data);
        s.data.reset();
        return out;
    }

    const std::string& library::error(std::size_t index) const
    {
        return slots_[index].error;
    }
}
