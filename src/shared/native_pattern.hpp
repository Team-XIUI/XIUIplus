#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

namespace xiui
{
    // Searches readable module memory and rejects missing or ambiguous signatures.
    inline auto FindUniquePattern(const uintptr_t base, const size_t size, const std::string_view pattern) -> uintptr_t
    {
        if (base == 0 || pattern.empty() || pattern.size() % 2 != 0 || pattern.size() / 2 > size) return 0;
        struct literal_run { size_t offset; std::vector<uint8_t> bytes; };
        std::vector<literal_run> runs;
        const auto hex = [](const char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            return -1;
        };
        for (size_t at = 0; at < pattern.size(); at += 2)
        {
            if (pattern.substr(at, 2) == "??") continue;
            const auto high = hex(pattern[at]), low = hex(pattern[at + 1]);
            if (high < 0 || low < 0) return 0;
            if (runs.empty() || runs.back().offset + runs.back().bytes.size() != at / 2)
                runs.push_back({at / 2, {}});
            runs.back().bytes.push_back(static_cast<uint8_t>((high << 4) | low));
        }
        const auto positions = size - pattern.size() / 2 + 1;
        if (runs.empty()) return positions == 1 ? base : 0;
        const auto& anchor = *std::max_element(runs.begin(), runs.end(), [](const auto& a, const auto& b) { return a.bytes.size() < b.bytes.size(); });
        const auto* cursor = reinterpret_cast<const uint8_t*>(base) + anchor.offset;
        const auto* end = cursor + positions;
        uintptr_t found{};
        while (cursor < end)
        {
            const auto* hit = static_cast<const uint8_t*>(std::memchr(cursor, anchor.bytes.front(), end - cursor));
            if (hit == nullptr) break;
            const auto* candidate = hit - anchor.offset;
            if (std::memcmp(hit, anchor.bytes.data(), anchor.bytes.size()) == 0 &&
                std::all_of(runs.begin(), runs.end(), [&](const auto& run) {
                    return &run == &anchor || std::memcmp(candidate + run.offset, run.bytes.data(), run.bytes.size()) == 0;
                }))
            {
                if (found != 0) return 0;
                found = reinterpret_cast<uintptr_t>(candidate);
            }
            cursor = hit + 1;
        }
        return found;
    }
}
