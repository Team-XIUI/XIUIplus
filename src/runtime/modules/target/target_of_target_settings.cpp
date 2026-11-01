#include "target_of_target_settings.hpp"
#include "modules/bar_config.hpp"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace xiui
{
    auto target_of_target_settings::Load(const settings_store& store, const bool showDistance, const bool showPercent, const bool showLabel,
        const char* section, const char* stylePrefix) -> void
    {
        const bool recipient = std::string_view{section} == "target.target_of_target";
        const bool legacy = recipient && store.ReadInt(section, "layout_version", 0) < 1 && !store.ReadString(section, "width", "").empty();
        this->enabled = store.ReadBool(section, "enabled", recipient ? true : this->enabled);
        this->health = store.ReadBool(section, "health", recipient ? true : this->health);
        this->distance = store.ReadBool(section, "distance", recipient ? legacy && showDistance : this->distance);
        this->percent = store.ReadBool(section, "percent", recipient ? legacy && showPercent : this->percent);
        this->label = store.ReadBool(section, "label", recipient ? legacy && showLabel : this->label);
        this->cast = store.ReadBool(section, "cast", recipient ? legacy : this->cast);
        this->statuses = store.ReadBool(section, "statuses", recipient ? legacy : this->statuses);
        this->attack = store.ReadBool(section, "attack", recipient ? legacy : this->attack);
        this->mob_level = store.ReadBool(section, "mob_level", false);
        const auto read = [&](const char* key, float fallback, float minimum, float maximum) {
            const float value = store.ReadFloat(section, key, fallback);
            return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
        };
        this->width = read("width", 0, 0, 1200);
        this->bar_height = read("bar_height", 0, 0, 64);
        this->gap = read("gap", recipient ? legacy ? 6.0f : 8.0f : this->gap, 0, 64);
        this->offset_x = read("offset_x", 0, -512, 512);
        this->offset_y = read("offset_y", 0, -512, 512);
        const bool moved = legacy && (this->width != 0 || this->bar_height != 0 || this->gap != 6 || this->offset_x != 0 || this->offset_y != 0);
        this->placement = std::clamp(store.ReadInt(section, "placement", recipient ? moved ? below : inline_right : this->placement), inline_right, detached);
        if (legacy && !moved)
        {
            this->gap = 8;
            if (this->health && this->distance && this->percent && this->label && this->cast && this->statuses && this->attack)
                this->distance = this->percent = this->label = this->cast = this->statuses = this->attack = false;
        }
        this->arrow = store.ReadBool(section, "arrow", recipient ? true : this->arrow);
        this->name = store.ReadBool(section, "name", recipient ? true : this->name);
        this->prefix = store.ReadBool(section, "prefix", recipient ? false : this->prefix);
        this->follow_meter = store.ReadBool(section, "follow_meter", recipient ? true : this->follow_meter);
        this->follow_fill = store.ReadBool(section, "follow_fill", recipient ? true : this->follow_fill);
        this->follow_text = store.ReadBool(section, "follow_text", recipient ? true : this->follow_text);
        this->tint_name = store.ReadBool(section, "tint_name", recipient ? true : this->tint_name);
        this->arrow_size = read("arrow_size", 24, 8, 64);
        this->arrow_gap = read("arrow_gap", 8, 0, 64);
        this->arrow_x = read("arrow_x", 0, -128, 128);
        this->arrow_y = read("arrow_y", 0, -128, 128);
        this->cast_height = read("cast_height", 8, 2, 32);
        this->alignment = std::clamp(store.ReadInt(section, "alignment", 1), 0, 2);
        this->name_placement = std::clamp(store.ReadInt(section, "name_placement", 0), 0, 2);
        this->selected_text = std::clamp(store.ReadInt(section, "selected_text", 0), 0, 3);
        this->fill_top = store.ReadArgb(section, "fill_top", this->fill_top);
        this->fill_bottom = store.ReadArgb(section, "fill_bottom", this->fill_bottom);
        this->arrow_color = store.ReadArgb(section, "arrow_color", 0xFFFFFFFF);
        this->meter = LoadMeterStyle(store, (std::string{"meter."} + stylePrefix).c_str(), this->meter);
        const auto inheritedText = this->text;
        constexpr const char* profiles[]{"name", "distance", "labels", "values"};
        for (size_t i = 0; i < this->text.size(); ++i)
            this->text[i] = LoadTextProfile(store, (std::string{"text."} + stylePrefix + "." + profiles[i]).c_str(), this->text[i]);
        if (recipient && store.ReadInt(section, "text_scale_version", 0) < 1)
        {
            const float legacyScale = read("text_scale", 1, 0.5f, 2);
            if (legacyScale != 1)
            {
                if (this->follow_text) this->text = inheritedText;
                for (auto& profile : this->text)
                    profile.style.scale = std::clamp(profile.style.scale * legacyScale, ui::text_min_scale, ui::text_max_scale);
                this->follow_text = false;
            }
        }
        if (recipient)
        {
            if (store.ReadInt(section, "shared_target_text_version", 0) < 1 && !this->follow_text &&
                this->text[0].placement == 2 && this->text[0].offset_x == -82 && this->text[0].offset_y == 16)
                this->follow_text = true;
            const bool customized = this->placement != inline_right || !this->arrow || this->offset_x != 0 || this->offset_y != 0;
            this->theme_layout_customized = store.ReadInt(section, "theme_layout_version", 0) >= 1 ?
                store.ReadBool(section, "theme_layout_customized", customized) : customized;
            this->ApplyThemeDefaults(store.ReadInt("appearance", "theme", 0) == ui::xiv1_appearance);
        }
    }

    auto target_of_target_settings::ApplyThemeDefaults(const bool xiv1) -> void
    {
        this->theme_layout_xiv1 = xiv1;
        if (this->theme_layout_customized) return;
        this->placement = xiv1 ? below : inline_right;
        this->arrow = !xiv1;
        this->offset_x = xiv1 ? xiv1_offset_x : 0;
        this->offset_y = xiv1 ? xiv1_offset_y : 0;
    }

    auto target_of_target_settings::Save(const settings_store& store, const char* section, const char* stylePrefix) const -> void
    {
        store.WriteInt(section, "layout_version", 1);
        store.WriteInt(section, "text_scale_version", 1);
        if (std::string_view{section} == "target.target_of_target")
        {
            store.WriteInt(section, "theme_layout_version", 2);
            store.WriteInt(section, "shared_target_text_version", 1);
            store.WriteBool(section, "theme_layout_customized", this->theme_layout_customized ||
                this->placement != (this->theme_layout_xiv1 ? below : inline_right) || this->arrow == this->theme_layout_xiv1 ||
                this->offset_x != (this->theme_layout_xiv1 ? xiv1_offset_x : 0) ||
                this->offset_y != (this->theme_layout_xiv1 ? xiv1_offset_y : 0));
        }
        store.WriteBool(section, "mob_level", this->mob_level);
        for (const auto& [key, value] : {std::pair{"enabled", this->enabled}, {"health", this->health}, {"distance", this->distance},
            {"percent", this->percent}, {"label", this->label}, {"cast", this->cast}, {"statuses", this->statuses}, {"attack", this->attack},
            {"arrow", this->arrow}, {"name", this->name}, {"prefix", this->prefix}, {"follow_meter", this->follow_meter},
            {"follow_fill", this->follow_fill}, {"follow_text", this->follow_text}, {"tint_name", this->tint_name}})
            store.WriteBool(section, key, value);
        for (const auto& [key, value] : {std::pair{"width", this->width}, {"bar_height", this->bar_height}, {"gap", this->gap},
            {"offset_x", this->offset_x}, {"offset_y", this->offset_y}, {"arrow_size", this->arrow_size}, {"arrow_gap", this->arrow_gap},
            {"arrow_x", this->arrow_x}, {"arrow_y", this->arrow_y}, {"cast_height", this->cast_height}})
            store.WriteFloat(section, key, value);
        store.WriteInt(section, "placement", this->placement);
        store.WriteInt(section, "alignment", this->alignment);
        store.WriteInt(section, "name_placement", this->name_placement);
        store.WriteInt(section, "selected_text", this->selected_text);
        store.WriteArgb(section, "fill_top", this->fill_top);
        store.WriteArgb(section, "fill_bottom", this->fill_bottom);
        store.WriteArgb(section, "arrow_color", this->arrow_color);
        SaveMeterStyle(store, (std::string{"meter."} + stylePrefix).c_str(), this->meter);
        constexpr const char* profiles[]{"name", "distance", "labels", "values"};
        for (size_t i = 0; i < this->text.size(); ++i)
            SaveTextProfile(store, (std::string{"text."} + stylePrefix + "." + profiles[i]).c_str(), this->text[i]);
    }

    auto target_of_target_settings::ConfigSections(void) -> std::span<const module_config_section>
    {
        return BarConfigSections();
    }

    auto target_of_target_settings::Contains(const size_t index, const size_t section) const -> bool
    {
        if (index >= option_count || section >= ConfigSections().size()) return false;
        const auto topic = static_cast<bar_config_section>(section);
        if (index >= base_options + meter_style_option_count) return topic == bar_config_section::text;
        if (index == base_options + 1) return topic == bar_config_section::bar_style;
        if (index >= base_options) return topic == MeterConfigSection(index - base_options);
        if (index <= 7 || index == 14 || index == 19 || index == 20 || index == 32) return topic == bar_config_section::general;
        if ((index >= 8 && index <= 18) || index == 30 || index == 31) return topic == bar_config_section::layout;
        if (index == 21) return topic == bar_config_section::bar_style || topic == bar_config_section::animation || topic == bar_config_section::effects;
        if (index >= 24 && index <= 27) return topic == bar_config_section::colors;
        return topic == bar_config_section::text;
    }

    auto target_of_target_settings::Option(const size_t index) -> module_option
    {
        if (index >= base_options + meter_style_option_count)
        {
            const auto field = index - base_options - meter_style_option_count;
            auto option = TextProfileOption(field, this->text[this->selected_text]);
            return option.VisibleWhen(!this->follow_text && !(this->selected_text == 0 && this->tint_name && (field == 5 || field == 6)));
        }
        if (index >= base_options)
        {
            const auto field = index - base_options;
            auto option = MeterStyleOption(field, this->meter);
            if (field == 1)
            {
                option.label = "Automatic bar height";
                option.description = "Base height when Follow target style is off and Bar height is zero (automatic).";
                return option.VisibleWhen(!this->follow_meter && this->bar_height == 0);
            }
            return option.VisibleWhen(!this->follow_meter);
        }
        static constexpr const char* layouts[]{"Inline right", "Inline left", "Below", "Above", "Detached"};
        static constexpr const char* anchors[]{"Top", "Center", "Bottom"};
        static constexpr const char* names[]{"Above bar", "Inside bar", "Below bar"};
        static constexpr const char* texts[]{"Name", "Distance", "HP label", "HP value"};
        const auto arrowOption = [&](module_option option, const bool detail = true) {
            return option.VisibleWhen((this->placement == inline_right || this->placement == inline_left) && (!detail || this->arrow));
        };
        switch (index)
        {
        case 0: return module_option::Toggle("Enabled", "Show your target's last observed action recipient.", &this->enabled).Ordered(-50);
        case 1: return module_option::Toggle("HP bar", "Show the recipient's health bar when available.", &this->health);
        case 2: return module_option::Toggle("HP percent", "Show health percent in the recipient's bar.", &this->percent).VisibleWhen(this->health);
        case 3: return module_option::Toggle("HP label", "Show HP inside the recipient's bar.", &this->label).VisibleWhen(this->health);
        case 4: return module_option::Toggle("Distance", "Show the recipient's distance.", &this->distance);
        case 5: return module_option::Toggle("Cast bar", "Show the recipient's observed spell or action.", &this->cast);
        case 6: return module_option::Toggle("Status effects", "Show the recipient's known status icons.", &this->statuses);
        case 7: return module_option::Toggle("Attack timing", "Include the recipient when Target attack timing is enabled.", &this->attack);
        case 8: return module_option::Slider("Width", "Zero uses one-third width inline, or full width when stacked.", &this->width, 0, 1200, 1).Ordered(100, true);
        case 9: return module_option::Slider("Bar height", "Zero follows a compact main-bar height, or Automatic bar height when using a custom style.", &this->bar_height, 0, 64, 1).Ordered(101);
        case 10: return module_option::Slider("Spacing", "Distance from the main target to the arrow or recipient.", &this->gap, 0, 64, 1).Ordered(300, true);
        case 11: return module_option::Slider("X offset", "Move the recipient left or right.", &this->offset_x, -512, 512, 1).Ordered(210, true);
        case 12: return module_option::Slider("Y offset", "Move the recipient up or down, including overlap.", &this->offset_y, -512, 512, 1).Ordered(211);
        case 13: return module_option::Choice("Position", "Link the bars or move Target of Target independently when detached.", &this->placement, layouts, std::size(layouts)).Ordered(200);
        case 14: return arrowOption(module_option::Toggle("Connecting arrow", "Use the original XIUI arrow for inline layouts.", &this->arrow), false);
        case 15: return arrowOption(module_option::Slider("Arrow size", "Connecting arrow size in pixels.", &this->arrow_size, 8, 64, 1).Ordered(110));
        case 16: return arrowOption(module_option::Slider("Arrow gap", "Space between the arrow and the recipient.", &this->arrow_gap, 0, 64, 1).Ordered(302));
        case 17: return arrowOption(module_option::Slider("Arrow X offset", "Nudge the arrow without moving the recipient.", &this->arrow_x, -128, 128, 1).Ordered(230, true));
        case 18: return arrowOption(module_option::Slider("Arrow Y offset", "Nudge the arrow up or down.", &this->arrow_y, -128, 128, 1).Ordered(231));
        case 19: return module_option::Toggle("Name", "Show the recipient's name.", &this->name);
        case 20: return module_option::Toggle("Name prefix", "Add Target: before the recipient's name.", &this->prefix).VisibleWhen(this->name);
        case 21: return module_option::Toggle("Follow target style", "Share the main target's meter style, animation and effects.", &this->follow_meter).Ordered(-40);
        case 22: return module_option::Toggle("Follow target text", "Share the main target's text profiles instead of independent ones.", &this->follow_text).Ordered(-40);
        case 23: return module_option::Toggle("Game name colors", "Tint the name by claim and entity type while retaining its effects.", &this->tint_name);
        case 24: return module_option::Toggle("Follow target colors", "Use the main target's HP fill gradient.", &this->follow_fill).Ordered(-40);
        case 25: case 26:
        {
            auto option = module_option::Color(index == 25 ? "Fill top" : "Fill bottom", "Turn off Follow target colors to edit the gradient.", index == 25 ? &this->fill_top : &this->fill_bottom);
            return option.VisibleWhen(!this->follow_fill);
        }
        case 27: return arrowOption(module_option::Color("Arrow color", "Tint and opacity for the connecting arrow.", &this->arrow_color));
        case 28: return module_option::Choice("Name position", "Place the name above, inside or below its HP bar.", &this->name_placement, names, std::size(names)).Ordered(200).VisibleWhen(this->name);
        case 29: return module_option::Element("Selected text", "Choose the independent recipient text profile to edit.", &this->selected_text, texts, std::size(texts)).VisibleWhen(!this->follow_text);
        case 30: return module_option::Choice("Bar alignment", "Align inline HP bars by their top, center or bottom edge.", &this->alignment, anchors, std::size(anchors)).Ordered(200);
        case 31: return module_option::Slider("Cast bar height", "Height of the recipient's optional cast bar.", &this->cast_height, 2, 32, 1).Ordered(112).VisibleWhen(this->cast);
        case 32: return module_option::Toggle("Mob level", "Append a known local database level/range by exact native server ID.", &this->mob_level);
        default: return {};
        }
    }

    auto target_of_target_settings::ShowArrow(void) const -> bool
    { return this->arrow && (this->placement == inline_right || this->placement == inline_left); }

    auto target_of_target_settings::BarHeight(const float mainHeight) const -> float
    { return std::max(3.0f, this->bar_height > 0 ? this->bar_height + 2 : !this->follow_meter ? this->meter.height + 2 : mainHeight * 0.75f); }

    auto target_of_target_settings::Area(const ui::rect& module, const float precedingHeight, const float mainBarHeight) const -> ui::rect
    {
        const bool inlineBar = this->placement <= inline_left;
        const float panelWidth = this->width > 0 ? std::max(32.0f, this->width) : inlineBar ? (module.width - 14) / 3 + 14 : module.width;
        const float arrowSpace = this->ShowArrow() ? this->arrow_size + this->arrow_gap : 0;
        const float height = this->BarHeight(mainBarHeight);
        float x = module.x, y = module.y;
        if (inlineBar)
        {
            x += this->placement == inline_right ? module.width - 14 + this->gap + arrowSpace : 14 - panelWidth - this->gap - arrowSpace;
            y += (this->alignment == 0 ? 0 : this->alignment == 2 ? mainBarHeight - height : (mainBarHeight - height) * 0.5f);
        }
        else if (this->placement == below) y += precedingHeight + this->gap;
        else y -= 31 + height + 16 + this->gap;
        return {x + this->offset_x, y + this->offset_y, panelWidth, 0};
    }

    auto target_of_target_settings::Arrow(const ui::rect& module, const float mainBarHeight) const -> ui::rect
    {
        if (!this->ShowArrow()) return {};
        const float x = this->placement == inline_right ? module.x + module.width - 7 + this->gap : module.x + 7 - this->gap - this->arrow_size;
        const float y = module.y + 31 + (mainBarHeight - this->arrow_size) * 0.5f;
        return {x + this->arrow_x, y + this->arrow_y, this->arrow_size, this->arrow_size};
    }
}
