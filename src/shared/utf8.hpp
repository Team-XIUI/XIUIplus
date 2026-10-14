#pragma once

#include <cstdint>
#include <string_view>

namespace xiui
{
    inline auto NextUtf8(std::string_view text, size_t& position) -> uint32_t
    {
        const auto first = static_cast<uint8_t>(text[position++]);
        if (first < 0x80) return first;
        const size_t count = first >= 0xF0 && first <= 0xF4 ? 3 : first >= 0xE0 ? 2 : first >= 0xC2 ? 1 : 0;
        if (count == 0 || position + count > text.size()) return first;
        uint32_t value = first & (0x7F >> count);
        for (size_t i = 0; i < count; ++i)
        {
            const auto byte = static_cast<uint8_t>(text[position + i]);
            if ((byte & 0xC0) != 0x80) return first;
            value = (value << 6) | (byte & 0x3F);
        }
        if (value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF) ||
            (count == 1 && value < 0x80) || (count == 2 && value < 0x800) || (count == 3 && value < 0x10000)) return first;
        position += count;
        return value;
    }
}
