#pragma once

#include "ui/appearance_theme.hpp"

#include <array>
#include <cstdint>

namespace xiui::ui
{
    struct hud_preset
    {
        const char* name;
        const char* command;
        uint16_t resource;
        int appearance;
    };

    inline constexpr std::array hud_presets{
        hud_preset{"Classic", "preset classic", 315, native_appearance},
        hud_preset{"XIV", "preset xiv", 316, native_appearance},
        hud_preset{"XIVClassic", "preset xivclassic", 317, xiv1_appearance}};
}
