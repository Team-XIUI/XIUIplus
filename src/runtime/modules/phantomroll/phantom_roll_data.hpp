#pragma once

#include "combat/server_rules.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace xiui
{
    struct phantom_roll_data
    {
        const char* name{};
        uint16_t ability{}, status{};
        uint8_t lucky{}, unlucky{};
        const char* stat{};
        bool percent{}, unknown{}, per_level{};
        float scale{1};
        std::array<int16_t, 12> powers{};
        // Added to a non-bust total when a party member has this main job; 0 means no job bonus.
        uint8_t bonus_job{};
        int16_t bonus{};
    };

    auto PhantomRolls(server_rules rules) -> std::span<const phantom_roll_data>;
    auto PhantomRollByAbility(uint32_t ability, server_rules rules) -> const phantom_roll_data*;
    auto PhantomRollByStatus(uint16_t status, server_rules rules) -> const phantom_roll_data*;
    auto PhantomRollHasJobBonus(const phantom_roll_data& roll, std::span<const uint8_t> partyJobs) -> bool;
    // rollerLevel is the roller's Corsair level; below the recipient's level it scales the value down (0 leaves it unscaled).
    auto PhantomRollBasePotency(const phantom_roll_data& roll, uint8_t total, uint8_t level, bool jobBonus = false, uint8_t rollerLevel = 0) -> std::optional<float>;
    auto PhantomRollPotencyText(const phantom_roll_data* roll, uint8_t total, uint8_t level, bool busted, bool jobBonus = false, uint8_t rollerLevel = 0) -> std::string;
    inline auto PhantomBustChance(const uint8_t total) -> float
    { return total >= 1 && total <= 11 ? (total > 5 ? total - 5 : 0) / 6.0f : 0; }
}
