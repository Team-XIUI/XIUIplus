#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace xiui
{
    struct vana_time
    {
        uint32_t year{}, month{}, day{}, weekday{}, hour{}, minute{};
    };

    inline auto DecodeVanaTime(const uint32_t raw) -> vana_time
    {
        const uint64_t seconds = uint64_t{raw} * 25;
        const auto days = seconds / 86400;
        return {static_cast<uint32_t>(886 + days / 360), static_cast<uint32_t>(days / 30 % 12 + 1),
            static_cast<uint32_t>(days % 30 + 1), static_cast<uint32_t>(days % 8),
            static_cast<uint32_t>(seconds / 3600 % 24), static_cast<uint32_t>(seconds / 60 % 60)};
    }

    inline constexpr std::array<const char*, 8> vana_days{"Firesday", "Earthsday", "Watersday", "Windsday", "Iceday", "Lightningday", "Lightsday", "Darksday"};
    inline constexpr std::array<const char*, 20> weather_names{"Clear", "Sunshine", "Clouds", "Fog", "Hot spell", "Heat wave", "Rain", "Squall", "Dust storm", "Sand storm", "Wind", "Gales", "Snow", "Blizzards", "Thunder", "Thunderstorms", "Auroras", "Stellar glare", "Gloom", "Darkness"};
    // Element order matches the weekday and native icon tables.
    inline constexpr std::array<int, 8> weather_elements{0, 2, 1, 3, 4, 5, 6, 7};
    inline auto WeatherElement(const int weather) -> int { return weather >= 4 && weather < 20 ? weather_elements[(weather - 4) / 2] : -1; }

    struct compass_point { float x{}, y{}; uint32_t color{0xFFFFFFFF}; bool selected{}; };
    struct world_state
    {
        bool time_valid{}, weather_valid{}, heading_valid{}, compass_rotation_valid{};
        vana_time time{};
        int weather{-1};
        float heading{};
        float compass_rotation{3.141592654f};
        std::vector<compass_point> markers{};
    };

    inline constexpr float compass_flatten = 0.453596121f; // Native compass tilt: cos(1.1).
    inline auto ProjectCompassPoint(const float x, const float y, const float rotation, const float scale) -> std::array<float, 2>
    {
        return {(x * std::cos(rotation) - y * std::sin(rotation)) * scale,
            (x * std::sin(rotation) + y * std::cos(rotation)) * scale * compass_flatten};
    }

    inline auto HeadingDegrees(const float heading) -> float
    {
        if (!std::isfinite(heading)) return 0.0f;
        return std::fmod(heading * 57.295779513f + 450.0f, 360.0f);
    }

    inline auto HeadingName(const float heading) -> const char*
    {
        constexpr std::array names{"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
        return names[static_cast<size_t>(std::floor((HeadingDegrees(heading) + 22.5f) / 45.0f)) % names.size()];
    }
}
