#pragma once

#include <cstdint>

namespace xiui
{
    struct native_ui_state
    {
        bool menu_available{}, menu_visible{}, window_type_available{};
        uint8_t window_type{};
        uint32_t menu_count{};
        float x{}, y{}, width{}, height{};
        float reference_width{}, reference_height{};
    };

    struct native_menu_layout
    {
        bool enabled{};
        float left{}, bottom{};
        float viewport_width{}, viewport_height{};
        bool anchor_top{};
        float top{};
    };
}
