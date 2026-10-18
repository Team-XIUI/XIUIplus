#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

namespace xiui
{
    using ground_point = std::array<float, 3>;
    using ground_triangle = std::array<ground_point, 3>;

    struct zone_ground
    {
        static constexpr float cell_size = 8;
        std::vector<ground_triangle> triangles;
        std::unordered_map<uint64_t, std::vector<uint32_t>> cells;

        auto Add(const ground_triangle& triangle) -> void;
        auto Nearby(const ground_point& center, float radius, std::vector<uint32_t>& indices) const -> void;
        auto Height(float x, float z, float anchor, float reach, float& height) const -> bool;
        static auto TriangleHeight(const ground_triangle& triangle, float x, float z, float& height) -> bool;
    };

    auto LoadZoneGround(uint16_t zone, const std::filesystem::path& root, const std::atomic_bool& cancel) -> std::unique_ptr<zone_ground>;
}
