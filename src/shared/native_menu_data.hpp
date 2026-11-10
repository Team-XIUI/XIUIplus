#pragma once

#include <cstdint>
#include "native_window_data.hpp"

namespace xiui
{
    struct native_ui_state
    {
        bool menu_available{}, menu_visible{}, window_type_available{};
        uint8_t window_type{};
        uint32_t menu_count{};
        float x{}, y{}, width{}, height{};
        float reference_width{}, reference_height{};
        std::array<native_window_state, native_window_count> windows{};
    };

    struct native_menu_layout
    {
        bool enabled{};
        float left{}, bottom{};
        float viewport_width{}, viewport_height{};
        bool anchor_top{};
        float top{};
        std::array<native_window_placement, native_window_count> windows{};
    };
}
