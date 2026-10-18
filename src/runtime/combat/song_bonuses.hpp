#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace xiui
{
    inline constexpr const char* song_bonus_keys[]{"lullaby", "requiem", "elegy", "threnody", "nocturne", "virelai"};
    inline constexpr const char* song_bonus_names[]{"Lullaby", "Requiem", "Elegy", "Threnody", "Nocturne", "Virelai"};
    using song_bonuses = std::array<float, 6>;

    inline auto SongFamily(const uint32_t spell) -> int
    {
        if (spell == 376 || spell == 377 || spell == 463 || spell == 471) return 0;
        if (spell >= 368 && spell <= 374) return 1;
        if (spell >= 421 && spell <= 423) return 2;
        if ((spell >= 454 && spell <= 461) || (spell >= 871 && spell <= 878)) return 3;
        if (spell == 472) return 4;
        return spell == 466 ? 5 : -1;
    }

    inline auto AdjustSongDuration(const uint32_t seconds, const uint32_t spell, const song_bonuses& bonuses) -> uint32_t
    {
        const auto family = SongFamily(spell);
        if (family < 0) return seconds;
        const auto value = bonuses[static_cast<size_t>(family)];
        const auto bonus = std::isfinite(value) ? std::clamp(value, 0.0f, 9.0f) : 0;
        return static_cast<uint32_t>(uint64_t{seconds} * (10 + static_cast<uint32_t>(bonus)) / 10);
    }
}
