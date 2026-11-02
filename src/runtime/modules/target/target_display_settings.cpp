#include "target_display_settings.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace xiui
{
    auto TargetIdSuffix(const uint32_t serverId, const int format) -> std::string
    {
        if (serverId == 0 || format == 0) return {};
        char value[24]{};
        if (format == 2) std::snprintf(value, sizeof(value), "%X", serverId);
        else std::snprintf(value, sizeof(value), "%u", serverId);
        const std::string number = value;
        return " [" + number.substr(number.size() > 3 ? number.size() - 3 : 0) + "]";
    }

    auto target_display_settings::Load(const settings_store& store, const D3DCOLOR top, const D3DCOLOR bottom,
        const ui::meter_style& meter, const module_text_profile& text) -> void
    {
        this->name = store.ReadBool("target", "show_name", true); this->casts = store.ReadBool("target", "show_casts", true);
        this->lock = store.ReadBool("target", "show_lock", true);
        this->percent_players = store.ReadBool("target", "percent_players", true); this->percent_mobs = store.ReadBool("target", "percent_mobs", true);
        this->id_format = std::clamp(store.ReadInt("target", "enemy_id_format", 0), 0, 2);
        this->mob_level = store.ReadBool("target", "mob_level", false);
        this->custom_cast_meter = store.ReadBool("target.cast", "custom_meter", false);
        this->custom_cast_fill = store.ReadBool("target.cast", "custom_fill", false);
        const auto number = [&](const char* key, float fallback, float minimum, float maximum) {
            const auto value = store.ReadFloat("target.cast", key, fallback);
            return std::clamp(std::isfinite(value) ? value : fallback, minimum, maximum);
        };
        this->cast_height = number("height", 8, 2, 32); this->cast_spacing = number("spacing", 3, 0, 32);
        this->cast_width = number("width", 0, 0, 1600);
        this->cast_offset_x = number("offset_x", 0, -1600, 1600); this->cast_offset_y = number("offset_y", 0, -1600, 1600);
        this->cast_placement = std::clamp(store.ReadInt("target.cast", "placement", 0), 0, 1);
        this->cast_top = store.ReadArgb("target.cast", "fill_top", top); this->cast_bottom = store.ReadArgb("target.cast", "fill_bottom", bottom);
        this->cast_meter = LoadMeterStyle(store, "meter.target.cast", meter);
        this->cast_text = LoadTextProfile(store, "text.target.cast", text);
        this->entity_fills = store.ReadBool("target.entity_fills", "enabled", false);
        this->fill_class = std::clamp(store.ReadInt("target.entity_fills", "selected_class", 0), 0, static_cast<int>(entity_class_count) - 1);
        this->name_class = std::clamp(store.ReadInt("entity.colors", "selected_class", 0), 0, static_cast<int>(entity_class_count) - 1);
        shared_entity_name_palette = {};
        shared_entity_name_palette.enabled = store.ReadBool("entity.colors", "enabled", false);
        for (size_t index = 0; index < entity_class_count; ++index)
        {
            const auto key = std::string{entity_class_keys[index]};
            this->fill_top[index] = store.ReadArgb("target.entity_fills", (key + "_top").c_str(), top);
            this->fill_bottom[index] = store.ReadArgb("target.entity_fills", (key + "_bottom").c_str(), bottom);
            shared_entity_name_palette.colors[index] = store.ReadArgb("entity.colors", key.c_str(), shared_entity_name_palette.colors[index]);
        }
    }

    auto target_display_settings::Save(const settings_store& store) const -> void
    {
        for (const auto& [key, value] : {std::pair{"show_name", this->name}, {"show_casts", this->casts}, {"show_lock", this->lock},
            {"percent_players", this->percent_players}, {"percent_mobs", this->percent_mobs}}) store.WriteBool("target", key, value);
        store.WriteInt("target", "enemy_id_format", this->id_format);
        store.WriteBool("target", "mob_level", this->mob_level);
        store.WriteBool("target.cast", "custom_meter", this->custom_cast_meter); store.WriteBool("target.cast", "custom_fill", this->custom_cast_fill);
        store.WriteFloat("target.cast", "height", this->cast_height); store.WriteFloat("target.cast", "spacing", this->cast_spacing);
        store.WriteFloat("target.cast", "width", this->cast_width);
        store.WriteFloat("target.cast", "offset_x", this->cast_offset_x); store.WriteFloat("target.cast", "offset_y", this->cast_offset_y);
        store.WriteInt("target.cast", "placement", this->cast_placement);
        store.WriteArgb("target.cast", "fill_top", this->cast_top); store.WriteArgb("target.cast", "fill_bottom", this->cast_bottom);
        SaveMeterStyle(store, "meter.target.cast", this->cast_meter); SaveTextProfile(store, "text.target.cast", this->cast_text);
        store.WriteBool("target.entity_fills", "enabled", this->entity_fills); store.WriteInt("target.entity_fills", "selected_class", this->fill_class);
        store.WriteBool("entity.colors", "enabled", shared_entity_name_palette.enabled); store.WriteInt("entity.colors", "selected_class", this->name_class);
        for (size_t index = 0; index < entity_class_count; ++index)
        {
            const auto key = std::string{entity_class_keys[index]};
            store.WriteArgb("target.entity_fills", (key + "_top").c_str(), this->fill_top[index]);
            store.WriteArgb("target.entity_fills", (key + "_bottom").c_str(), this->fill_bottom[index]);
            store.WriteArgb("entity.colors", key.c_str(), shared_entity_name_palette.colors[index]);
        }
    }

    auto target_display_settings::Option(const size_t index) -> module_option
    {
        if (index >= layout_options)
        {
            static constexpr const char* placements[]{"Follow Target", "Independent"};
            switch (index - layout_options)
            {
            case 0: return module_option::Choice("Cast position", "XI Theme places casts below HP; XIV 1.0 replaces the target name while casting. Independent uses its saved Movement position. Casting never shifts other Target elements.", &this->cast_placement, placements, 2).Ordered(200, true).VisibleWhen(this->casts);
            case 1: return module_option::Slider("Cast width", "Zero follows the Target HP width. Attached XIV 1.0 casts always fit the name row.", &this->cast_width, 0, 1600, 1).Ordered(300, true).VisibleWhen(this->casts);
            default:
                auto option = index - layout_options == 2 ?
                    module_option::Slider("Cast X offset", "Horizontal adjustment for attached XI Theme casts. XIV 1.0 uses the name row; independent positioning uses Movement.", &this->cast_offset_x, -1600, 1600, 1).Ordered(320, true) :
                    module_option::Slider("Cast Y offset", "Vertical adjustment for attached XI Theme casts. XIV 1.0 uses the name row; independent positioning uses Movement.", &this->cast_offset_y, -1600, 1600, 1).Ordered(321);
                return option.VisibleWhen(this->casts && this->cast_placement == 0);
            }
        }
        if (index >= base_options)
        {
            if (index - base_options == 1) return {};
            auto option = MeterStyleOption(index - base_options, this->cast_meter);
            return option.VisibleWhen(this->casts && this->custom_cast_meter);
        }
        static constexpr const char* formats[]{"Off", "Decimal suffix", "Hex suffix"};
        switch (index)
        {
        case 0: return module_option::Toggle("Name", "Show the main target's name.", &this->name);
        case 1: return module_option::Toggle("Cast bar", "Show the main target's observed spell or action.", &this->casts);
        case 2: return module_option::Toggle("Lock highlight", "Highlight the HP bar when the native camera is locked on.", &this->lock);
        case 3: return module_option::Choice("Enemy ID", "Last three characters of the monster's native server ID, matching XIUI+; this is not entity index or mob level.", &this->id_format, formats, 3);
        case 4: return module_option::Toggle("Player HP percent", "Include player entities when main Target HP percent is enabled.", &this->percent_players);
        case 5: return module_option::Toggle("Monster HP percent", "Include monster entities when main Target HP percent is enabled.", &this->percent_mobs);
        case 6: return module_option::Slider("Cast height", "Height of the main Target cast bar. XIV 1.0 keeps its compact track at most six pixels tall.", &this->cast_height, 2, 32, 1).Ordered(301).VisibleWhen(this->casts);
        case 7: return module_option::Slider("Cast spacing", "Gap below HP in XI Theme. Attached XIV 1.0 casts use the name row.", &this->cast_spacing, 0, 32, 1).Ordered(310, true).VisibleWhen(this->casts && this->cast_placement == 0);
        case 8: return module_option::Toggle("Independent cast style", "Configure the cast meter separately from the HP bar.", &this->custom_cast_meter).VisibleWhen(this->casts);
        case 9: return module_option::Toggle("Custom cast gradient", "Replace the theme accent with a dedicated cast fill.", &this->custom_cast_fill).VisibleWhen(this->casts);
        case 10: return module_option::Color("Cast fill top", "Upper custom cast fill.", &this->cast_top).VisibleWhen(this->casts && this->custom_cast_fill);
        case 11: return module_option::Color("Cast fill bottom", "Lower custom cast fill.", &this->cast_bottom).VisibleWhen(this->casts && this->custom_cast_fill);
        case 12: return module_option::Toggle("Entity HP gradients", "Use these gradients for main Target and followers of target colors.", &this->entity_fills);
        case 13: return module_option::Choice("HP entity type", "Choose the entity/claim class to customize.", &this->fill_class, entity_class_names, entity_class_count).VisibleWhen(this->entity_fills);
        case 14: return module_option::Color("Entity HP top", "Upper entity-specific HP gradient.", &this->fill_top[this->fill_class]).VisibleWhen(this->entity_fills);
        case 15: return module_option::Color("Entity HP bottom", "Lower entity-specific HP gradient.", &this->fill_bottom[this->fill_class]).VisibleWhen(this->entity_fills);
        case 16: return module_option::Toggle("Custom entity names", "Share these colors with Target, Enemy List and other native entity-name consumers.", &shared_entity_name_palette.enabled);
        case 17: return module_option::Choice("Name entity type", "Choose the shared name/claim class to customize.", &this->name_class, entity_class_names, entity_class_count).VisibleWhen(shared_entity_name_palette.enabled);
        case 18: return module_option::Color("Entity name color", "Tint names while preserving configured gradients and text effects.", &shared_entity_name_palette.colors[this->name_class]).VisibleWhen(shared_entity_name_palette.enabled);
        default: return module_option::Toggle("Mob level", "Append the local database level/range by exact server ID when known.", &this->mob_level);
        }
    }
}
