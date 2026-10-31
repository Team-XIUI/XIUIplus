#pragma once

#include <algorithm>
#include <cmath>

namespace xiui::ui
{
    struct hud_scale
    {
        float x{1}, y{1};
        bool locked{true};
        auto Sanitize(void) -> void
        {
            this->x = std::clamp(std::isfinite(this->x) ? this->x : 1.0f, .25f, 3.0f);
            this->y = std::clamp(std::isfinite(this->y) ? this->y : 1.0f, .25f, 3.0f);
        }
    };

    struct hud_transform
    {
        float x{1}, y{1}, offset_x{}, offset_y{};
        auto Identity(void) const -> bool { return this->x == 1 && this->y == 1 && this->offset_x == 0 && this->offset_y == 0; }
        auto X(float value) const -> float { return value * this->x + this->offset_x; }
        auto Y(float value) const -> float { return value * this->y + this->offset_y; }
        auto InverseX(float value) const -> float { return (value - this->offset_x) / this->x; }
        auto InverseY(float value) const -> float { return (value - this->offset_y) / this->y; }
        template<typename T> auto Map(const T& bounds) const -> T
        { return {this->X(bounds.x), this->Y(bounds.y), bounds.width * this->x, bounds.height * this->y}; }
        template<typename T> auto Inverse(const T& bounds) const -> T
        { return {this->InverseX(bounds.x), this->InverseY(bounds.y), bounds.width / this->x, bounds.height / this->y}; }
        static auto At(float originX, float originY, float scaleX, float scaleY) -> hud_transform
        { return {scaleX, scaleY, originX * (1 - scaleX), originY * (1 - scaleY)}; }
    };
}
