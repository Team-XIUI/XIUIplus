#include "config_text.hpp"

#include "modules/module.hpp"
#include "settings/text_style_settings.hpp"

#include <algorithm>
#include <cmath>

namespace xiui::ui
{
    namespace
    {
        constexpr char section[] = "config.text";
    }

    auto config_text_settings::Load(const settings_store& store) -> void
    {
        *this = {};
        const auto number = [&](const char* key, const float fallback, const float minimum, const float maximum) {
            const float value = store.ReadFloat(section, key, fallback);
            return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
        };
        this->font = xiui::ReadFontChoice(store, section, this->font);
        this->size = number("size", this->size, 8, 18);
        this->letter_spacing = number("letter_spacing", this->letter_spacing, -1, 4);
        this->shadow.enabled = store.ReadBool(section, "shadow_enabled", this->shadow.enabled);
        this->shadow.color = store.ReadArgb(section, "shadow_color", this->shadow.color);
        this->shadow.offset_x = number("shadow_x", this->shadow.offset_x, -10, 10);
        this->shadow.offset_y = number("shadow_y", this->shadow.offset_y, -10, 10);
        this->shadow.soft = store.ReadBool(section, "shadow_soft", this->shadow.soft);
        this->outline = std::clamp(store.ReadInt(section, "outline", this->outline), 0, 2);
        this->outline_color = store.ReadArgb(section, "outline_color", this->outline_color);
        this->glow.enabled = store.ReadBool(section, "glow_enabled", this->glow.enabled);
        this->glow.color = store.ReadArgb(section, "glow_color", this->glow.color);
        this->sheen.enabled = store.ReadBool(section, "sheen_enabled", this->sheen.enabled);
        this->sheen.color = store.ReadArgb(section, "sheen_color", this->sheen.color);
        this->sheen.width = number("sheen_width", this->sheen.width, 2, 120);
        this->sheen.speed = number("sheen_speed", this->sheen.speed, 0, 2);
    }

    auto config_text_settings::Save(const settings_store& store) const -> void
    {
        xiui::WriteFontChoice(store, section, this->font);
        store.WriteFloat(section, "size", this->size);
        store.WriteFloat(section, "letter_spacing", this->letter_spacing);
        store.WriteBool(section, "shadow_enabled", this->shadow.enabled);
        store.WriteArgb(section, "shadow_color", this->shadow.color);
        store.WriteFloat(section, "shadow_x", this->shadow.offset_x);
        store.WriteFloat(section, "shadow_y", this->shadow.offset_y);
        store.WriteBool(section, "shadow_soft", this->shadow.soft);
        store.WriteInt(section, "outline", this->outline);
        store.WriteArgb(section, "outline_color", this->outline_color);
        store.WriteBool(section, "glow_enabled", this->glow.enabled);
        store.WriteArgb(section, "glow_color", this->glow.color);
        store.WriteBool(section, "sheen_enabled", this->sheen.enabled);
        store.WriteArgb(section, "sheen_color", this->sheen.color);
        store.WriteFloat(section, "sheen_width", this->sheen.width);
        store.WriteFloat(section, "sheen_speed", this->sheen.speed);
    }

    auto config_text_settings::Option(const size_t index) -> module_option
    {
        static constexpr const char* outlines[]{"Off", "Buttons only", "All text"};
        switch (index)
        {
        case 0: return module_option::Dropdown("Config font", "Font for configuration labels, buttons, menus and dialogs.", &this->font, FontNames(), FontCount()).Ordered(100, true);
        case 1: return module_option::Slider("Letter spacing", "Additional pixels between configuration letters.", &this->letter_spacing, -1, 4, 0.1f, module_option_group::text).Ordered(201);
        case 2: return module_option::Toggle("Drop shadow", "Shadow behind configuration text.", &this->shadow.enabled, module_option_group::effects).Ordered(600, true);
        case 3: return module_option::Color("Shadow color", "Shadow color and opacity.", &this->shadow.color).Ordered(601).VisibleWhen(this->shadow.enabled);
        case 4: return module_option::Slider("Shadow X offset", "Horizontal shadow offset in pixels.", &this->shadow.offset_x, -10, 10, 1, module_option_group::effects).Ordered(606, true).VisibleWhen(this->shadow.enabled);
        case 5: return module_option::Slider("Shadow Y offset", "Vertical shadow offset in pixels.", &this->shadow.offset_y, -10, 10, 1, module_option_group::effects).Ordered(607).VisibleWhen(this->shadow.enabled);
        case 6: return module_option::Toggle("Soft shadow", "Soften the shadow edge.", &this->shadow.soft, module_option_group::effects).Ordered(604).VisibleWhen(this->shadow.enabled);
        case 7: return module_option::Choice("Stroke outline", "Outline button labels, all configuration text, or neither.", &this->outline, outlines, std::size(outlines), module_option_group::effects).Ordered(650, true);
        case 8: return module_option::Color("Outline color", "Stroke color and opacity.", &this->outline_color).Ordered(651).VisibleWhen(this->outline != 0);
        case 9: return module_option::Toggle("Outer glow", "Glow around configuration text.", &this->glow.enabled, module_option_group::effects).Ordered(700, true);
        case 10: return module_option::Color("Glow color", "Glow color and opacity.", &this->glow.color).Ordered(701).VisibleWhen(this->glow.enabled);
        case 11: return module_option::Toggle("Animated sheen", "Moving highlight across configuration text.", &this->sheen.enabled, module_option_group::effects).Ordered(750);
        case 12: return module_option::Color("Sheen color", "Highlight color and opacity.", &this->sheen.color).Ordered(751).VisibleWhen(this->sheen.enabled);
        case 13: return module_option::Slider("Sheen width", "Highlight width in pixels.", &this->sheen.width, 2, 120, 1, module_option_group::effects).Ordered(752, true).VisibleWhen(this->sheen.enabled);
        case 14: return module_option::Slider("Sheen speed", "Highlight cycles per second.", &this->sheen.speed, 0, 2, 0.05f, module_option_group::effects).Ordered(753).VisibleWhen(this->sheen.enabled);
        case 15: return module_option::Slider("Config text size (px)", "Base configuration text size. Headings and smaller help text retain their relative sizes.", &this->size, 8, 18, 1, module_option_group::text).Ordered(101);
        default: return {};
        }
    }
}
