#pragma once

#include "menu_texture.hpp"

namespace xiui::resources
{
    // The calling thread owns its COM apartment. Pixels use straight-alpha BGRA.
    auto DecodeWicImage(std::span<const uint8_t> bytes, uint32_t maximumDimension,
        bool square = false, uint32_t fitSquare = 0) -> std::optional<menu_texture>;
}
