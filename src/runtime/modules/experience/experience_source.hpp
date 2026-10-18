#pragma once

#include "combat/action_packet.hpp"

#include "Ashita.h"
#include "experience_packets.hpp"
#include "settings/settings_store.hpp"

#include <map>

namespace xiui
{
    struct experience_effect
    {
        bool active{};
        uint64_t deadline{};
        auto Remaining(uint64_t now) const -> std::optional<uint32_t>
        { return active && deadline > now ? std::optional{static_cast<uint32_t>((deadline - now + 999) / 1000)} : std::nullopt; }
    };
    struct experience_ring
    {
        uint32_t id{};
        const char* name{};
        int charges{-1};
        std::optional<uint32_t> recast{};
    };

    class experience_source
    {
    public:
        experience_tracker tracker{};
        experience_effect food{}, dedication{};
        experience_ring ring{};
        bool ready{}, sync_active{};
        std::optional<uint32_t> sync_penalty{}, dedication_pool{};
        std::string no_exp_reason{};
        bool food_ended{}, dedication_ended{};
        auto Update(IAshitaCore* core, uint64_t now) -> void;
        auto HandlePacket(IAshitaCore* core, uint16_t id, std::span<const uint8_t> data, uint64_t now, const combat_action_packet* decoded = nullptr) -> void;
        auto Load(const settings_store& store) -> void;
        auto Save(const settings_store& store) const -> void;
        auto Utc(void) const -> uint32_t { return this->utc_; }

    private:
#if defined(XIUI_ENABLE_PARITY_TESTS)
        friend struct module_parity_test;
#endif
        uintptr_t time_slot_{};
        bool initialized_{}, pool_invalidated_{}, food_consumed_{}, dedication_consumed_{};
        uint64_t next_scan_{}, next_ring_{}, report_at_{}, reason_until_{}, last_status_sequence_{}, last_chain_sequence_{};
        uint32_t utc_{}, loaded_player_{};
        std::optional<momentum_report> report_{};
        std::optional<settings_store> restore_{};
        struct saved_character { experience_record record{}; std::deque<experience_record> history{}; bool paused{}; };
        std::map<uint32_t, saved_character> characters_{};
        std::optional<experience_chain> restored_chain_{};
        uint32_t restored_chain_zone_{};
        auto InitializeClock(void) -> void;
        auto ScanRings(const IInventory& inventory) -> void;
        auto LoadCharacter(uint32_t player) -> void;
    };
}
