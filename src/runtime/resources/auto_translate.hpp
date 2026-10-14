#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>
#include <optional>

namespace xiui::resources
{
    struct auto_translate_entry { uint16_t id{}; uint8_t language{}; std::string text; };

    inline auto DecodeAutoTranslate(const std::span<const uint8_t> bytes) -> std::optional<std::vector<auto_translate_entry>>
    {
        std::vector<auto_translate_entry> result;
        for (size_t at = 0; at < bytes.size();)
        {
            if (bytes.size() - at < 4 || bytes[at] != 2 || (bytes[at + 1] != 1 && bytes[at + 1] != 2)) return std::nullopt;
            const uint8_t language = bytes[at + 1];
            const auto id = static_cast<uint16_t>((bytes[at + 2] << 8) | bytes[at + 3]);
            at += 4;
            if ((id & 255) == 0)
            {
                if (bytes.size() - at < 72) return std::nullopt;
                at += 72;
                continue;
            }
            if (at == bytes.size()) return std::nullopt;
            const size_t length = bytes[at++];
            if (length == 0 || length > bytes.size() - at || bytes[at + length - 1] != 0) return std::nullopt;
            result.push_back({id, language, {reinterpret_cast<const char*>(bytes.data() + at), length - 1}});
            at += length;
            if (language == 1)
            {
                if (at == bytes.size()) return std::nullopt;
                const size_t reading = bytes[at++];
                if (reading > bytes.size() - at) return std::nullopt;
                at += reading;
            }
            if (result.size() > 16384) return std::nullopt;
        }
        return result;
    }
}
