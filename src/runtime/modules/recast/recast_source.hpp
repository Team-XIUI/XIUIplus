#pragma once

#include "ashita_compat.hpp"

#include "Ashita.h"
#include "recast_tracker.hpp"
#include <array>
#include <unordered_set>

namespace xiui
{
    class recast_source
    {
    public:
        auto Clear(void) -> void { *this = {}; }
        auto Update(IAshitaCore* core, uint64_t now) -> void;
        auto Available(void) const -> bool { return this->available_; }
        auto Job(void) const -> uint8_t { return this->job_; }
        auto Known(const std::string& name) const -> bool { return this->known_names_.contains(name); }
        auto KnownTimer(uint32_t timer) const -> bool { return timer < this->known_timers_.size() && this->known_timers_[timer]; }
        auto Entries(void) const -> std::span<const recast_entry> { return this->tracker_.Entries(); }

    private:
#if defined(XIUI_ENABLE_PARITY_TESTS)
        friend struct pet_hud_test;
#endif
        recast_tracker tracker_{};
        std::unordered_set<std::string> known_names_{};
        std::array<bool, 256> known_timers_{};
        uint64_t next_update_{};
        uint32_t player_{}, zone_{};
        uint8_t job_{}, subjob_{}, level_{}, sublevel_{};
        bool available_{};
        auto AbilityName(IResourceManager* resources, ashita::player_view player, uint32_t timer) const -> std::string;
    };
}
