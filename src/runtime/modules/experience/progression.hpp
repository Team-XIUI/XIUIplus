#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>

namespace xiui
{
    enum class progression_kind { experience, limit, capacity, mastery };
    enum class progression_mode { automatic, experience, limit, capacity, mastery };

    struct progression_input
    {
        uint32_t job{}, level{}, exp{}, exp_needed{1}, limit{}, merits{}, merit_max{}, capacity{}, job_points{}, spent{};
        uint32_t mastery_job{}, mastery{}, mastery_needed{}, master_level{};
        bool limit_mode{}, exp_locked{}, master_breaker{};
    };

    struct progression_state
    {
        progression_kind kind{};
        uint32_t current{}, needed{1}, points{}, points_max{}, level{};
        bool available{true};
        auto operator==(const progression_state&) const -> bool = default;
        auto Label(void) const -> const char*
        {
            switch (this->kind) {
            case progression_kind::limit: return "LIMIT";
            case progression_kind::capacity: return "CAPACITY";
            case progression_kind::mastery: return "EXEMPLAR";
            default: return "EXP";
            }
        }
        auto Name(void) const -> const char*
        {
            switch (this->kind) {
            case progression_kind::limit: return "Limit Points";
            case progression_kind::capacity: return "Capacity Points";
            case progression_kind::mastery: return "Exemplar Points";
            default: return "Experience Points";
            }
        }
        auto UnavailableReason(void) const -> const char*
        {
            switch (this->kind) {
            case progression_kind::limit: return "Requires level 75";
            case progression_kind::capacity: return "Requires level 99";
            case progression_kind::mastery: return "Mastery not unlocked for this job";
            default: return "Waiting for progression data";
            }
        }
    };

    inline auto SelectProgression(const progression_input& input, progression_mode mode = progression_mode::automatic) -> progression_state
    {
        const bool mastery = input.level >= 99 && input.spent >= 2100 && input.master_breaker && input.mastery_job == input.job && input.mastery_needed > 0;
        if (mode == progression_mode::experience)
            return {progression_kind::experience, input.exp, std::max(1u, input.exp_needed), 0, 0, input.level};
        if (mode == progression_mode::limit)
            return {progression_kind::limit, input.limit, 10000, input.merits, input.merit_max, input.level, input.level >= 75};
        if (mode == progression_mode::capacity)
            return {progression_kind::capacity, input.capacity, 30000, input.job_points, 500, input.level, input.level >= 99};
        if (mode == progression_mode::mastery)
            return {progression_kind::mastery, mastery ? input.mastery : 0, mastery ? input.mastery_needed : 0, 0, 0, mastery ? input.master_level : 0, mastery};
        if (mastery)
            return {progression_kind::mastery, input.mastery, input.mastery_needed, 0, 0, input.master_level};
        if (input.level >= 75 && (input.limit_mode || (input.exp_locked && input.level < 99) ||
            (input.level == 75 && input.merit_max > 0 && input.exp_needed > 1 && input.exp >= input.exp_needed - 1)))
            return {progression_kind::limit, input.limit, 10000, input.merits, input.merit_max, input.level};
        if (input.level >= 99)
            return {progression_kind::capacity, input.capacity, 30000, input.job_points, 500, input.level};
        return {progression_kind::experience, input.exp, std::max(1u, input.exp_needed), 0, 0, input.level};
    }

    inline auto FormatProgressionInfo(const progression_state& progress, const bool current, const bool percent, const bool remaining,
        const bool points, const bool masterLevel) -> std::string
    {
        if (!progress.available) return progress.UnavailableReason();
        std::string info;
        const auto add = [&info](const char* text) { if (!info.empty()) info += "   "; info += text; };
        char value[64]{};
        if (current) { std::snprintf(value, sizeof(value), "%u / %u", progress.current, progress.needed); add(value); }
        if (points && (progress.kind == progression_kind::limit || progress.kind == progression_kind::capacity))
        {
            std::snprintf(value, sizeof(value), "%s %u/%u", progress.kind == progression_kind::limit ? "Merits" : "JP", progress.points, progress.points_max);
            add(value);
        }
        if (masterLevel && progress.kind == progression_kind::mastery) { std::snprintf(value, sizeof(value), "ML %u", progress.level); add(value); }
        if (percent)
        {
            const auto ratio = progress.needed > 0 ? std::clamp(static_cast<double>(progress.current) / progress.needed, 0.0, 1.0) : 0;
            std::snprintf(value, sizeof(value), "%.1f%%", ratio * 100); add(value);
        }
        if (remaining) { std::snprintf(value, sizeof(value), "%u left", progress.needed > progress.current ? progress.needed - progress.current : 0); add(value); }
        return info;
    }
}
