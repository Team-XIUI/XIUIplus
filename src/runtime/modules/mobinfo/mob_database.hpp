#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace xiui
{
#pragma pack(push, 1)
    struct mob_record
    {
        uint32_t id{}, name_offset{}, respawn{}, placeholder_of{}, immunity{};
        uint16_t zone{}, detects{};
        int16_t exp_level_mod{};
        std::array<int16_t, 12> damage{};
        std::array<int16_t, 4> damage_taken{}; // physical, magic, ranged, breath
        std::array<int8_t, 8> elements{};
        std::array<int8_t, 10> statuses{};
        uint8_t minimum_level{}, maximum_level{}, flags{}, main_job{}, sub_job{};
        uint8_t traits{};     // regen, refresh, regain, shadows, counter bits
        uint8_t add_effect{}; // build_mob_data.py ADD_EFFECTS index + 1; 0 is none
        uint8_t absorbs{};    // element bits in damage order
    };
#pragma pack(pop)
    static_assert(sizeof(mob_record) == 84);

    inline constexpr const char* mob_data_source = "Phoenix 11c6ee5 / HeadsUp 161ad60 (2026-10-16)";
    auto MobById(uint32_t serverId) -> const mob_record*;
    auto MobForEntity(uint32_t serverId, uint16_t zone, bool monster) -> const mob_record*;
    auto MobDataName(const mob_record& mob) -> std::string_view;
    auto MobLevel(const mob_record& mob) -> std::string;
    auto MobJobs(const mob_record& mob) -> std::string;
    // English catalog key for the additional effect; nullptr when none.
    auto MobAddEffectName(const mob_record& mob) -> const char*;
    auto MobDetection(const mob_record& mob) -> std::string;
    auto MobImmunities(const mob_record& mob) -> std::string;
    auto MobSpawnHints(uint32_t serverId) -> std::vector<std::string>;
    auto MobIsPlaceholder(uint32_t serverId) -> bool;
}
