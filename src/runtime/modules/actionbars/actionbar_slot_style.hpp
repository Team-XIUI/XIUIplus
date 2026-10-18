#pragma once

#include <d3d8.h>

namespace xiui
{
    struct actionbar_slot_style
    {
        static constexpr int window_frame = 4;
        int frame{window_frame}, cooldown_style{}, binding_anchor{}, count_anchor{8}, timer_anchor{4}, cost_anchor{6};
        float radius{3}, border_width{1}, icon_inset{1}, icon_opacity{1}, inactive_opacity{0.38f};
        float binding_x{}, binding_y{-3}, count_x{}, count_y{}, timer_x{}, timer_y{}, cost_x{}, cost_y{};
        bool shadow{true}, sheen{true}, hover_highlight{true}, press_highlight{true}, activation_flash{true}, cooldown_text{true}, mp_cost{true};
        D3DCOLOR fill{0xE0171B24}, empty_fill{0x70252A33}, border{0x887C818A};
        D3DCOLOR icon_tint{0xFFFFFFFF}, unavailable_tint{0xFF8D8D95}, inactive_tint{0xFF62626A};
        D3DCOLOR hover_color{0xCCF4DA97}, pressed_color{0xFF565D77}, cooldown_color{0xA8000000};
        auto operator==(const actionbar_slot_style&) const -> bool = default;
    };
}
