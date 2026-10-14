#pragma once

#include "design_system.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace xiui::ui
{
    struct slice_region
    {
        rect bounds{};
        rect uv{0.0f, 0.0f, 1.0f, 1.0f};
    };

    inline auto LayoutNineSlice(const rect& bounds, float edge, float tileSize = 16.0f) -> std::array<slice_region, 9>
    {
        const float width = std::max(0.0f, bounds.width);
        const float height = std::max(0.0f, bounds.height);
        const float side = std::clamp(edge, 0.0f, width * 0.5f);
        const float cap = std::clamp(edge, 0.0f, height * 0.5f);
        const float widths[] = {side, width - side * 2.0f, side};
        const float heights[] = {cap, height - cap * 2.0f, cap};
        const float xs[] = {bounds.x, bounds.x + side, bounds.x + width - side};
        const float ys[] = {bounds.y, bounds.y + cap, bounds.y + height - cap};
        std::array<slice_region, 9> result{};
        for (size_t row = 0; row < 3; ++row)
        {
            for (size_t column = 0; column < 3; ++column)
            {
                auto& slice = result[row * 3 + column];
                slice.bounds = {xs[column], ys[row], widths[column], heights[row]};
                slice.uv.width = column == 1 ? widths[column] / std::max(1.0f, tileSize) : 1.0f;
                slice.uv.height = row == 1 ? heights[row] / std::max(1.0f, tileSize) : 1.0f;
            }
        }
        return result;
    }

    inline auto LayoutNineSliceFrame(const rect& bounds, float corner, float edge) -> std::array<slice_region, 9>
    {
        auto result = LayoutNineSlice(bounds, corner);
        const float side = std::clamp(edge, 0.0f, std::max(0.0f, bounds.width) * 0.5f);
        const float cap = std::clamp(edge, 0.0f, std::max(0.0f, bounds.height) * 0.5f);
        result[1].bounds.height = result[7].bounds.height = cap;
        result[7].bounds.y = bounds.y + std::max(0.0f, bounds.height) - cap;
        result[3].bounds.width = result[5].bounds.width = side;
        result[5].bounds.x = bounds.x + std::max(0.0f, bounds.width) - side;
        result[4].bounds = {bounds.x, bounds.y, std::max(0.0f, bounds.width), std::max(0.0f, bounds.height)};
        return result;
    }

    struct nine_slice_tile
    {
        IDirect3DTexture8* texture{};
        rect source{};
        float texture_width{1.0f}, texture_height{1.0f};
        texture_style style{};
    };

    struct nine_slice_skin
    {
        std::array<nine_slice_tile, 9> tiles{};
        float corner_size{16.0f}, edge_size{16.0f};
        bool background_under_frame{};
    };

    struct horizontal_fade
    {
        rect bounds{};
        float left{1}, right{1};
        auto At(const float x) const -> float
        { return std::lerp(this->left, this->right, std::clamp((x - this->bounds.x) / std::max(1.0f, this->bounds.width), 0.0f, 1.0f)); }
    };

    inline auto DrawNineSliceTile(const design_system& design, const nine_slice_tile& tile, const rect& bounds, float scale, float opacity, const horizontal_fade* fade = nullptr) -> void
    {
        scale = std::max(0.01f, scale);
        opacity = std::clamp(opacity, 0.0f, 1.0f);
        auto source = tile.source;
        if (tile.style.repeat_x) source.width = bounds.width / scale;
        if (tile.style.repeat_y) source.height = bounds.height / scale;
        const rect uv{source.x / tile.texture_width, source.y / tile.texture_height,
            source.width / tile.texture_width, source.height / tile.texture_height};
        auto style = tile.style;
        for (size_t i = 0; i < style.colors.size(); ++i)
        {
            auto& color = style.colors[i];
            const float mask = fade != nullptr ? fade->At(bounds.x + (i % 2 == 1 ? bounds.width : 0)) : 1;
            color = (color & 0xFFFFFF) | (static_cast<uint32_t>((color >> 24) * opacity * mask) << 24);
        }
        design.DrawTexture(tile.texture, bounds, uv, style);
    }

    inline auto DrawNineSlice(const design_system& design, const nine_slice_skin& skin, const rect& bounds, float scale, float opacity, const bool includeBackground = true, const horizontal_fade* fade = nullptr) -> void
    {
        scale = std::max(0.01f, scale);
        opacity = std::clamp(opacity, 0.0f, 1.0f);
        const auto slices = skin.background_under_frame
            ? LayoutNineSliceFrame(bounds, skin.corner_size * scale, skin.edge_size * scale)
            : LayoutNineSlice(bounds, skin.corner_size * scale);
        constexpr std::array<size_t, 9> order{4, 0, 1, 2, 3, 5, 6, 7, 8};
        for (const auto index : order)
            if (includeBackground || index != 4) DrawNineSliceTile(design, skin.tiles[index], slices[index].bounds, scale, opacity, fade);
    }
} // namespace xiui::ui
