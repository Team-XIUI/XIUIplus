#pragma once

#include "config_layout.hpp"
#include "ui/hud_transform.hpp"

#include <cmath>

namespace xiui::ui
{
    inline auto ConfigWindowScale(const float requested, const D3DVIEWPORT8& viewport) -> float
    {
        if (viewport.Width == 0 || viewport.Height == 0) return requested;
        const float fit = std::min(viewport.Width / config_layout::minimum_width, viewport.Height / config_layout::minimum_height);
        return std::min(requested, std::max(1.0f, fit));
    }

    inline auto ConfigWindowViewport(const D3DVIEWPORT8& viewport, const hud_transform& transform) -> D3DVIEWPORT8
    {
        auto result = viewport;
        result.X = static_cast<DWORD>(std::max(0.0f, std::ceil(transform.InverseX(static_cast<float>(viewport.X)))));
        result.Y = static_cast<DWORD>(std::max(0.0f, std::ceil(transform.InverseY(static_cast<float>(viewport.Y)))));
        const auto right = transform.InverseX(static_cast<float>(viewport.X + viewport.Width));
        const auto bottom = transform.InverseY(static_cast<float>(viewport.Y + viewport.Height));
        result.Width = static_cast<DWORD>(std::max(1.0f, std::floor(right) - result.X));
        result.Height = static_cast<DWORD>(std::max(1.0f, std::floor(bottom) - result.Y));
        return result;
    }
}
