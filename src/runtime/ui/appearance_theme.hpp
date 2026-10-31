#pragma once

namespace xiui::ui
{
    inline constexpr int native_appearance = 0, xiv1_appearance = 1;
    inline constexpr const char* appearance_themes[]{"XI Theme", "XIV 1.0 Theme"};
    inline constexpr auto AppearanceTheme(const int value) -> int
    { return value == xiv1_appearance ? xiv1_appearance : native_appearance; }
}
