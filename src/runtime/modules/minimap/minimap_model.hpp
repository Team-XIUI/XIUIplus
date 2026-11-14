#pragma once

#include "design_system.hpp"
#include "world/entity_name_colors.hpp"

#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace xiui
{
#pragma pack(push, 1)
    struct native_map_record
    {
        int16_t zone{};
        int8_t floor{};
        uint8_t number{}, flags{};
        int8_t scale{}, texture{}, unknown{};
        uint16_t resource{};
        int16_t offset_x{}, offset_y{};
    };
#pragma pack(pop)
    static_assert(sizeof(native_map_record) == 14);

    inline auto FindMinimapRecord(const std::span<const native_map_record> records, const uint16_t zone,
        const std::optional<int> floor) -> const native_map_record*
    {
        const native_map_record* only{};
        size_t count{};
        for (const auto& record : records)
        {
            if (record.zone != zone || record.scale == 0 || record.texture <= 0) continue;
            if (floor && record.floor == *floor) return &record;
            only = &record; ++count;
        }
        // A single map needs no terrain lookup. Never guess a floor in a multimap zone.
        return count == 1 ? only : nullptr;
    }

    // The client's own map DAT; the low flag bit selects the later-expansion file range.
    inline auto MinimapNativeFile(const native_map_record& map) -> uint32_t
    {
        return ((map.flags & 1) != 0 ? 53295u : 5312u) + map.resource;
    }

    inline auto MinimapUV(const native_map_record& map, const float x, const float y) -> ui::point
    {
        const float scale = std::abs(static_cast<float>(map.scale)) / 5.0f;
        return {(x * scale - map.offset_x) / 512.0f, (-y * scale - map.offset_y) / 512.0f};
    }

    inline auto MinimapHeight(const float width, const int shape) -> float { return shape == 2 ? width * 9 / 16 : width; }

    inline constexpr float minimap_coordinate_band = 16.0f / 512;
    enum class minimap_coordinate_edge : uint8_t { left, top, right, bottom };
    struct minimap_coordinate_strip { ui::rect bounds{}, uv{}; };

    inline auto MinimapCoordinateStrip(const ui::rect& view, const ui::rect& map, const minimap_coordinate_edge edge) -> minimap_coordinate_strip
    {
        if (map.width <= 0 || view.width <= 0 || view.height <= 0) return {};
        const float size = map.width, thickness = size * minimap_coordinate_band;
        const bool horizontal = edge == minimap_coordinate_edge::top || edge == minimap_coordinate_edge::bottom;
        if (horizontal)
        {
            const float left = std::max(view.x, map.x), right = std::min(view.x + view.width, map.x + size);
            if (right <= left) return {};
            const bool bottom = edge == minimap_coordinate_edge::bottom;
            return {{left, bottom ? view.y + view.height - thickness : view.y, right - left, thickness},
                {(left - map.x) / size, bottom ? 1 - minimap_coordinate_band : 0, (right - left) / size, minimap_coordinate_band}};
        }
        const float top = std::max(view.y, map.y), bottom = std::min(view.y + view.height, map.y + size);
        if (bottom <= top) return {};
        const bool right = edge == minimap_coordinate_edge::right;
        return {{right ? view.x + view.width - thickness : view.x, top, thickness, bottom - top},
            {right ? 1 - minimap_coordinate_band : 0, (top - map.y) / size, minimap_coordinate_band, (bottom - top) / size}};
    }

    inline auto MinimapCornerRadius(const ui::rect& view, const int shape, const float radius) -> float
    {
        const float limit = std::max(0.0f, std::min(view.width, view.height) / 2);
        return shape == 1 ? limit : std::clamp(radius, 0.0f, limit);
    }

    inline auto MinimapContains(const ui::rect& view, const int shape, const ui::point point, const float margin = 0, const float cornerRadius = 0) -> bool
    {
        if (point.x < view.x + margin || point.y < view.y + margin ||
            point.x > view.x + view.width - margin || point.y > view.y + view.height - margin) return false;
        const float radius = std::max(0.0f, MinimapCornerRadius(view, shape, cornerRadius) - margin);
        if (radius == 0) return true;
        const float x = std::clamp(point.x, view.x + margin + radius, view.x + view.width - margin - radius);
        const float y = std::clamp(point.y, view.y + margin + radius, view.y + view.height - margin - radius);
        return std::hypot(point.x - x, point.y - y) <= radius + 0.0001f;
    }

    inline auto MinimapVisionArc(const ui::rect& view, const float heading, const float size, const float angle) -> std::array<ui::point, 25>
    {
        std::array<ui::point, 25> arc{};
        const float radius = std::max(0.0f, std::min(view.width, view.height) / 2 - 6) * size;
        const float spread = angle * 0.01745329252f;
        for (size_t i = 0; i < arc.size(); ++i)
        {
            const float direction = heading - spread / 2 + spread * static_cast<float>(i) / (arc.size() - 1);
            arc[i] = {view.x + view.width / 2 + std::cos(direction) * radius, view.y + view.height / 2 + std::sin(direction) * radius};
        }
        return arc;
    }

    inline auto IsMinimapNpc(const uint32_t spawnFlags, const uint32_t renderFlags) -> bool
    {
        return ClassifyEntity(spawnFlags, 0, false, false) == entity_class::npc && (spawnFlags & 0x1800) == 0 &&
            (renderFlags & 0x200) != 0 && (renderFlags & 0x4000) == 0;
    }

    inline auto IsMinimapEnemy(const uint32_t spawnFlags, const uint32_t renderFlags, const uint32_t hpPercent) -> bool
    {
        return IsMonster(ClassifyEntity(spawnFlags, 0, false, false)) && (spawnFlags & 0x1800) == 0 &&
            (renderFlags & 0x200) != 0 && (renderFlags & 0x4000) == 0 && hpPercent > 0;
    }

    inline constexpr uint32_t minimap_npc_color = 0xFF55DD77;
    inline constexpr uint32_t minimap_enemy_color = 0xFFFF5555;
    inline auto MinimapEnemyColor(const uint32_t claim, const bool groupClaim) -> uint32_t
    {
        return (claim & 0xFFFF) != 0 && !groupClaim ? other_claim_name_color : minimap_enemy_color;
    }
    enum class minimap_marker_kind : uint8_t { party, alliance, pet, target, npc, enemy };
    struct minimap_marker { ui::point uv{}; uint32_t color{0xFFFFFFFF}; minimap_marker_kind kind{}; };
    struct minimap_state
    {
        bool available{};
        uint16_t zone{};
        int floor{};
        int map_number{1};
        ui::point player{};
        float heading{};
        std::string name{};
        std::string native_map{};
        std::vector<minimap_marker> markers{};
    };
}
