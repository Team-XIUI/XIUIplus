#pragma once

#include "resources/icon_theme_art.hpp"

namespace xiui::ui
{
    // Saved catalog positions are stable; unknown choices retain their saved value and use the default art.
    inline constexpr const char* status_icon_themes[]{"Native", "HD", "Tetsouou", "XIView"};
    inline constexpr const char* status_icon_overrides[]{"Follow global", "Native", "HD", "Tetsouou", "XIView"};
    inline constexpr const char* job_icon_themes[]{"XIVParty", "Classic", "FFXI", "FFXIV-1"};
    inline constexpr int xiv1_job_icon_theme = 3;

    inline auto StatusIconTheme(const int theme) -> int
    { return theme >= 0 && theme < static_cast<int>(std::size(status_icon_themes)) ? theme : 0; }
    inline auto JobIconTheme(const int theme) -> int
    { return theme >= 0 && theme < static_cast<int>(std::size(job_icon_themes)) ? theme : 0; }
    inline auto StatusArtTheme(const uint32_t effect, const int preference, const int globalTheme) -> int
    {
        if (preference < 0 || preference >= static_cast<int>(std::size(status_icon_overrides))) return 0;
        const auto theme = StatusIconTheme(preference == 0 ? globalTheme : preference - 1);
        if (theme == 0 || effect > 1023) return 0;
        const auto& mask = resources::status_theme_presence[theme - 1];
        return (mask[effect / 8] & (1 << (effect % 8))) != 0 ? theme : 0;
    }
}
