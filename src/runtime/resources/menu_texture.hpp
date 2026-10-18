#pragma once

#include <array>
#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <vector>

namespace xiui::resources
{
    inline auto MenuColorFromRgba(const uint32_t color) -> uint32_t
    {
        return (color & 0xFF00FF00) | ((color & 0xFF) << 16) | ((color >> 16) & 0xFF);
    }

    inline auto MenuVertexColorFromRgba(const uint32_t color) -> uint32_t
    {
        const auto argb = MenuColorFromRgba(color);
        uint32_t result{};
        for (unsigned shift = 0; shift < 24; shift += 8)
            result |= std::min(255u, ((argb >> shift) & 255) + 1) << shift;
        const auto alpha = argb >> 24;
        // The client adjusts DAT vertex colors while constructing menu sprites.
        return result | ((alpha == 0 || alpha == 255 ? alpha : std::min(255u, (alpha + 1) * 3 / 2)) << 24);
    }

    struct menu_texture
    {
        uint32_t width{}, height{};
        std::vector<uint32_t> pixels{};
    };

    inline auto ExtractMenuTexture(const menu_texture& image, const uint32_t u, const uint32_t v,
        const uint32_t width, const uint32_t height) -> std::optional<menu_texture>
    {
        if (width == 0 || height == 0 || u >= image.width || v >= image.height ||
            width > image.width - u || height > image.height - v || image.pixels.size() != size_t{image.width} * image.height)
            return std::nullopt;
        menu_texture result{std::bit_ceil(width + 2), std::bit_ceil(height + 2), {}};
        result.pixels.resize(size_t{result.width} * result.height);
        // Extrude the edge texels so linear filtering never reaches adjacent atlas art.
        for (uint32_t y = 0; y < result.height; ++y)
        for (uint32_t x = 0; x < result.width; ++x)
        {
            const auto sourceX = u + std::clamp(x, 1u, width) - 1;
            const auto sourceY = v + std::clamp(y, 1u, height) - 1;
            result.pixels[size_t{y} * result.width + x] = image.pixels[size_t{sourceY} * image.width + sourceX];
        }
        return result;
    }

    inline auto DecodeMenuTexture(const std::span<const uint8_t> data) -> std::optional<menu_texture>
    {
        if (data.size() < 0x45 || data[0] != 0xA1) return std::nullopt;
        const auto word = [data](size_t at) { uint16_t value{}; std::memcpy(&value, data.data() + at, 2); return value; };
        const auto dword = [data](size_t at) { uint32_t value{}; std::memcpy(&value, data.data() + at, 4); return value; };
        const auto width = dword(0x15), height = dword(0x19), format = dword(0x39);
        const bool dxt3 = format == 0x44585433;
        if ((!dxt3 && format != 0x44585431) || width == 0 || height == 0 || width > 2048 || height > 2048) return std::nullopt;
        const size_t columns = (width + 3) / 4, rows = (height + 3) / 4, blockSize = dxt3 ? 16 : 8;
        if (columns * rows * blockSize > data.size() - 0x45) return std::nullopt;
        const auto rgb565 = [](uint16_t value) -> uint32_t {
            const uint32_t r = (value >> 11) & 31, g = (value >> 5) & 63, b = value & 31;
            return 0xFF000000 | ((r << 3 | r >> 2) << 16) | ((g << 2 | g >> 4) << 8) | (b << 3 | b >> 2);
        };
        menu_texture result{width, height, std::vector<uint32_t>(size_t{width} * height)};
        for (size_t by = 0; by < rows; ++by) for (size_t bx = 0; bx < columns; ++bx)
        {
            const size_t block = 0x45 + (by * columns + bx) * blockSize, color = block + (dxt3 ? 8 : 0);
            const auto c0 = word(color), c1 = word(color + 2);
            std::array<uint32_t, 4> palette{rgb565(c0), rgb565(c1), 0, 0};
            for (unsigned shift = 0; shift < 24; shift += 8)
            {
                const auto a = (palette[0] >> shift) & 255, b = (palette[1] >> shift) & 255;
                palette[2] |= ((c0 > c1 || dxt3) ? (2 * a + b) / 3 : (a + b) / 2) << shift;
                if (c0 > c1 || dxt3) palette[3] |= ((a + 2 * b) / 3) << shift;
            }
            palette[2] |= 0xFF000000;
            if (c0 > c1 || dxt3) palette[3] |= 0xFF000000;
            const auto indices = dword(color + 4);
            for (unsigned pixel = 0; pixel < 16; ++pixel)
            {
                const size_t x = bx * 4 + pixel % 4, y = by * 4 + pixel / 4;
                auto rgba = palette[(indices >> (pixel * 2)) & 3];
                if (dxt3) rgba = (rgba & 0xFFFFFF) | (static_cast<uint32_t>((data[block + pixel / 2] >> ((pixel % 2) * 4)) & 15) * 17 << 24);
                if (x < width && y < height) result.pixels[y * width + x] = rgba;
            }
        }
        return result;
    }
}
