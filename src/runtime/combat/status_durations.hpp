#pragma once

#include "resources/xiui_status_durations.hpp"

#include <algorithm>
#include <span>

namespace xiui
{
    inline auto SpellStatusRules(const uint32_t spell) -> std::span<const resources::spell_status_rule>
    {
        const auto& rules = resources::xiui_spell_status_rules;
        const auto first = std::lower_bound(rules.begin(), rules.end(), spell,
            [](const auto& rule, const uint32_t id) { return rule.spell < id; });
        const auto last = std::upper_bound(first, rules.end(), spell,
            [](const uint32_t id, const auto& rule) { return id < rule.spell; });
        return {first, last};
    }

    inline auto SpellStatusRule(const uint32_t spell, const uint32_t status) -> const resources::spell_status_rule*
    {
        const auto rules = SpellStatusRules(spell);
        for (const auto& rule : rules)
            if (rule.status == status || rule.display == status || rule.status == 0) return &rule;
        return nullptr;
    }

    inline auto FindStatusDuration(const std::span<const resources::status_duration_rule> rules,
        const uint32_t action, const uint32_t status) -> uint32_t
    {
        auto entry = std::lower_bound(rules.begin(), rules.end(), action,
            [](const auto& rule, const uint32_t id) { return rule.action < id; });
        for (; entry != rules.end() && entry->action == action; ++entry)
            if (entry->status == 0 || entry->status == status) return entry->seconds;
        return 0;
    }

    inline auto XIUIStatusSeconds(const uint32_t category, const uint32_t argument, const uint32_t status,
        const uint32_t message) -> uint32_t
    {
        using namespace resources;
        if (message == 160 || message == 164 || message == 374)
            return FindStatusDuration(xiui_additional_durations, status, 0);
        const auto action = argument & 0xFFFF;
        const auto ability = action >= 512 && action < 1024 ? action - 512 : action;
        uint32_t seconds{};
        switch (category)
        {
        case 4: seconds = FindStatusDuration(xiui_spells_durations, action, status); break;
        case 3:
            if (message == 127 || message == 266 || message == 267 || message == 319 || message == 320)
                seconds = FindStatusDuration(xiui_ja_durations, ability, status);
            else seconds = FindStatusDuration(xiui_weaponSkills_durations, action, status);
            break;
        case 6: case 14: seconds = FindStatusDuration(xiui_ja_durations, ability, status); break;
        case 13: seconds = FindStatusDuration(xiui_pet_durations, action, status); break;
        case 11:
            seconds = FindStatusDuration(xiui_ja_durations, action, status);
            if (seconds == 0) seconds = FindStatusDuration(xiui_pet_durations, action, status);
            break;
        }
        return seconds != 0 ? seconds : FindStatusDuration(xiui_fallback_durations, status, 0);
    }
}
