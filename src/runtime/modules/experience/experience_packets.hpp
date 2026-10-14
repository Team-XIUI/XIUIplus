#pragma once

#include "experience_tracker.hpp"

#include <charconv>
#include <cstring>
#include <span>
#include <string_view>

namespace xiui
{
    template<typename T>
    inline auto ExperiencePacketValue(const std::span<const uint8_t> data, const size_t offset) -> T
    {
        T value{};
        if (offset <= data.size() && sizeof(T) <= data.size() - offset) std::memcpy(&value, data.data() + offset, sizeof(T));
        return value;
    }

    struct experience_award { progression_kind kind{}; uint32_t amount{}, chain{}; };
    inline auto DecodeExperienceAward(const std::span<const uint8_t> data, const uint32_t player) -> std::optional<experience_award>
    {
        if (data.size() < 28 || player == 0 || ExperiencePacketValue<uint32_t>(data, 4) != player || ExperiencePacketValue<uint32_t>(data, 8) != player) return std::nullopt;
        const auto amount = ExperiencePacketValue<uint32_t>(data, 16);
        if (amount == 0 || amount > 0x7FFFFFFF) return std::nullopt;
        const auto message = ExperiencePacketValue<uint16_t>(data, 24) & 0x7FFF;
        progression_kind kind{};
        switch (message)
        {
        case 8: case 253: kind = progression_kind::experience; break;
        case 371: case 372: kind = progression_kind::limit; break;
        case 718: case 735: kind = progression_kind::capacity; break;
        case 809: case 810: kind = progression_kind::mastery; break;
        default: return std::nullopt;
        }
        return experience_award{kind, amount, ExperiencePacketValue<uint32_t>(data, 20)};
    }

    struct momentum_report
    {
        std::array<uint64_t, 15> values{};
        bool chain{};
    };

    inline auto DecodeMomentumReport(const std::span<const uint8_t> data, const experience_sample& sample) -> std::optional<momentum_report>
    {
        if (data.size() < 24 || data[4] != 29 || sample.player == 0) return std::nullopt;
        const auto* sender = reinterpret_cast<const char*>(data.data() + 8);
        if (std::string_view{sender, strnlen(sender, 15)} != "Momentum") return std::nullopt;
        const auto* bytes = reinterpret_cast<const char*>(data.data() + 23);
        std::string_view text{bytes, strnlen(bytes, data.size() - 23)};
        if (text.size() > 150) return std::nullopt;
        momentum_report report;
        if (text.starts_with("MOM1|")) text.remove_prefix(5);
        else if (text.starts_with("MOMC1|")) { text.remove_prefix(6); report.chain = true; }
        else return std::nullopt;
        const size_t count = report.chain ? 12 : 15;
        constexpr uint64_t maximum = 9007199254740991ull;
        for (size_t i = 0; i < count; ++i)
        {
            const auto end = text.find('|');
            const auto part = text.substr(0, end);
            if (part.empty() || part.size() > 16 || part.front() < '0' || part.front() > '9') return std::nullopt;
            const auto parsed = std::from_chars(part.data(), part.data() + part.size(), report.values[i]);
            if (parsed.ec != std::errc{} || parsed.ptr != part.data() + part.size() || report.values[i] > maximum) return std::nullopt;
            if (i + 1 == count) { if (end != std::string_view::npos) return std::nullopt; }
            else { if (end == std::string_view::npos) return std::nullopt; text.remove_prefix(end + 1); }
        }
        const auto& v = report.values;
        if (v[0] != sample.player || v[1] != sample.zone || v[1] != ExperiencePacketValue<uint16_t>(data, 6) ||
            v[2] != sample.progress.job || v[3] != sample.progress.level || v[4] != sample.effective_level || v[5] == 0 || v[6] == 0 ||
            v[6] > 0xFFFFFFFFull || v[7] > 1) return std::nullopt;
        if (report.chain)
        {
            if (v[8] > 1 || (!v[7] && v[8]) || (!v[8] && (v[9] || v[10] || v[11])) || v[9] > 65534 ||
                (v[11] == 0 && v[9] != 0) || (v[11] != 0 && (v[11] <= v[10] || v[11] - v[10] > 300000))) return std::nullopt;
        }
        else if (v[8] > 50 || (!v[7] && v[8]) || (v[9] != 0 && v[9] != 15 && v[9] != 20 && v[9] != 25) || v[10] > 1 ||
            v[11] > 65535 || v[12] > 1000 || v[13] > 604800 || v[14] > 1 || (!v[10] && (v[11] || v[12] || v[13]))) return std::nullopt;
        return report;
    }
}
