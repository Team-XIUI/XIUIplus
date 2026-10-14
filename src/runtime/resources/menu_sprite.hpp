#pragma once

#include "menu_texture.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace xiui::resources
{
    struct menu_sprite_quad
    {
        std::array<int16_t, 8> points{};
        uint16_t width{}, height{}, u{}, v{};
        uint8_t uv_flags{};
        std::array<uint32_t, 4> colors{};
        bool additive{};
        std::string texture;
    };

    using menu_sprite = std::vector<std::vector<menu_sprite_quad>>;

    inline auto DecodeMenuSprite(const std::span<const uint8_t> payload, const std::optional<size_t> selectedGroup = std::nullopt) -> std::optional<menu_sprite>
    {
        if (payload.size() < 19) return std::nullopt;
        const auto word = [payload](const size_t at) { uint16_t value{}; std::memcpy(&value, payload.data() + at, 2); return value; };
        size_t at = 17 + size_t{payload[16]} * 16;
        if (at + 2 > payload.size()) return std::nullopt;
        const auto groups = word(at); at += 2;
        if (groups == 0 || groups > (selectedGroup ? 1024 : 128) || (selectedGroup && *selectedGroup >= groups)) return std::nullopt;
        menu_sprite result(selectedGroup ? 1 : groups);
        for (size_t groupIndex = 0; groupIndex < groups; ++groupIndex)
        {
            if (at >= payload.size()) return std::nullopt;
            const size_t count = payload[at++];
            if (count == 0 || (!selectedGroup && count > 32) || count * 61 > payload.size() - at) return std::nullopt;
            if (selectedGroup && groupIndex != *selectedGroup) { at += count * 61; continue; }
            auto& group = result[selectedGroup ? 0 : groupIndex];
            for (size_t index = 0; index < count; ++index, at += 61)
            {
                menu_sprite_quad quad{};
                std::memcpy(quad.points.data(), payload.data() + at, 16);
                quad.width = word(at + 16); quad.height = word(at + 18);
                quad.u = word(at + 20); quad.v = word(at + 22);
                quad.uv_flags = payload[at + 24];
                for (size_t vertex = 0; vertex < quad.colors.size(); ++vertex)
                {
                    uint32_t rgba{}; std::memcpy(&rgba, payload.data() + at + 25 + vertex * 4, 4);
                    quad.colors[vertex] = MenuColorFromRgba(rgba);
                }
                quad.additive = payload[at + 43] == 2;
                quad.texture.assign(reinterpret_cast<const char*>(payload.data() + at + 53), 8);
                if (quad.width == 0 || quad.height == 0 || quad.width > 2048 || quad.height > 2048 || quad.uv_flags > 3) return std::nullopt;
                group.push_back(std::move(quad));
            }
            if (selectedGroup) return result;
        }
        return result;
    }
}
