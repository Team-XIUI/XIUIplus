#pragma once

#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <vector>

namespace xiui::resources
{
    struct item_icon_bitmap
    {
        uint32_t width{};
        uint32_t height{};
        std::vector<uint32_t> pixels{};
    };

    inline auto DecodeItemIcon(const std::span<const uint8_t> data) -> std::optional<item_icon_bitmap>
    {
        // Items are paletted DIBs; native status icons can also use BGRA DIBs.
        if (data.size() < 40)
            return std::nullopt;
        const auto read32 = [data](const size_t offset) {
            uint32_t value{};
            std::memcpy(&value, data.data() + offset, sizeof(value));
            return value;
        };
        const auto read16 = [data](const size_t offset) {
            uint16_t value{};
            std::memcpy(&value, data.data() + offset, sizeof(value));
            return value;
        };
        const auto headerSize = read32(0);
        const auto width = static_cast<int32_t>(read32(4));
        const auto signedHeight = static_cast<int32_t>(read32(8));
        const auto bits = read16(14);
        if (headerSize < 40 || headerSize > data.size() || width <= 0 || width > 256 ||
            signedHeight == 0 || signedHeight < -256 || signedHeight > 256 || read16(12) != 1 || (bits != 8 && bits != 32) || read32(16) != 0)
            return std::nullopt;
        const uint32_t height = static_cast<uint32_t>(signedHeight < 0 ? -signedHeight : signedHeight);
        const uint32_t colors = bits == 8 ? (read32(32) != 0 ? read32(32) : 256) : 0;
        if (colors > 256)
            return std::nullopt;
        const size_t pixelOffset = static_cast<size_t>(headerSize) + colors * 4;
        const size_t stride = (static_cast<size_t>(width) * (bits / 8) + 3) & ~size_t{3};
        if (pixelOffset > data.size() || stride * height > data.size() - pixelOffset)
            return std::nullopt;

        item_icon_bitmap result{static_cast<uint32_t>(width), height, {}};
        result.pixels.resize(static_cast<size_t>(width) * height);
        for (uint32_t y = 0; y < height; ++y)
        {
            const uint32_t sourceY = signedHeight > 0 ? height - 1 - y : y;
            for (uint32_t x = 0; x < result.width; ++x)
            {
                if (bits == 32)
                {
                    result.pixels[y * result.width + x] = read32(pixelOffset + sourceY * stride + x * 4);
                    continue;
                }
                const auto index = data[pixelOffset + sourceY * stride + x];
                if (index >= colors)
                    return std::nullopt;
                const uint32_t rgb = read32(headerSize + index * 4) & 0x00FFFFFF;
                result.pixels[y * result.width + x] = rgb != 0 ? rgb | 0xFF000000 : 0;
            }
        }
        return result;
    }
} // namespace xiui::resources
