#pragma once

#include "settings_store.hpp"

namespace xiui
{
    enum class text_property : size_t
    {
        size, font, letter_spacing, shadow, shadow_color, shadow_x, shadow_y, soft_shadow,
        outline, outline_color, glow, glow_color, sheen, sheen_color, sheen_width, sheen_speed, weight, italic, count
    };
    inline constexpr size_t text_property_count = static_cast<size_t>(text_property::count);

    auto TextPropertyFlag(text_property property) -> const char*;
    // Built-in faces save their index; installed fonts save font_family so the choice survives list changes.
    auto ReadFontChoice(const settings_store& store, const char* section, int fallback) -> int;
    auto WriteFontChoice(const settings_store& store, const char* section, int font) -> void;
    auto CopyTextProperty(ui::text_style& target, const ui::text_style& source, text_property property) -> void;
    auto LoadTextStyle(const settings_store& store, const char* section, const ui::text_style& fallback) -> ui::text_style;
    auto SaveTextStyle(const settings_store& store, const char* section, const ui::text_style& style) -> void;
    auto InheritGlobalTextStyle(const settings_store& store, const ui::text_style& fallback) -> ui::text_style;
}
