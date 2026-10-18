#pragma once

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
    };

    auto PhantomRolls(int rules) -> std::span<const phantom_roll_data>;
    auto PhantomRollByAbility(uint32_t ability, int rules) -> const phantom_roll_data*;
    auto PhantomRollByStatus(uint16_t status, int rules) -> const phantom_roll_data*;
    auto PhantomRollBasePotency(const phantom_roll_data& roll, uint8_t total, uint8_t level) -> std::optional<float>;
    auto PhantomRollPotencyText(const phantom_roll_data* roll, uint8_t total, uint8_t level, bool busted) -> std::string;
    inline auto PhantomBustChance(const uint8_t total) -> float
    { return total >= 1 && total <= 11 ? (total > 5 ? total - 5 : 0) / 6.0f : 0; }
}
