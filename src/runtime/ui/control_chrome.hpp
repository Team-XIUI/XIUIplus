#pragma once

#include "design_system.hpp"

namespace xiui::ui
{
    inline auto ControlBorderColor(const design_system& design, const bool highlighted = false) -> D3DCOLOR
    {
        if (design.IsXiv1Theme()) return highlighted ? 0xFFADB5C0 : 0xFF7E8794;
        return highlighted ? design.GetTheme().colors.control_selected : design.GetTheme().colors.border;
    }
}
