#pragma once

#include <cstdint>
#include <optional>

namespace xiui
{
    // Native radar colors and eligibility, independent of entity-name colors.
    inline auto CompassRadarColor(const uint32_t flags, const uint32_t status, const bool monstrosity,
        const bool enemies) -> std::optional<uint32_t>
    {
        if (flags == 0) return std::nullopt;
        if ((flags & 1) != 0 && !monstrosity) return (flags & 0x0C) != 0 ? 0xFF3060E0 : 0xFF0040F0;
        if ((flags & 0x100) != 0) return 0xFF30D000;
        if ((flags & 2) != 0) return status == 0x31 ? std::nullopt : std::optional<uint32_t>{0xFF00B040};
        if (!enemies) return std::nullopt;
        return status == 2 || status == 3 ? 0xFF206060 : 0xFF808080;
    }
}
