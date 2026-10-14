#pragma once

#include "Ashita.h"
#include "world_state.hpp"

namespace xiui
{
    class world_source
    {
    public:
        auto Initialize(void) -> void;
        auto Update(IAshitaCore* core, uint64_t now) -> void;
        auto State(void) const -> const world_state& { return this->state_; }

    private:
        uintptr_t time_function_{}, weather_address_{}, compass_rotation_function_{};
        uint64_t next_environment_{}, next_markers_{};
        world_state state_{};
    };
}
