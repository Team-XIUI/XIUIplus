#pragma once

#include "native_hud_options.hpp"
#include "native_menu_windows.hpp"

namespace xiui
{
    inline constexpr size_t native_hud_window_count = native_hud_options.size();
    inline constexpr size_t native_window_count = native_hud_window_count + std::size(native_menu_windows);

    struct native_window_state
    {
        bool live{}, movable{};
        float x{}, y{}, width{}, height{};
        bool visible{};
    };

    struct native_window_placement
    {
        bool enabled{}, hidden{};
        float x{}, y{};
    };
}
