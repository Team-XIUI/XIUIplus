#pragma once

#include "modules/module.hpp"

#include <algorithm>
#include <vector>

namespace xiui::ui
{
    struct config_option_entry
    {
        size_t index{};
        int order{};
        bool pair_next{};
    };

    inline auto ArrangeConfigOptions(std::vector<config_option_entry> entries, const size_t empty, const int columns = 2) -> std::vector<size_t>
    {
        std::stable_sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) { return left.order < right.order; });
        std::vector<size_t> slots;
        slots.reserve(entries.size() + 4);
        for (size_t at = 0; at < entries.size(); ++at)
        {
            const auto& entry = entries[at];
            const bool paired = entry.pair_next && at + 1 < entries.size() && entries[at + 1].order == entry.order + 1;
            if (paired && columns > 1 && slots.size() % columns != 0) slots.push_back(empty);
            slots.push_back(entry.index);
        }
        return slots;
    }
}
