#include "global_text.hpp"

#include "modules/module.hpp"

namespace xiui::ui
{
    auto global_text_settings::Load(const settings_store& store) -> void
    {
        *this = {};
        this->style = LoadTextStyle(store, global_text_section, this->style);
        for (size_t i = 0; i < option_count; ++i)
            this->applied[i] = store.ReadBool(global_text_section, TextPropertyFlag(static_cast<text_property>(i)), false);
    }

    auto global_text_settings::Save(const settings_store& store) const -> void
    {
        SaveTextStyle(store, global_text_section, this->style);
        for (size_t i = 0; i < option_count; ++i)
            store.WriteBool(global_text_section, TextPropertyFlag(static_cast<text_property>(i)), this->applied[i]);
    }

    auto global_text_settings::Apply(const size_t index, text_style& target) const -> void
    {
        if (index < option_count) CopyTextProperty(target, this->style, static_cast<text_property>(index));
    }

    auto global_text_settings::Option(const size_t index) -> module_option
    {
        constexpr auto scope = "Apply to all HUD text. Later module text edits override this property.";
        switch (static_cast<text_property>(index))
        {
        case text_property::size: return TextSizeOption("Global text size (px)", scope, &this->style.scale).Ordered(101);
        case text_property::font: return module_option::Dropdown("Global font", scope, &this->style.font, font_names, font_count).Ordered(100, true);
        case text_property::letter_spacing: return module_option::Slider("Letter spacing", scope, &this->style.letter_spacing, -1, 6, 0.25f).Ordered(201);
        case text_property::shadow: return module_option::Toggle("Drop shadow", scope, &this->style.shadow.enabled).Ordered(600, true);
        case text_property::shadow_color: return module_option::Color("Shadow color", scope, &this->style.shadow.color).Ordered(601).VisibleWhen(this->style.shadow.enabled);
        case text_property::shadow_x: return module_option::Slider("Shadow X offset", scope, &this->style.shadow.offset_x, -10, 10, 1).Ordered(606, true).VisibleWhen(this->style.shadow.enabled);
        case text_property::shadow_y: return module_option::Slider("Shadow Y offset", scope, &this->style.shadow.offset_y, -10, 10, 1).Ordered(607).VisibleWhen(this->style.shadow.enabled);
        case text_property::soft_shadow: return module_option::Toggle("Soft shadow", scope, &this->style.shadow.soft).Ordered(604).VisibleWhen(this->style.shadow.enabled);
        case text_property::outline: return module_option::Toggle("Stroke outline", scope, &this->style.stroke.enabled).Ordered(650, true);
        case text_property::outline_color: return module_option::Color("Outline color", scope, &this->style.stroke.color).Ordered(651).VisibleWhen(this->style.stroke.enabled);
        case text_property::glow: return module_option::Toggle("Outer glow", scope, &this->style.glow.enabled).Ordered(700, true);
        case text_property::glow_color: return module_option::Color("Glow color", scope, &this->style.glow.color).Ordered(701).VisibleWhen(this->style.glow.enabled);
        case text_property::sheen: return module_option::Toggle("Animated sheen", scope, &this->style.sheen.enabled).Ordered(750);
        case text_property::sheen_color: return module_option::Color("Sheen color", scope, &this->style.sheen.color).Ordered(751).VisibleWhen(this->style.sheen.enabled);
        case text_property::sheen_width: return module_option::Slider("Sheen width", scope, &this->style.sheen.width, 2, 120, 1).Ordered(752, true).VisibleWhen(this->style.sheen.enabled);
        case text_property::sheen_speed: return module_option::Slider("Sheen speed", scope, &this->style.sheen.speed, 0, 2, 0.05f).Ordered(753).VisibleWhen(this->style.sheen.enabled);
        default: return {};
        }
    }
}
