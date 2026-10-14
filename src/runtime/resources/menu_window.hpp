#pragma once

#include "menu_texture.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace xiui::resources
{
    struct menu_patch
    {
        size_t texture{};
        uint16_t width{}, height{}, u{}, v{};
        std::array<uint32_t, 4> colors{};
        bool additive{};
    };

    struct menu_window
    {
        // Background, horizontal frame, corner, vertical frame.
        std::array<menu_texture, 4> textures{};
        std::array<menu_patch, 9> patches{};
    };

    inline auto DecodeMenuWindow(const std::span<const uint8_t> data) -> std::optional<menu_window>
    {
        const auto word = [data](size_t at) { uint16_t v{}; std::memcpy(&v, data.data() + at, 2); return v; };
        const auto dword = [data](size_t at) { uint32_t v{}; std::memcpy(&v, data.data() + at, 4); return v; };
        const auto textureIndex = [](std::string_view name) -> size_t {
            constexpr std::array names{"newtex  ", "hfr1    ", "corner  ", "vfr1    "};
            for (size_t i = 0; i < names.size(); ++i)
                if (name == names[i]) return i;
            return names.size();
        };
        menu_window result{};
        bool havePatches{};
        for (size_t section = 0; section + 16 <= data.size();)
        {
            const auto meta = dword(section + 4);
            const size_t size = ((meta >> 7) & 0xFFFFF) * size_t{16};
            if (size < 16 || size > data.size() - section)
                return std::nullopt;
            const size_t start = section + 16, end = section + size;
            if ((meta & 0x7F) == 0x20 && size >= 16 + 0x45)
            {
                const size_t index = textureIndex({reinterpret_cast<const char*>(data.data() + start + 9), 8});
                if (index < result.textures.size())
                {
                    auto texture = DecodeMenuTexture(data.subspan(start, end - start));
                    if (!texture) return std::nullopt;
                    result.textures[index] = std::move(*texture);
                }
            }
            else if ((meta & 0x7F) == 0x31 && size >= 33)
            {
                // A window definition lists its textures, then 62-byte textured quads.
                const size_t countAt = start + 17 + size_t{data[start + 16]} * 16;
                if (countAt + 2 > end || word(countAt) < 9 || size_t{word(countAt)} * 62 > end - countAt - 2)
                    return std::nullopt;
                constexpr std::array<size_t, 9> order{4, 0, 1, 2, 3, 5, 6, 7, 8};
                for (size_t i = 0; i < order.size(); ++i)
                {
                    const size_t at = countAt + 2 + i * 62;
                    auto& patch = result.patches[order[i]];
                    patch.texture = textureIndex({reinterpret_cast<const char*>(data.data() + at + 54), 8});
                    patch.width = word(at + 17); patch.height = word(at + 19);
                    patch.u = word(at + 21); patch.v = word(at + 23);
                    patch.additive = data[at + 44] == 2;
                    if (patch.texture >= result.textures.size() || patch.width == 0 || patch.height == 0 ||
                        patch.width > 512 || patch.height > 512)
                        return std::nullopt;
                    for (size_t vertex = 0; vertex < 4; ++vertex)
                        patch.colors[vertex] = MenuColorFromRgba(dword(at + 26 + vertex * 4));
                }
                havePatches = true;
            }
            section = end;
        }
        for (const auto& texture : result.textures)
            if (texture.pixels.empty()) return std::nullopt;
        return havePatches ? std::optional{std::move(result)} : std::nullopt;
    }
} // namespace xiui::resources
