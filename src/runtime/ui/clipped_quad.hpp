#pragma once

#include "design_system.hpp"

#include <algorithm>
#include <optional>
#include <vector>

namespace xiui::ui
{
    struct textured_vertex { float x, y, z, rhw; D3DCOLOR color; float u, v; };

    inline auto AppendClippedQuad(std::vector<textured_vertex>& output, const std::array<textured_vertex, 4>& quad,
        const std::optional<rect>& clip) -> void
    {
        if (!clip)
        {
            for (const auto index : {0, 1, 2, 1, 3, 2}) output.push_back(quad[index]);
            return;
        }
        if (clip->width <= 0 || clip->height <= 0) return;
        const float edges[]{clip->x - 0.5f, clip->x + clip->width - 0.5f, clip->y - 0.5f, clip->y + clip->height - 0.5f};
        unsigned outsideAny{}, outsideAll{15};
        for (const auto& vertex : quad)
        {
            const unsigned outside = (!(vertex.x - edges[0] >= 0) ? 1u : 0u) | (!(edges[1] - vertex.x >= 0) ? 2u : 0u) |
                (!(vertex.y - edges[2] >= 0) ? 4u : 0u) | (!(edges[3] - vertex.y >= 0) ? 8u : 0u);
            outsideAny |= outside; outsideAll &= outside;
        }
        if (outsideAll != 0) return;
        if (outsideAny == 0)
        {
            for (const auto index : {0, 1, 3, 0, 3, 2}) output.push_back(quad[index]);
            return;
        }
        const auto interpolate = [](const textured_vertex& a, const textured_vertex& b, const float t) {
            auto result = a;
            result.x += (b.x - a.x) * t; result.y += (b.y - a.y) * t;
            result.u += (b.u - a.u) * t; result.v += (b.v - a.v) * t;
            result.color = 0;
            for (unsigned shift = 0; shift < 32; shift += 8)
            {
                const auto first = static_cast<float>((a.color >> shift) & 255), second = static_cast<float>((b.color >> shift) & 255);
                result.color |= static_cast<uint32_t>(std::clamp(first + (second - first) * t, 0.0f, 255.0f)) << shift;
            }
            return result;
        };
        std::array<textured_vertex, 12> polygon{quad[0], quad[1], quad[3], quad[2]}, next{};
        size_t count = 4;
        for (size_t edge = 0; edge < 4 && count >= 3; ++edge)
        {
            const auto distance = [&](const textured_vertex& vertex) {
                const float value = edge < 2 ? vertex.x : vertex.y;
                return edge % 2 == 0 ? value - edges[edge] : edges[edge] - value;
            };
            size_t written{};
            for (size_t i = 0; i < count; ++i)
            {
                const auto& a = polygon[(i + count - 1) % count]; const auto& b = polygon[i];
                const auto da = distance(a), db = distance(b);
                if ((da >= 0) != (db >= 0)) next[written++] = interpolate(a, b, da / (da - db));
                if (db >= 0) next[written++] = b;
            }
            polygon = next; count = written;
        }
        for (size_t i = 1; i + 1 < count; ++i)
        { output.push_back(polygon[0]); output.push_back(polygon[i]); output.push_back(polygon[i + 1]); }
    }
}
