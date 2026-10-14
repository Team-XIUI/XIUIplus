#pragma once

#include "Ashita.h"

#include <array>
#include <cmath>

namespace xiui
{
    inline auto ProjectWorldPoint(const std::array<float, 3>& position, const D3DMATRIX& view, const D3DMATRIX& projection,
        const D3DVIEWPORT8& viewport, float& screenX, float& screenY) -> bool
    {
        const auto transform = [](const std::array<float, 4>& point, const D3DMATRIX& matrix) {
            std::array<float, 4> result{};
            for (size_t column = 0; column < 4; ++column)
                for (size_t row = 0; row < 4; ++row)
                    result[column] += point[row] * matrix.m[row][column];
            return result;
        };
        const auto camera = transform({position[0], position[1], position[2], 1.0f}, view);
        const auto clip = transform(camera, projection);
        for (const float value : clip)
            if (!std::isfinite(value))
                return false;
        if (viewport.Width == 0 || viewport.Height == 0 || clip[3] <= 0.001f)
            return false;
        const float x = clip[0] / clip[3];
        const float y = clip[1] / clip[3];
        const float z = clip[2] / clip[3];
        if (z < 0.0f || z > 1.0f || std::abs(x) > 1.0f || std::abs(y) > 1.0f)
            return false;
        screenX = static_cast<float>(viewport.X) + (x + 1.0f) * static_cast<float>(viewport.Width) * 0.5f;
        screenY = static_cast<float>(viewport.Y) + (1.0f - y) * static_cast<float>(viewport.Height) * 0.5f;
        return true;
    }
} // namespace xiui
