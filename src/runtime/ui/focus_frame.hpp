#pragma once

#include "design_system.hpp"

namespace xiui::ui
{
    struct focus_frame_style
    {
        D3DCOLOR color{0xFFFFA356};
        float thickness{2};
        float radius{4};
        float padding{2};
        bool glow{true};
        D3DCOLOR glow_color{0xFFFFA356};
        float glow_opacity{0.18f};
        float glow_size{4};
        bool shadow{true};
        D3DCOLOR shadow_color{0xFF000000};
        float shadow_opacity{0.45f};
        float shadow_size{3};
        float shadow_x{1};
        float shadow_y{1};
    };
}
