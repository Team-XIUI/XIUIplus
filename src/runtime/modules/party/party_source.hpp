#pragma once

#include "Ashita.h"
#include "combat/action_packet.hpp"

#include <array>
#include <span>
#include <string>

namespace xiui
{
    struct party_cast
    {
        std::string name{};
        uint32_t spell_id{};
        uint64_t started{}, interrupted{};
        float seconds{};
        auto Active(uint64_t now) const -> bool;
    };

    struct party_member
    {
        uint32_t id{}, index{}, slot{}, hp{}, mp{}, tp{}, hp_max{}, mp_max{};
        uint16_t zone{};
        uint8_t hp_percent{}, mp_percent{}, main_job{}, main_level{}, sub_job{}, sub_level{}, master_level{};
        uint32_t status{};
        bool status_known{};
        bool preview{};
        bool active{}, in_zone{}, entity_valid{}, distance_known{}, leader{}, alliance_leader{}, sync{};
        bool selected{}, subtarget{}, hp_max_exact{}, mp_max_exact{};
        float distance{};
        std::string name{}, zone_name{};
        std::array<uint16_t, 32> buffs{};
        std::array<uint64_t, 32> buff_expires{};
        bool buffs_known{};
        size_t buff_count{};
        party_cast cast{};
    };

    auto DecodePartyBuffs(std::span<const uint8_t> icons, uint64_t mask, std::span<uint16_t> output) -> size_t;
    auto LocalStatusSnapshotReady(std::span<const int16_t> icons) -> bool;
    auto PartyStatusSnapshotReady(std::span<const uint8_t> icons, uint64_t mask) -> bool;
    auto PartyResourceMaximum(uint32_t value, uint32_t percent) -> uint32_t;
    auto LocalResourceMaximum(uint32_t value, uint32_t percent, uint32_t reported, uint32_t previous = 0) -> uint32_t;
    auto FormatPartyResource(uint32_t value, uint32_t percent, uint32_t maximum, bool exact, int mode) -> std::string;
    auto PartyJobName(uint32_t job) -> const char*;
    auto PartyJobHasMP(uint32_t job) -> bool;
    auto ReadNativePartyCursor(uintptr_t root) -> int;
    inline auto NativeStatusDeadline(const uint32_t timer, const uint32_t utc, const uint64_t now) -> uint64_t
    {
        if (utc <= 0x3C307D70u || timer == 0 || timer == 0x7FFFFFFF) return 0;
        const auto ticks = static_cast<int32_t>(timer - (utc - 0x3C307D70u) * 60u);
        return ticks > 0 && ticks <= 604800 * 60 ? now + static_cast<uint64_t>(ticks) * 1000 / 60 : 0;
    }

    class party_source
    {
    public:
        auto Clear(void) -> void;
        auto Update(IAshitaCore* core, uint64_t now) -> void;
        auto SetUtc(uint32_t utc, uint64_t sampledAt) -> void { this->utc_ = utc; this->utc_sampled_at_ = sampledAt; }
        auto Resources(void) const -> IResourceManager* { return this->resources_; }
        auto HandlePacket(IAshitaCore* core, uint16_t id, std::span<const uint8_t> data, uint64_t now, const combat_action_packet* decoded = nullptr) -> void;
        auto Members(void) const -> const std::array<party_member, 18>& { return this->members_; }
        auto Count(size_t group) const -> size_t;
        auto InAlliance(void) const -> bool { return this->Count(1) != 0 || this->Count(2) != 0; }
        auto InCutscene(void) const -> bool { return this->cutscene_; }
        auto Available(void) const -> bool { return this->player_id_ != 0; }

    private:
#if defined(XIUI_ENABLE_PARITY_TESTS)
        friend struct module_parity_test;
        friend struct status_module_test;
        friend struct status_display_test;
        friend struct player_resource_test;
#endif
#if defined(XIUI_ENABLE_INPUT_TESTS)
        friend struct config_input_test;
        friend struct controller_module_test;
        friend struct controller_runtime_test;
#endif
        std::array<party_member, 18> members_{};
        uint32_t player_id_{}, zone_{};
        uint64_t next_update_{};
        bool cutscene_{};
        IResourceManager* resources_{};
        uint32_t utc_{};
        uint64_t utc_sampled_at_{};
        auto UpdateSelection(uint32_t main, uint32_t secondary, bool subtarget, int partyCursor) -> void;
    };
}
