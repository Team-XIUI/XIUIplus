#pragma once

#include "Ashita.h"
#include "action_packet.hpp"
#include "server_rules.hpp"
#include "song_bonuses.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace xiui
{
    struct combat_status
    {
        uint16_t id{}, spell{};
        uint64_t observed{}, expires{};
        bool uncertain{};
        bool estimated{};
        uint64_t earliest{};
    };

    struct combat_cast
    {
        std::string name{};
        uint64_t started{}, interrupted{};
        float seconds{};
        auto Active(uint64_t now) const -> bool;
        auto Ratio(uint64_t now) const -> float;
    };

    struct combat_actor
    {
        uint32_t id{}, index{}, target{};
        uint64_t updated{}, targeted{};
        combat_cast cast{};
        std::vector<combat_status> statuses{};
    };

    auto FindEntityIndex(IAshitaCore& core, uint32_t id) -> uint32_t;
    auto IsStatusGain(uint32_t message) -> bool;
    auto IsStatusLoss(uint32_t message) -> bool;

    class combat_source
    {
    public:
        static constexpr size_t capacity = 128, status_capacity = 32;
        auto Clear(void) -> void;
        auto SetRules(const server_rules rules) -> void
        {
            if (this->rules_ == rules) return;
            this->Clear();
            this->rules_ = rules;
        }
        auto SetSongBonuses(const song_bonuses& value) -> void { this->songs_ = value; }
        auto Update(IAshitaCore* core, uint64_t now) -> void;
        auto HandlePacket(IAshitaCore* core, uint16_t id, std::span<const uint8_t> data, uint64_t now, const combat_action_packet* decoded = nullptr) -> void;
        auto Observe(const combat_action& action, IResourceManager* resources, uint64_t now) -> void;
        auto ApplyStatus(uint32_t target, uint32_t status, uint32_t spell, uint32_t message, uint64_t now, uint32_t seconds = 0) -> void;
        auto Find(uint32_t id) const -> const combat_actor*;
        auto Prune(uint64_t now) -> void;
        auto Count(void) const -> size_t { return this->actors_.size(); }

    private:
        std::unordered_map<uint32_t, combat_actor> actors_{};
        uint32_t player_{}, zone_{};
        uint64_t next_update_{};
        server_rules rules_{};
        song_bonuses songs_{};
        auto Actor(uint32_t id, uint64_t now) -> combat_actor&;
    };
}
