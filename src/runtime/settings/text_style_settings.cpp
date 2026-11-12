#include "text_style_settings.hpp"

#include <algorithm>
#include <cmath>

namespace xiui
{
    auto TextPropertyFlag(const text_property property) -> const char*
    {
        static constexpr const char* flags[]{"size_set", "font_set", "letter_spacing_set", "shadow_enabled_set", "shadow_color_set",
            "shadow_x_set", "shadow_y_set", "shadow_soft_set", "stroke_enabled_set", "stroke_color_set", "glow_enabled_set",
            "glow_color_set", "sheen_enabled_set", "sheen_color_set", "sheen_width_set", "sheen_speed_set", "weight_set", "italic_set"};
        static_assert(std::size(flags) == text_property_count);
        return flags[static_cast<size_t>(property)];
    }

    auto ReadFontChoice(const settings_store& store, const char* section, const int fallback) -> int
    {
        const auto family = store.ReadString(section, "font_family");
        return family.empty() ? ui::FontIndex(store.ReadInt(section, "font", fallback)) : ui::FontFamilyIndex(family);
    }

    auto WriteFontChoice(const settings_store& store, const char* section, const int font) -> void
    {
        const auto family = ui::FontFamily(font);
        // Older builds read font 0 for an installed family. Sections that never used one stay unchanged.
        store.WriteInt(section, "font", family.empty() ? font : 0);
        if (!family.empty() || !store.ReadString(section, "font_family").empty()) store.WriteString(section, "font_family", family);
    }

    auto CopyTextProperty(ui::text_style& target, const ui::text_style& source, const text_property property) -> void
    {
        switch (property)
        {
        case text_property::size: target.scale = source.scale; break;
        case text_property::font: target.font = source.font; target.font_explicit = source.font_explicit; break;
        case text_property::letter_spacing: target.letter_spacing = source.letter_spacing; break;
        case text_property::shadow: target.shadow.enabled = source.shadow.enabled; break;
        case text_property::shadow_color: target.shadow.color = source.shadow.color; break;
        case text_property::shadow_x: target.shadow.offset_x = source.shadow.offset_x; break;
        case text_property::shadow_y: target.shadow.offset_y = source.shadow.offset_y; break;
        case text_property::soft_shadow: target.shadow.soft = source.shadow.soft; break;
        case text_property::outline: target.stroke.enabled = source.stroke.enabled; break;
        case text_property::outline_color: target.stroke.color = source.stroke.color; break;
        case text_property::glow: target.glow.enabled = source.glow.enabled; break;
        case text_property::glow_color: target.glow.color = source.glow.color; break;
        case text_property::sheen: target.sheen.enabled = source.sheen.enabled; break;
        case text_property::sheen_color: target.sheen.color = source.sheen.color; break;
        case text_property::sheen_width: target.sheen.width = source.sheen.width; break;
        case text_property::sheen_speed: target.sheen.speed = source.sheen.speed; break;
        case text_property::weight: target.weight_choice = source.weight_choice; break;
        case text_property::italic: target.italic = source.italic; break;
        default: break;
        }
    }

    auto LoadTextStyle(const settings_store& store, const char* section, const ui::text_style& fallback) -> ui::text_style
    {
        auto style = fallback;
        const auto number = [&](const char* key, const float value, const float minimum, const float maximum) {
            const float saved = store.ReadFloat(section, key, value);
            return std::isfinite(saved) ? std::clamp(saved, minimum, maximum) : value;
        };
        style.scale = number("scale", style.scale, 0.5f, ui::text_max_scale);
        style.font = ReadFontChoice(store, section, style.font);
        style.font_explicit = store.ReadBool(section, "font_explicit", style.font_explicit || store.ReadBool(section, "font_set", false));
        style.weight_choice = std::clamp(store.ReadInt(section, "weight", style.weight_choice), ui::text_weight_default, ui::text_weight_bold);
        style.italic = store.ReadBool(section, "italic", style.italic);
        style.letter_spacing = number("letter_spacing", style.letter_spacing, -1, 6);
        style.shadow.enabled = store.ReadBool(section, "shadow_enabled", style.shadow.enabled);
        style.shadow.color = store.ReadArgb(section, "shadow_color", style.shadow.color);
        style.shadow.offset_x = number("shadow_x", style.shadow.offset_x, -10, 10);
        style.shadow.offset_y = number("shadow_y", style.shadow.offset_y, -10, 10);
        style.shadow.soft = store.ReadBool(section, "shadow_soft", style.shadow.soft);
        style.stroke.enabled = store.ReadBool(section, "stroke_enabled", style.stroke.enabled);
        style.stroke.color = store.ReadArgb(section, "stroke_color", style.stroke.color);
        style.glow.enabled = store.ReadBool(section, "glow_enabled", style.glow.enabled);
        style.glow.color = store.ReadArgb(section, "glow_color", style.glow.color);
        style.sheen.enabled = store.ReadBool(section, "sheen_enabled", style.sheen.enabled);
        style.sheen.color = store.ReadArgb(section, "sheen_color", style.sheen.color);
        style.sheen.width = number("sheen_width", style.sheen.width, 2, 120);
        style.sheen.speed = number("sheen_speed", style.sheen.speed, 0, 2);
        return style;
    }

    auto SaveTextStyle(const settings_store& store, const char* section, const ui::text_style& style) -> void
    {
        store.WriteFloat(section, "scale", style.scale);
        WriteFontChoice(store, section, style.font);
        store.WriteBool(section, "font_explicit", style.font_explicit);
        store.WriteInt(section, "weight", style.weight_choice);
        store.WriteBool(section, "italic", style.italic);
        store.WriteFloat(section, "letter_spacing", style.letter_spacing);
        store.WriteBool(section, "shadow_enabled", style.shadow.enabled);
        store.WriteArgb(section, "shadow_color", style.shadow.color);
        store.WriteFloat(section, "shadow_x", style.shadow.offset_x);
        store.WriteFloat(section, "shadow_y", style.shadow.offset_y);
        store.WriteBool(section, "shadow_soft", style.shadow.soft);
        store.WriteBool(section, "stroke_enabled", style.stroke.enabled);
        store.WriteArgb(section, "stroke_color", style.stroke.color);
        store.WriteBool(section, "glow_enabled", style.glow.enabled);
        store.WriteArgb(section, "glow_color", style.glow.color);
        store.WriteBool(section, "sheen_enabled", style.sheen.enabled);
        store.WriteArgb(section, "sheen_color", style.sheen.color);
        store.WriteFloat(section, "sheen_width", style.sheen.width);
        store.WriteFloat(section, "sheen_speed", style.sheen.speed);
    }

    auto InheritGlobalTextStyle(const settings_store& store, const ui::text_style& fallback) -> ui::text_style
    {
        auto style = fallback;
        const auto global = LoadTextStyle(store, ui::global_text_section, fallback);
        for (size_t i = 0; i < text_property_count; ++i)
        {
            const auto property = static_cast<text_property>(i);
            if (property == text_property::font || store.ReadBool(ui::global_text_section, TextPropertyFlag(property), false))
                CopyTextProperty(style, global, property);
        }
        return style;
    }
}
