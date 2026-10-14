#pragma once

#include "Ashita.h"
#include "recast_tracker.hpp"

namespace xiui
{
    class recast_source
    {
    public:
        auto Clear(void) -> void { *this = {}; }
        auto Update(IAshitaCore* core, uint64_t now) -> void;
        auto Available(void) const -> bool { return this->available_; }
        auto Job(void) const -> uint8_t { return this->job_; }
        auto Entries(void) const -> std::span<const recast_entry> { return this->tracker_.Entries(); }

    private:
        recast_tracker tracker_{};
        uint64_t next_update_{};
        uint32_t player_{}, zone_{};
        uint8_t job_{}, subjob_{}, level_{}, sublevel_{};
        bool available_{};
        auto AbilityName(IResourceManager* resources, IPlayer* player, uint32_t timer) const -> std::string;
    };
}
