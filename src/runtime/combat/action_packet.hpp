#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace xiui
{
    struct action_result
    {
        uint32_t parameter{}, message{}, additional_parameter{}, additional_message{}, spikes_parameter{}, spikes_message{};
    };

    struct action_target
    {
        uint32_t id{}, parameter{}, message{};
        size_t result_count{};
        std::array<action_result, 15> results{};
    };

    struct combat_action
    {
        uint32_t actor{}, category{}, parameter{};
        size_t target_count{};
        std::array<action_target, 63> targets{};
    };

    inline auto DecodeCombatAction(std::span<const uint8_t> data) -> std::optional<combat_action>
    {
        size_t position = 40;
        bool valid = data.size() >= 19;
        const auto read = [&](unsigned bits) -> uint32_t {
            if (!valid || position + bits > data.size() * 8) { valid = false; return 0; }
            uint32_t value{};
            for (unsigned i = 0; i < bits; ++i, ++position)
                value |= uint32_t{(data[position / 8] >> (position % 8)) & 1u} << i;
            return value;
        };
        combat_action result{};
        result.actor = read(32);
        result.target_count = read(6);
        read(4);
        result.category = read(4);
        result.parameter = read(32);
        read(32);
        if (result.actor == 0 || result.target_count == 0)
            return std::nullopt;
        for (size_t target = 0; target < result.target_count && valid; ++target)
        {
            auto& entry = result.targets[target];
            entry.id = read(32);
            const auto actions = read(4);
            if (actions == 0) return std::nullopt;
            entry.result_count = actions;
            for (uint32_t action = 0; action < actions && valid; ++action)
            {
                auto& outcome = entry.results[action];
                read(5); read(12); read(7); read(3);
                const auto parameter = read(17), message = read(10);
                outcome.parameter = parameter; outcome.message = message;
                if (action == 0) { entry.parameter = parameter; entry.message = message; }
                read(31);
                if (read(1)) { read(10); outcome.additional_parameter = read(17); outcome.additional_message = read(10); }
                if (read(1)) { read(10); outcome.spikes_parameter = read(14); outcome.spikes_message = read(10); }
            }
        }
        return valid ? std::optional{result} : std::nullopt;
    }
} // namespace xiui
