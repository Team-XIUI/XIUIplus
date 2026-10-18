#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace xiui
{
    struct invitation_packet { uint32_t sender{}, index{}; bool alliance{}; std::string name{}; };
    struct trade_response { uint32_t sender{}, kind{}; };

    inline auto DecodeTradeResponse(uint16_t id, std::span<const uint8_t> data) -> std::optional<trade_response>
    {
        if (id != 0x22 || data.size() < 12) return std::nullopt;
        trade_response result;
        std::memcpy(&result.sender, data.data() + 4, 4);
        std::memcpy(&result.kind, data.data() + 8, 4);
        return result;
    }

    inline auto DecodeInvitation(const uint16_t id, const std::span<const uint8_t> data) -> std::optional<invitation_packet>
    {
        if ((id != 0xDC && id != 0x21) || data.size() < (id == 0xDC ? 0x20u : 0xCu)) return std::nullopt;
        invitation_packet result{}; uint16_t index{};
        std::memcpy(&result.sender, data.data() + 4, 4); std::memcpy(&index, data.data() + 8, 2);
        result.index = index;
        if (result.sender == 0 || result.index == 0) return std::nullopt;
        if (id == 0xDC)
        {
            const auto* text = reinterpret_cast<const char*>(data.data() + 12);
            result.name.assign(text, strnlen(text, 16));
            result.alliance = data[11] != 0;
            if (result.name.empty()) return std::nullopt;
        }
        return result;
    }

    class key_item_tracker
    {
    public:
        auto Seed(const uint32_t key) -> void
        { if (key < 4096) this->tables_[key / 512][key % 512 / 8] |= 1u << (key % 8); }

        auto Apply(const uint32_t table, const std::span<const uint8_t> flags, const bool notify) -> std::vector<uint32_t>
        {
            std::vector<uint32_t> gained;
            if (table >= this->tables_.size() || flags.size() != 64) return gained;
            auto& previous = this->tables_[table];
            for (size_t byte = 0; byte < previous.size(); ++byte)
            {
                const auto added = static_cast<uint8_t>(flags[byte] & ~previous[byte]);
                if (notify)
                    for (unsigned bit = 0; bit < 8; ++bit)
                        if ((added & (1u << bit)) != 0) gained.push_back(static_cast<uint32_t>(table * 512 + byte * 8 + bit));
                previous[byte] = flags[byte];
            }
            return gained;
        }

    private:
        std::array<std::array<uint8_t, 64>, 8> tables_{};
    };
}
