#pragma once

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace xiui
{
    inline constexpr size_t chat_text_capacity = 4096;
    inline constexpr size_t chat_draft_capacity = 512;
    inline constexpr size_t chat_history_capacity = 1024;

    struct chat_event
    {
        uint64_t sequence{};
        uint64_t timestamp{};
        uint32_t mode{};
        uint32_t color{0xFFFFFFFF};
        uint32_t windows{};
        bool indent{};
        char text[chat_text_capacity]{};
    };

    struct chat_palette
    {
        bool available{};
        std::array<uint32_t, 256> modes{};
        std::array<uint32_t, 512> colors{};
    };

    inline auto ChatColor(const uint32_t native) -> uint32_t
    {
        const auto peak = std::max({128u, (native >> 16) & 255u, (native >> 8) & 255u, native & 255u});
        const auto channel = [native, peak](const uint32_t shift) {
            return ((((native >> shift) & 255u) * 255u / peak) << shift);
        };
        return 0xFF000000 | channel(16) | channel(8) | channel(0);
    }
} // namespace xiui
