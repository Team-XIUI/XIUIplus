#pragma once

#include <d3d8.h>

namespace xiui::ui
{
    struct selection_style
    {
        D3DCOLOR target_color{0xFF69BFFF}, subtarget_color{0xFFFFAC58};
        float background_opacity{0.12f};
        float padding_x{6}, padding_y{4};
        bool arrow{true};
    };
}
