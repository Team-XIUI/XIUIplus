#pragma once

#include "design_system.hpp"
#include <string>

namespace xiui::ui
{
    inline auto FitText(const design_system& design, std::string text, const float width, const text_style& style) -> std::string
    {
        if (design.MeasureText(text.c_str(), style) <= width) return text;
        while (!text.empty())
        {
            auto last = text.size() - 1;
            while (last > 0 && (static_cast<unsigned char>(text[last]) & 0xC0) == 0x80) --last;
            text.resize(last);
            if (design.MeasureText((text + "...").c_str(), style) <= width) return text + "...";
        }
        return {};
    }
}
