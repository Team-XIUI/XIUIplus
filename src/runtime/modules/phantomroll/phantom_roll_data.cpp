#include "phantom_roll_data.hpp"
#include "localization/localization.hpp"
#include "resources/phoenix_roll_data.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
    // Base values from XIUI modules/phantomroll/data.lua (LSB retail/Horizon tables).
    constexpr std::array<xiui::phantom_roll_data, 31> retail{{
        {"Fighter's", 98, 310, 5, 9, "Double Atk.", true, false, false, 1, {2, 2, 3, 4, 12, 5, 6, 7, 1, 9, 18, 6}, 1, 6},
        {"Monk's", 99, 311, 3, 7, "Subtle Blow", true, false, false, 1, {8, 10, 32, 12, 14, 16, 4, 20, 22, 24, 40, 11}, 2, 10},
        {"Healer's", 100, 312, 3, 7, "Cure Pot.", true, false, false, 1, {3, 4, 12, 5, 6, 7, 1, 8, 9, 10, 16, 4}, 3, 4},
        {"Wizard's", 101, 313, 5, 9, "MAB", false, false, false, 1, {4, 6, 8, 10, 25, 12, 14, 17, 2, 20, 30, 10}, 4, 10},
        {"Warlock's", 102, 314, 4, 8, "MACC", false, false, false, 1, {2, 3, 4, 12, 5, 6, 7, 1, 8, 9, 15, 5}, 5, 5},
        {"Rogue's", 103, 315, 5, 9, "Crit Rate", true, false, false, 1, {2, 2, 3, 4, 12, 5, 6, 6, 1, 8, 19, 6}, 6, 6},
        {"Gallant's", 104, 316, 3, 7, "Dmg Taken", true, false, false, 100, {600, 800, 2400, 900, 1100, 1200, 300, 1500, 1700, 1800, 3000, 500}, 7, 500},
        {"Chaos", 105, 317, 4, 8, "ATK", true, false, false, 1, {6, 8, 9, 25, 11, 13, 16, 3, 17, 19, 31, 10}, 8, 10},
        {"Beast", 106, 318, 4, 8, "Pet ATK", true, false, false, 1, {4, 5, 7, 19, 8, 9, 11, 2, 13, 14, 23, 7}, 9, 10},
        {"Choral", 107, 319, 2, 6, "Spell Intr.", true, false, false, 1, {13, 55, 17, 20, 25, 8, 30, 35, 40, 45, 65, 25}, 10, 25},
        {"Hunter's", 108, 320, 4, 8, "ACC", false, false, false, 1, {10, 13, 15, 40, 18, 20, 25, 5, 27, 30, 50, 5}, 11, 15},
        {"Samurai", 109, 321, 2, 6, "Store TP", false, false, false, 1, {8, 32, 10, 12, 14, 4, 16, 20, 22, 24, 40, 5}, 12, 10},
        {"Ninja", 110, 322, 4, 8, "EVA", false, false, false, 1, {4, 5, 5, 14, 6, 7, 9, 2, 10, 11, 18, 6}, 13, 6},
        {"Drachen", 111, 323, 4, 8, "Pet ACC", false, false, false, 1, {10, 13, 15, 40, 18, 20, 25, 5, 28, 30, 50, 15}, 14, 15},
        {"Evoker's", 112, 324, 5, 9, "Refresh", false, false, false, 1, {1, 1, 1, 1, 3, 2, 2, 2, 1, 3, 4, 1}, 15, 1},
        {"Magus's", 113, 325, 2, 6, "MDB", false, false, false, 1, {5, 20, 6, 8, 9, 3, 10, 13, 14, 15, 25, 5}, 16, 8},
        {"Corsair's", 114, 326, 5, 9, "EXP", true, false, false, 1, {10, 11, 11, 12, 20, 13, 15, 16, 8, 17, 24, 6}},
        {"Puppet", 115, 327, 3, 7, "Pet MAB", false, false, false, 1, {4, 5, 18, 7, 9, 10, 2, 11, 13, 15, 22, 8}, 18, 8},
        {"Dancer's", 116, 328, 3, 7, "Regen", false, false, false, 1, {3, 4, 12, 5, 6, 7, 1, 8, 9, 10, 16, 4}, 19, 4},
        {"Scholar's", 117, 329, 2, 6, "Conserve MP", false, false, false, 1, {2, 9, 3, 4, 5, 2, 6, 6, 7, 9, 14, 4}, 20, 4},
        {"Bolter's", 118, 330, 3, 9, "Move Speed", true, false, false, 1, {6, 6, 16, 8, 8, 10, 10, 12, 4, 14, 20, 0}},
        {"Caster's", 119, 331, 2, 7, "Fast Cast", true, false, false, 1, {6, 15, 7, 8, 9, 10, 5, 11, 12, 13, 20, -10}},
        {"Courser's", 120, 332, 3, 9, "Snapshot", true, false, false, 1, {2, 3, 11, 4, 5, 6, 7, 8, 1, 10, 12, -5}},
        {"Blitzer's", 121, 333, 4, 9, "Atk Delay", true, false, false, 1, {-2, -3, -4, -11, -5, -6, -7, -8, -1, -10, -12, 3}},
        {"Tactician's", 122, 334, 5, 8, "Regain", false, false, false, 1, {10, 10, 10, 10, 30, 10, 10, 0, 20, 20, 40, -10}},
        {"Allies'", 302, 335, 3, 10, "SC Dmg", true, false, false, 1, {2, 3, 20, 5, 7, 9, 11, 13, 15, 1, 25, -5}},
        {"Miser's", 303, 336, 5, 7, "Save TP", false, false, false, 1, {30, 50, 70, 90, 200, 110, 20, 130, 150, 170, 250, 0}},
        {"Companion's", 304, 337, 2, 10, "Pet Regen", false, true, false, 1, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0}},
        {"Avenger's", 305, 338, 4, 8, "Counter", true, false, false, 1, {2, 2, 3, 12, 4, 5, 6, 1, 7, 9, 18, 6}},
        {"Naturalist's", 390, 339, 3, 7, "Enh. Dur.", true, false, false, 1, {6, 7, 15, 8, 9, 10, 5, 11, 12, 13, 20, -5}, 21, 5},
        {"Runeist's", 391, 600, 4, 8, "MEVA", false, false, false, 1, {4, 6, 8, 25, 10, 12, 14, 2, 17, 20, 30, -10}, 22, 7},
    }};
    constexpr std::array<xiui::phantom_roll_data, 31> horizon{{
        {"Fighter's", 98, 310, 5, 9, "Double Atk.", true, false, false, 1, {2, 2, 3, 4, 12, 5, 6, 6, 1, 9, 18, -6}},
        {"Monk's", 99, 311, 3, 7, "Subtle Blow", true, false, false, 1, {8, 10, 32, 12, 14, 16, 4, 20, 22, 24, 40, -11}},
        {"Healer's", 100, 312, 3, 7, "hMP", false, false, false, 1, {2, 3, 10, 4, 4, 5, 1, 6, 6, 7, 12, -3}},
        {"Wizard's", 101, 313, 5, 9, "MAB", false, false, false, 1, {2, 3, 4, 4, 10, 5, 6, 7, 1, 7, 12, -4}},
        {"Warlock's", 102, 314, 4, 8, "MACC", false, false, false, 1, {2, 3, 4, 12, 5, 6, 7, 1, 8, 9, 15, -5}},
        {"Rogue's", 103, 315, 5, 9, "Crit Rate", true, false, false, 1, {2, 2, 3, 4, 12, 5, 6, 6, 1, 8, 19, -6}},
        {"Gallant's", 104, 316, 3, 7, "DEF", false, false, false, 1, {48, 60, 200, 72, 88, 104, 32, 120, 140, 160, 240, -120}},
        {"Chaos", 105, 317, 4, 8, "ATK", false, false, true, 1, {29, 36, 39, 92, 45, 54, 61, 21, 64, 71, 111, -15}},
        {"Beast", 106, 318, 4, 8, "Pet ATK", true, false, false, 1, {4, 5, 7, 19, 8, 9, 11, 2, 13, 14, 23, 7}},
        {"Choral", 107, 319, 2, 6, "Spell Intr.", true, false, false, 1, {-13, -55, -17, -20, -25, -8, -30, -35, -40, -45, -65, 25}},
        {"Hunter's", 108, 320, 4, 8, "ACC", false, false, false, 1, {10, 13, 15, 40, 18, 20, 25, 5, 27, 30, 50, -15}},
        {"Samurai", 109, 321, 2, 6, "Store TP", false, false, false, 1, {8, 32, 10, 12, 14, 4, 16, 20, 22, 24, 40, -5}},
        {"Ninja", 110, 322, 4, 8, "EVA", false, false, false, 1, {10, 13, 15, 40, 18, 20, 25, 5, 27, 30, 50, -15}},
        {"Drachen", 111, 323, 4, 8, "Pet ACC", false, false, false, 1, {10, 13, 15, 40, 18, 20, 25, 5, 27, 30, 50, 0}},
        {"Evoker's", 112, 324, 5, 9, "Refresh", false, false, false, 1, {1, 1, 1, 1, 3, 2, 2, 2, 1, 2, 4, -1}},
        {"Magus's", 113, 325, 2, 6, "MDB", false, false, false, 1, {5, 20, 6, 8, 9, 3, 10, 13, 14, 15, 25, -5}},
        {"Corsair's", 114, 326, 5, 9, "EXP", true, false, false, 1, {10, 11, 11, 12, 20, 13, 15, 16, 8, 17, 24, -6}},
        {"Puppet", 115, 327, 3, 7, "Pet MAB", false, false, false, 1, {4, 5, 18, 7, 9, 10, 2, 11, 13, 15, 22, -8}},
        {"Dancer's", 116, 328, 3, 7, "Regen", false, false, false, 1, {3, 4, 12, 5, 6, 7, 1, 8, 9, 10, 16, 4}},
        {"Scholar's", 117, 329, 2, 6, "Conserve MP", false, false, false, 1, {2, 9, 3, 4, 5, 2, 6, 6, 7, 9, 14, 4}},
        {"Bolter's", 118, 330, 3, 9, "Move Speed", true, false, false, 1, {6, 6, 16, 8, 8, 10, 10, 12, 4, 14, 20, 0}},
        {"Caster's", 119, 331, 2, 7, "Fast Cast", true, false, false, 1, {6, 15, 7, 8, 9, 10, 5, 11, 12, 13, 20, -10}},
        {"Courser's", 120, 332, 3, 9, "Snapshot", true, false, false, 1, {2, 3, 11, 4, 5, 6, 7, 8, 1, 10, 12, -5}},
        {"Blitzer's", 121, 333, 4, 9, "Atk Delay", true, false, false, 1, {-2, -3, -4, -11, -5, -6, -7, -8, -1, -10, -12, 3}},
        {"Tactician's", 122, 334, 5, 8, "Regain", false, false, false, 1, {10, 10, 10, 10, 30, 10, 10, 0, 20, 20, 40, -10}},
        {"Allies'", 302, 335, 3, 10, "SC Dmg", true, false, false, 1, {2, 3, 20, 5, 7, 9, 11, 13, 15, 1, 25, -5}},
        {"Miser's", 303, 336, 5, 7, "Save TP", false, false, false, 1, {30, 50, 70, 90, 200, 110, 20, 130, 150, 170, 250, 0}},
        {"Companion's", 304, 337, 2, 10, "Pet Regen", false, true, false, 1, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0}},
        {"Avenger's", 305, 338, 4, 8, "Counter", true, false, false, 1, {2, 2, 3, 12, 4, 5, 6, 1, 7, 9, 18, 6}},
        {"Naturalist's", 390, 339, 3, 7, "Enh. Dur.", true, false, false, 1, {6, 7, 15, 8, 9, 10, 5, 11, 12, 13, 20, -5}},
        {"Runeist's", 391, 600, 4, 8, "MEVA", false, false, false, 1, {4, 6, 8, 25, 10, 12, 14, 2, 17, 20, 30, -10}},
    }};
}

namespace xiui
{
    auto PhantomRolls(const server_rules rules) -> std::span<const phantom_roll_data>
    {
        switch (rules)
        {
        case server_rules::phoenix: return resources::phoenix_rolls;
        case server_rules::horizon: return horizon;
        default: return retail;
        }
    }

    auto PhantomRollByAbility(const uint32_t ability, const server_rules rules) -> const phantom_roll_data*
    {
        for (const auto& roll : PhantomRolls(rules))
        {
            if (roll.ability == ability) return &roll;
        }
        return nullptr;
    }

    auto PhantomRollByStatus(const uint16_t status, const server_rules rules) -> const phantom_roll_data*
    {
        for (const auto& roll : PhantomRolls(rules))
        {
            if (roll.status == status) return &roll;
        }
        return nullptr;
    }

    auto PhantomRollHasJobBonus(const phantom_roll_data& roll, const std::span<const uint8_t> partyJobs) -> bool
    { return roll.bonus_job != 0 && roll.bonus != 0 && std::find(partyJobs.begin(), partyJobs.end(), roll.bonus_job) != partyJobs.end(); }

    auto PhantomRollBasePotency(const phantom_roll_data& roll, const uint8_t total, const uint8_t level, const bool jobBonus, const uint8_t rollerLevel) -> std::optional<float>
    {
        if (roll.unknown || total < 1 || total > 11 || roll.scale <= 0 || (roll.per_level && level == 0)) return std::nullopt;
        float value = roll.powers[total - 1] + (jobBonus ? roll.bonus : 0);
        if (rollerLevel != 0 && rollerLevel < level) value = std::ceil(value * rollerLevel / level);
        if (roll.per_level) value = std::floor(value * level / 75.0f);
        return value / roll.scale;
    }

    auto PhantomRollPotencyText(const phantom_roll_data* roll, const uint8_t total, const uint8_t level, const bool busted, const bool jobBonus, const uint8_t rollerLevel) -> std::string
    {
        if (busted) return i18n::Text("Bust penalty: unknown");
        const bool bonus = jobBonus && roll != nullptr && roll->bonus_job != 0 && roll->bonus != 0;
        const auto value = roll != nullptr ? PhantomRollBasePotency(*roll, total, level, bonus, rollerLevel) : std::nullopt;
        if (!value) return i18n::Text("Potency: unknown");
        const auto number = [&](const float amount) {
            char text[32]{};
            std::snprintf(text, sizeof(text), std::abs(amount - std::round(amount)) < 0.05f ? "%+.0f%s" : "%+.1f%s", static_cast<double>(amount), roll->percent ? "%" : "");
            return std::string{text};
        };
        if (!bonus) return i18n::Format("Base %s %s", number(*value).c_str(), roll->stat);
        const auto plain = PhantomRollBasePotency(*roll, total, level, false, rollerLevel);
        return i18n::Format("%s %s (job bonus %s)", number(*value).c_str(), roll->stat, number(*value - plain.value_or(*value)).c_str());
    }
}
