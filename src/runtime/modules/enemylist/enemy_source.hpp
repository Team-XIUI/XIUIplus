#pragma once

#include "Ashita.h"
#include "combat/action_packet.hpp"
#include "entities/entity_name_colors.hpp"

#include <string>
#include <unordered_set>
#include <vector>

namespace xiui
{
    struct enemy_entry
    {
        uint32_t id{}, index{}, hp_percent{};
        D3DCOLOR name_color{unclaimed_name_color};
        std::string name{}, target_name{}, cast_name{};
        float distance{}, cast_seconds{};
        uint64_t evidence_at{}, action_at{}, cast_started{}, interrupted_at{};
        uint32_t last_target{}, cast_id{};
        bool claimed{}, selected{}, attacking_player{}, hostile{};

        auto HasCast(uint64_t now) const -> bool
        {
            return !this->cast_name.empty() && (this->interrupted_at != 0
                ? now - this->interrupted_at < 1000
                : now - this->cast_started < static_cast<uint64_t>((this->cast_seconds + 2.0f) * 1000.0f));
        }
    };

    class enemy_source
    {
    public:
        static constexpr size_t capacity = 32;
        auto Clear(void) -> void;
        auto Update(IAshitaCore* core, uint64_t now, bool includeSelected) -> void;
        auto HandlePacket(IAshitaCore* core, uint16_t id, std::span<const uint8_t> data, uint64_t now, const combat_action_packet* decoded = nullptr) -> void;
        auto Entries(void) const -> const std::vector<enemy_entry>& { return this->entries_; }

    private:
#if defined(XIUI_ENABLE_PARITY_TESTS)
        friend struct module_parity_test;
        friend struct enemy_list_layout_test;
#endif
#if defined(XIUI_ENABLE_INPUT_TESTS)
        friend struct controller_module_test;
#endif
        std::vector<enemy_entry> entries_{};
        std::unordered_set<uint32_t> group_{};
        uint32_t player_id_{}, zone_id_{};
        uint64_t next_scan_{};
        auto RefreshGroup(IParty& party, IEntity& entity) -> void;
        auto RefreshSession(IParty& party, IPlayer& player, IEntity& entity) -> bool;
        auto Find(uint32_t id) -> enemy_entry*;
        auto Add(uint32_t id, uint32_t index, uint64_t now) -> enemy_entry*;
        static auto FindIndex(IAshitaCore& core, uint32_t id) -> uint32_t;
        static auto IsLiveMonster(IEntity& entity, uint32_t index) -> bool;
    };
} // namespace xiui
