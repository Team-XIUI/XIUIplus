#pragma once

#include "combat_timing_source.hpp"
#include "modules/party/party_source.hpp"

#include <unordered_map>

namespace xiui
{
    class party_timing_source
    {
    public:
        auto Clear(void) -> void { *this = {}; }
        auto Update(std::span<const party_member> members, uint32_t player, const combat_timing_source* local, uint64_t now) -> void;
        auto Observe(const combat_action& action, uint64_t now) -> void;
        auto Swing(uint32_t member, double now) const -> attack_prediction;
        auto Resting(uint32_t member, double now) const -> attack_prediction;
        auto Full(uint32_t member) const -> bool;
        auto IsPlayer(uint32_t member) const -> bool { return member != 0 && member == this->player_; }

    private:
        struct member_clock
        {
            attack_clock attack{};
            recovery_clock recovery{};
            recovery_sample sample{};
            uint32_t index{};
            uint16_t zone{};
            uint64_t effects{};
            double last_round{};
            float interval{4};
            bool initialized{}, engaged{}, resting{}, full{};
        };
        std::unordered_map<uint32_t, member_clock> members_{};
        const combat_timing_source* local_{};
        uint32_t player_{};
        uint64_t previous_update_{}, next_update_{};
    };
}
