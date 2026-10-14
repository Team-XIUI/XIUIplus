#pragma once

#include "module.hpp"

namespace xiui
{
    enum class bar_config_section : size_t { general, layout, bar_style, animation, effects, colors, text };

    inline auto BarConfigSections(void) -> std::span<const module_config_section>
    {
        static constexpr module_config_section sections[]{
            {"General", "Visibility and information shown by this module."},
            {"Layout", "Bar width and height, placement and container layout."},
            {"Bar Style", "Window theme or Modern bars, shape and bookends."},
            {"Animation", "Smooth resource changes and animated highlights."},
            {"Effects", "Bar shadows, outer glow and fill glow."},
            {"Colors", "Resource fills, track colors and bar effects."},
            {"Text", "Choose a text element and edit its style, effects and offsets."}};
        return sections;
    }

    inline constexpr auto MeterConfigSection(const size_t index) -> bar_config_section
    {
        if (index == 1) return bar_config_section::layout;
        if (index <= 5) return bar_config_section::bar_style;
        if (index >= 13 && index <= 17) return bar_config_section::animation;
        if (index >= 18 && index <= 26) return bar_config_section::colors;
        return bar_config_section::effects;
    }
}
