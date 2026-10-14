#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace xiui
{
    inline constexpr std::string_view chat_cap_open = "\xEE\x80\x80", chat_cap_close = "\xEE\x80\x81";

    inline auto ChatCapAt(const std::string_view text, const size_t at) -> int
    {
        if (at > text.size()) return -1;
        if (text.substr(at).starts_with(chat_cap_open)) return 0;
        if (text.substr(at).starts_with(chat_cap_close)) return 1;
        return -1;
    }

    inline auto ChatTokenSize(const std::string_view text, const size_t at) -> size_t
    {
        if (at > text.size() || text.size() - at < 6 || static_cast<uint8_t>(text[at]) != 0xFD || static_cast<uint8_t>(text[at + 5]) != 0xFD) return 0;
        for (size_t i = 1; i < 5; ++i) if (text[at + i] == '\0') return 0;
        return 6;
    }

    inline auto ChatToken(const uint16_t id, const uint8_t language = 2) -> std::string
    {
        if ((id & 255) == 0 || (id >> 8) == 0) return {};
        return {static_cast<char>(0xFD), 2, static_cast<char>(language), static_cast<char>(id >> 8), static_cast<char>(id & 255), static_cast<char>(0xFD)};
    }
}
