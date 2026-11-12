#include "target_mob_info.hpp"
#include "ui/game_icon_cache.hpp"
#include "ui/native_ui_assets.hpp"
#include "target_module.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace xiui
{
    auto target_mob_info_module::ResetSettingsFrom(const hud_module& defaults, const ui::theme&) -> void
    {
        const auto& source = static_cast<const target_mob_info_module&>(defaults);
        this->parent_.settings_.mob_info.groups[this->group_] = source.parent_.settings_.mob_info.groups[source.group_];
    }

    namespace
    {
        using group_kind = mob_info_group;
        constexpr const char* groupNames[]{"Job", "Behavior", "Links", "Detection", "Weaknesses", "Resistances", "Element ranks", "Status ranks", "Immunities", "Traits", "Server ID", "Spawn interval"};
        constexpr const char* groupIds[]{"mobjob", "mobbehavior", "moblinks", "mobdetection", "mobweak", "mobresist", "mobelementranks", "mobstatusranks", "mobimmunities", "mobtraits", "mobserverid", "mobspawn"};
        static_assert(std::size(groupIds) == mob_info_group_count && std::size(groupNames) == mob_info_group_count);
        constexpr auto Index(const group_kind group) -> size_t { return static_cast<size_t>(group); }
        auto GroupSection(const size_t index) -> std::string { return std::string{"target.mobinfo."} + groupIds[index]; }
        // Job, behavior, links and detection form the compact icon row and XIV 1.0 header cells.
        constexpr auto HeaderGroup(const size_t index) { return index <= Index(group_kind::detection); }
        constexpr auto DamageGroup(const size_t index) { return index == Index(group_kind::weaknesses) || index == Index(group_kind::resistances); }
        constexpr auto DefaultPlacement(const size_t index)
        { return HeaderGroup(index) || DamageGroup(index) ? mob_info_group_settings::inline_row : mob_info_group_settings::own_row; }
        // XIV 1.0 draws these Together-mode groups above the caption instead of below the Target.
        auto InHeader(const size_t index, const mob_info_group_settings& group)
        { return (HeaderGroup(index) || DamageGroup(index)) && group.placement == mob_info_group_settings::inline_row; }
        constexpr auto DefaultSpacing(const size_t index) -> float
        { return index == Index(group_kind::job) || DamageGroup(index) ? 4.0f : HeaderGroup(index) ? 2.0f : 12.0f; }
        constexpr auto DefaultEnabled(const size_t index)
        { return HeaderGroup(index) || index == Index(group_kind::immunities); }
    }

    target_mob_info::target_mob_info()
    {
        for (size_t i = 0; i < this->groups.size(); ++i)
        {
            auto& group = this->groups[i];
            group.state.enabled = DefaultEnabled(i);
            group.placement = DefaultPlacement(i);
            group.cell_spacing = DefaultSpacing(i);
            group.headings = !DamageGroup(i);
        }
    }

    auto target_mob_info::GroupName(const size_t index) -> const char* { return groupNames[index]; }
    auto target_mob_info::GroupId(const size_t index) -> const char* { return groupIds[index]; }
    auto target_mob_info::VisitTextSettings(const text_settings_visitor& visit) -> void
    {
        visit(this->text.style);
        for (auto& group : this->groups) visit(group.text.style);
    }
    auto target_mob_info::ApplyTextDefaults(const ui::theme& theme) -> void
    {
        this->text = {theme.caption_text};
        for (auto& group : this->groups) group.text = {theme.caption_text};
    }

    auto target_mob_info::Load(const settings_store& store) -> void
    {
        const auto read = [&](const char* key, bool fallback) {
            return store.ReadBool("target.mobinfo", key, store.ReadBool("mobinfo", key, fallback));
        };
        this->enabled = read("enabled", false);
        // Weaknesses and resistances replace the physical and element damage groups.
        const bool legacyDamage = read("damage", false);
        const bool damage = store.ReadBool("target.mobinfo.mobphysical", "enabled", legacyDamage) ||
            store.ReadBool("target.mobinfo.mobelements", "enabled", legacyDamage);
        const bool behavior = read("behavior", true), ranks = read("ranks", false);
        const bool defaults[]{true, behavior, behavior, behavior, damage, damage, ranks, ranks, read("immunities", true), false,
            read("server_id", false), read("respawn", false)};
        static_assert(std::size(defaults) == mob_info_group_count);
        const auto finite = [&](const char* section, const char* key, float fallback, float low, float high) {
            const auto value = store.ReadFloat(section, key, fallback);
            return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
        };
        this->spacing = finite("mobinfo", "spacing", 2, 0, 32);
        this->spacing = finite("target.mobinfo", "spacing", this->spacing, 0, 32);
        this->text = LoadTextProfile(store, "text.target.mobinfo", LoadTextProfile(store, "text.mobinfo.details", this->text));
        const bool legacyHeader = store.ReadInt("target.mobinfo", "header_layout_version", 0) < 1;
        if (legacyHeader && this->text.offset_x == 15 && this->text.offset_y == -56)
            this->text.offset_x = this->text.offset_y = 0;
        for (size_t i = 0; i < this->groups.size(); ++i)
        {
            auto& group = this->groups[i]; const auto section = GroupSection(i);
            group.state.enabled = store.ReadBool(section.c_str(), "enabled", defaults[i]);
            group.placement = std::clamp(store.ReadInt(section.c_str(), "placement", DefaultPlacement(i)), 0, 2);
            group.follow_text = store.ReadBool(section.c_str(), "follow_text", true);
            // Damage icons read by their signed amounts, so their headings start hidden.
            group.headings = store.ReadBool(section.c_str(), "headings", !DamageGroup(i));
            group.positioned = store.ReadBool(section.c_str(), "positioned", group.placement == 2);
            group.percentages = store.ReadBool(section.c_str(), "percentages", true);
            group.group_percentages = store.ReadBool(section.c_str(), "group_percentages", true);
            group.width = finite(section.c_str(), "width", 0, 0, 2000);
            group.icon_size = finite(section.c_str(), "icon_size", 0, 0, 64);
            group.cell_spacing = finite(section.c_str(), "cell_spacing", DefaultSpacing(i), 0, 64);
            if (legacyHeader && i >= Index(group_kind::behavior) && HeaderGroup(i) &&
                group.placement == mob_info_group_settings::inline_row && group.cell_spacing == 12)
                group.cell_spacing = 2;
            group.row_spacing = finite(section.c_str(), "row_spacing", 2, 0, 32);
            auto fallback = this->text; fallback.offset_x = fallback.offset_y = 0;
            group.text = LoadTextProfile(store, ("text." + section).c_str(), fallback);
            group.state.bounds = store.ReadRect(("layout." + std::string{groupIds[i]}).c_str(), {24, 218, 300, 24});
            group.state.bounds.width = std::clamp(group.state.bounds.width, 64.0f, 2000.0f);
            group.state.maximum_width = 2000;
            group.state.content_bounds = {}; group.attached_bounds = {};
        }
    }

    auto target_mob_info::Save(const settings_store& store) const -> void
    {
        store.WriteBool("target.mobinfo", "enabled", this->enabled);
        store.WriteInt("target.mobinfo", "header_layout_version", 1);
        store.WriteFloat("target.mobinfo", "spacing", this->spacing);
        SaveTextProfile(store, "text.target.mobinfo", this->text);
        for (size_t i = 0; i < this->groups.size(); ++i)
        {
            const auto& group = this->groups[i]; const auto section = GroupSection(i);
            store.WriteBool(section.c_str(), "enabled", group.state.enabled);
            store.WriteInt(section.c_str(), "placement", group.placement);
            store.WriteBool(section.c_str(), "follow_text", group.follow_text);
            store.WriteBool(section.c_str(), "headings", group.headings);
            store.WriteBool(section.c_str(), "positioned", group.positioned);
            store.WriteBool(section.c_str(), "percentages", group.percentages);
            store.WriteBool(section.c_str(), "group_percentages", group.group_percentages);
            store.WriteFloat(section.c_str(), "width", group.width);
            store.WriteFloat(section.c_str(), "icon_size", group.icon_size);
            store.WriteFloat(section.c_str(), "cell_spacing", group.cell_spacing);
            store.WriteFloat(section.c_str(), "row_spacing", group.row_spacing);
            SaveTextProfile(store, ("text." + section).c_str(), group.text);
            store.WriteRect(("layout." + std::string{groupIds[i]}).c_str(), group.state.bounds);
        }
    }

    auto target_mob_info::OptionVisible(const size_t index) const -> bool
    {
        if (index == 0) return true;
        if (this->selected_group == 0)
            return index <= 2 || (index > common_text_options && index < common_text_options + text_profile_option_count);
        const auto group = static_cast<size_t>(this->selected_group - 1);
        if (index == 9) return !HeaderGroup(group) && group < Index(group_kind::server_id);
        if (index >= 3 + percent_options) return DamageGroup(group);
        return index >= 3 && index != 11;
    }
    auto target_mob_info::Option(const size_t index) -> module_option
    {
        static constexpr const char* tabs[]{"Common", "Job", "Behavior", "Links", "Detection", "Weaknesses", "Resistances", "Element ranks", "Status ranks", "Immunities", "Traits", "Server ID", "Spawn interval"};
        static_assert(std::size(tabs) == mob_info_group_count + 1);
        if (index == 0) return module_option::Element("Information group", "Choose common settings or customize one information group.", &this->selected_group, tabs, std::size(tabs));
        if (index == 1) return module_option::Toggle("Mob information", "Show available monster information with Target.", &this->enabled).Ordered(-50);
        if (index == 2) return module_option::Slider("Row spacing", "Space between attached information rows.", &this->spacing, 0, 32, 1).Ordered(220);
        if (this->selected_group == 0)
            return index >= common_text_options && index - common_text_options < text_profile_option_count ?
                TextProfileOption(index - common_text_options, this->text) : module_option{};
        return this->GroupOption(static_cast<size_t>(this->selected_group - 1), index - 3);
    }
    auto target_mob_info::GroupOption(const size_t groupIndex, const size_t index) -> module_option
    {
        auto& group = this->groups[groupIndex];
        static constexpr const char* positions[]{"Together", "Own row", "Independent"};
        if (index == percent_options)
            return module_option::Toggle("Percentages", "Show how much more or less damage the monster takes beside each icon.", &group.percentages).Ordered(21);
        if (index == percent_options + 1)
            return module_option::Toggle("Group by percentage", "Icons with the same change share one percentage.", &group.group_percentages).Ordered(22).VisibleWhen(group.percentages);
        switch (index)
        {
        case 0: return module_option::Toggle("Show group", "Show this information when the target has recorded data.", &group.state.enabled).Ordered(-50);
        case 1: return module_option::Choice("Position", "Together continues the preceding row. Independent can be dragged or moved in Movement.", &group.placement, positions, 3).Ordered(200);
        case 2: return module_option::Slider("Width", "Attached: zero follows Target width. Independent: width of this group.",
            group.placement == 2 ? &group.state.bounds.width : &group.width, group.placement == 2 ? 64.0f : 0.0f, 2000, 1).Ordered(100, true);
        case 3: return module_option::Slider("Icon size", "Zero follows text size; otherwise use this many pixels.", &group.icon_size, 0, 64, 1).Ordered(101);
        case 4: return module_option::Slider("Icon spacing", "Horizontal space between information cells.", &group.cell_spacing, 0, 64, 1).Ordered(220, true);
        case 5: return module_option::Slider("Wrap row spacing", "Vertical space when this group wraps to another line.", &group.row_spacing, 0, 32, 1).Ordered(221);
        case 6: return module_option::Toggle("Headings", "Show the heading for damage, rank, immunity and trait groups.", &group.headings).Ordered(20);
        case 7: return module_option::Toggle("Follow common text", "Use Common's text style. X/Y offsets remain independent for this entire group.", &group.follow_text).Ordered(30);
        default:
        {
            auto option = TextProfileOption(index - group_text_options, group.text);
            option.display_order += 200;
            if (index == 10 || index == 11) option.description = "Move this whole group, including its icons.";
            return option.VisibleWhen(!group.follow_text || index == 10 || index == 11);
        }
        }
    }
    auto target_mob_info::OnOptionChanged(const size_t index, const ui::rect& target) -> void
    {
        if (this->selected_group <= 0) return;
        auto& group = this->groups[static_cast<size_t>(this->selected_group - 1)];
        if (index == 4 && group.placement == 2 && !group.positioned)
        {
            group.state.bounds = group.attached_bounds.width > 0 ? group.attached_bounds : ui::rect{target.x + 7, target.y + target.height + 3, target.width - 14, 24};
            group.state.bounds.width = std::clamp(group.width > 0 ? group.width : group.state.bounds.width, 64.0f, 2000.0f);
            group.state.content_bounds = {}; group.positioned = true;
        }
        if (index == 10 && !group.follow_text) group.text.style = this->text.style;
    }

    namespace
    {
        constexpr std::array elements{ui::native_ui_image::fire, ui::native_ui_image::ice, ui::native_ui_image::wind,
            ui::native_ui_image::earth, ui::native_ui_image::lightning, ui::native_ui_image::water, ui::native_ui_image::light, ui::native_ui_image::dark};
        constexpr const char* damageNames[]{"Slash", "Pierce", "Impact", "H2H", "Fire", "Ice", "Wind", "Earth", "Lightning", "Water", "Light", "Dark"};
        constexpr const char* damageKindNames[]{"Physical", "Magic", "Ranged", "Breath"};
        constexpr const char* traitNames[]{"HP Regen", "MP Refresh", "TP Regain", "Shadows", "Counter"};
        constexpr const char* statusNames[]{"Paralyze", "Bind", "Silence", "Slow", "Poison", "Light Sleep", "Dark Sleep", "Blind", "Stun", "Gravity"};
        constexpr uint16_t statuses[]{4, 11, 6, 13, 3, 2, 2, 5, 10, 12};
        constexpr const char* immunityNames[]{"Addle", "Gravity", "Bind", "Stun", "Silence", "Paralyze", "Blind", "Slow", "Poison",
            "Elegy", "Requiem", "Light Sleep", "Dark Sleep", "Aspir", "Terror", "Dispel", "Petrify", "Plague", "Curse"};
        constexpr uint16_t immunityStatuses[]{21, 12, 11, 10, 6, 4, 5, 13, 3, 194, 192, 2, 2, 0, 28, 0, 7, 31, 9};

        // Damage taken changes by 1 + value / 10000.
        auto PercentText(const int change) -> std::string
        {
            char text[16]{};
            if (change % 100 == 0) std::snprintf(text, sizeof(text), "%+d%%", change / 100);
            else std::snprintf(text, sizeof(text), "%+.1f%%", change / 100.0);
            return text;
        }

        auto ElementArt(const module_render_context& context, const int index) -> IDirect3DTexture8*
        { return context.NativeAssets() != nullptr && index >= 0 && index < 8 ? context.NativeAssets()->Texture(elements[index]) : nullptr; }

        auto CellArt(const mob_info_value& cell, const module_render_context& context) -> ui::game_icon_view
        {
            ui::game_icon_view art{};
            if (cell.icon == mob_info_icon::physical && context.Icons() != nullptr) art = context.Icons()->PhysicalDamageIcon(cell.id);
            else if (cell.icon == mob_info_icon::detection && context.Icons() != nullptr) art = context.Icons()->MobDetectionIcon(static_cast<ui::mob_detection_icon>(cell.id));
            else if (cell.icon == mob_info_icon::element) art.texture = ElementArt(context, cell.id);
            else if (cell.icon == mob_info_icon::status && context.Icons() != nullptr) art.texture = context.Icons()->Status(cell.id);
            return art;
        }

        struct damage_change { size_t type{}; int change{}; bool absorbs{}; };

        // Icon types strongest first, then the broad damage kinds; neutral values never appear.
        auto DamageRow(const mob_record& mob, const mob_info_group_settings& settings, const bool weak) -> std::vector<mob_info_value>
        {
            const auto stronger = [&](const damage_change& a, const damage_change& b) {
                if (a.absorbs != b.absorbs) return a.absorbs;
                return weak ? a.change > b.change : a.change < b.change;
            };
            std::vector<damage_change> types, kinds;
            for (size_t i = 0; i < std::size(damageNames); ++i)
            {
                const bool absorbs = i >= 4 && (mob.absorbs & (1u << (i - 4))) != 0;
                const int change = absorbs ? 0 : mob.damage[i];
                if (weak ? change > 0 : absorbs || change < 0) types.push_back({i, change, absorbs});
            }
            for (size_t i = 0; i < std::size(damageKindNames); ++i)
                if (const int change = mob.damage_taken[i]; weak ? change > 0 : change < 0) kinds.push_back({i, change});
            std::stable_sort(types.begin(), types.end(), stronger);
            std::stable_sort(kinds.begin(), kinds.end(), stronger);
            std::vector<mob_info_value> values;
            for (size_t i = 0; i < types.size(); ++i)
            {
                const auto& type = types[i];
                const bool physical = type.type < 4;
                const bool shared = settings.group_percentages && i + 1 < types.size() &&
                    types[i + 1].change == type.change && types[i + 1].absorbs == type.absorbs;
                std::string value = type.absorbs ? i18n::Text("Absorbs") : settings.percentages ? PercentText(type.change) : "";
                if (shared && (type.absorbs || settings.percentages)) value.clear();
                values.push_back({i18n::Text(damageNames[type.type]), std::move(value), physical ? mob_info_icon::physical : mob_info_icon::element,
                    static_cast<uint16_t>(physical ? type.type : type.type - 4)});
            }
            for (const auto& kind : kinds)
                values.push_back({i18n::Text(damageKindNames[kind.type]), PercentText(kind.change)});
            return values;
        }
    }

    auto target_mob_info::Rows(const mob_record* mob) const -> std::vector<mob_info_row>
    {
        if (!this->enabled || mob == nullptr) return {};
        std::vector<mob_info_row> rows;
        const auto show = [&](const mob_info_group group) { return this->groups[static_cast<size_t>(group)].state.enabled; };
        if (show(mob_info_group::server_id)) rows.push_back({"Server ID: " + std::to_string(mob->id), {}, mob_info_group::server_id});
        if (show(mob_info_group::job))
            if (auto jobs = MobJobs(*mob); !jobs.empty()) rows.push_back({{}, {{std::move(jobs)}}, mob_info_group::job});
        using icon = ui::mob_detection_icon;
        const bool aggressive = (mob->flags & 3) != 0 && (mob->flags & (4 | 128)) == 0;
        const bool notorious = (mob->flags & 8) != 0, trueDetection = (mob->flags & 64) != 0;
        for (const auto group : {mob_info_group::behavior, mob_info_group::links, mob_info_group::detection})
        {
            if (!show(group)) continue;
            mob_info_row row{{}, {}, group};
            const auto add = [&](const char* name, const icon image) {
                row.values.push_back({name, {}, mob_info_icon::detection, static_cast<uint16_t>(image)});
            };
            if (group == mob_info_group::behavior)
            {
                if (aggressive) add(notorious ? "Aggressive (NM)" : "Aggressive", notorious ? icon::aggressive_nm : icon::aggressive);
                else add(notorious ? "Passive (NM)" : "Passive", notorious ? icon::passive_nm : icon::passive);
                if (MobIsPlaceholder(mob->id) || mob->placeholder_of != 0) row.values.push_back({"Placeholder"});
            }
            if (group == mob_info_group::links && (mob->flags & 32) != 0) add("Links", icon::links);
            if (group == mob_info_group::detection)
            {
                if ((mob->detects & 0x001) != 0) add(trueDetection ? "True sight" : "Sight", trueDetection ? icon::true_sight : icon::sight);
                if ((mob->detects & 0x002) != 0) add(trueDetection ? "True sound" : "Sound", icon::sound);
                if ((mob->detects & 0x100) != 0) add("Scent", icon::scent);
                if ((mob->detects & 0x020) != 0) add("Magic", icon::magic);
                if ((mob->detects & 0x040) != 0) add("Abilities", icon::ability);
                if ((mob->detects & 0x004) != 0) add("Low HP", icon::low_hp);
            }
            if (!row.values.empty()) rows.push_back(std::move(row));
        }
        for (const auto group : {mob_info_group::weaknesses, mob_info_group::resistances})
        {
            if (!show(group)) continue;
            const bool weak = group == mob_info_group::weaknesses;
            auto values = DamageRow(*mob, this->groups[static_cast<size_t>(group)], weak);
            if (!values.empty()) rows.push_back({i18n::Text(weak ? "Weak:" : "Resists:"), std::move(values), group});
        }
        if (show(mob_info_group::element_ranks))
        {
            mob_info_row row{"Element ranks:", {}, mob_info_group::element_ranks};
            for (size_t i = 0; i < 8; ++i)
                row.values.push_back({damageNames[i + 4], std::to_string(mob->elements[i]), mob_info_icon::element, static_cast<uint16_t>(i)});
            rows.push_back(std::move(row));
        }
        if (show(mob_info_group::status_ranks))
        {
            mob_info_row row{"Status ranks:", {}, mob_info_group::status_ranks};
            for (size_t i = 0; i < 10; ++i)
                row.values.push_back({statusNames[i], std::to_string(mob->statuses[i]), mob_info_icon::status, statuses[i], i == 5 ? 6 : i == 6 ? 7 : -1});
            rows.push_back(std::move(row));
        }
        if (show(mob_info_group::immunities) && mob->immunity != 0)
        {
            mob_info_row row{"Immune:", {}, mob_info_group::immunities};
            for (size_t bit = 0; bit < std::size(immunityNames); ++bit)
                if ((mob->immunity & (1u << bit)) != 0)
                    row.values.push_back({immunityNames[bit], {}, immunityStatuses[bit] != 0 ? mob_info_icon::status : mob_info_icon::none,
                        immunityStatuses[bit], bit == 11 ? 6 : bit == 12 ? 7 : -1});
            if (!row.values.empty()) rows.push_back(std::move(row));
        }
        if (show(mob_info_group::traits))
        {
            mob_info_row row{i18n::Text("Traits:"), {}, mob_info_group::traits};
            for (size_t bit = 0; bit < std::size(traitNames); ++bit)
                if ((mob->traits & (1u << bit)) != 0) row.values.push_back({i18n::Text(traitNames[bit])});
            if (const auto* effect = MobAddEffectName(*mob); effect != nullptr)
                row.values.push_back({mob->add_effect == 1 ? i18n::Text(effect) : i18n::Format("Additional effect: %s", i18n::Text(effect))});
            if (!row.values.empty()) rows.push_back(std::move(row));
        }
        if (show(mob_info_group::respawn))
        {
            rows.push_back({mob->respawn != 0 ? "Base spawn interval: " + std::to_string(mob->respawn) + "s" : "Base spawn interval: unknown", {}, mob_info_group::respawn});
            for (auto& hint : MobSpawnHints(mob->id)) rows.push_back({std::move(hint), {}, mob_info_group::respawn});
        }
        return rows;
    }

    auto target_mob_info::DrawHeaderIcons(const mob_record* mob, const ui::rect& area, const module_render_context& context) -> ui::rect
    {
        struct header_cell
        {
            size_t group{};
            ui::game_icon_view art{};
            std::string value{};
            module_text_profile text{};
            float width{}, height{}, icon_size{}, icon_width{}, gap{};
        };
        struct header_line { size_t first{}, last{}; float width{}, height{}; };
        // Damage changes stack above the job and behavior cells, which rest on the caption.
        std::vector<header_cell> damage, header;
        for (size_t i = 0; i < this->groups.size(); ++i)
            if (InHeader(i, this->groups[i])) this->groups[i].attached_bounds = {};
        for (const auto& row : this->Rows(mob))
        {
            const auto index = static_cast<size_t>(row.group);
            const auto& group = this->groups[index];
            if (!InHeader(index, group)) continue;
            auto text = group.text;
            if (group.follow_text) text.style = this->text.style;
            text.offset_x += this->text.offset_x; text.offset_y += this->text.offset_y;
            text.placement = 0; text.style.alignment = ui::text_alignment::left;
            const float iconSize = group.icon_size > 0 ? group.icon_size : std::clamp(context.Design().TextHeight(text.style), 16.0f, 32.0f);
            const float cellHeight = std::max(iconSize, context.Design().TextHeight(text.style));
            auto& cells = DamageGroup(index) ? damage : header;
            if (DamageGroup(index) && !cells.empty() && cells.back().group != index) cells.back().gap = std::max(cells.back().gap, 8.0f);
            const auto add = [&](const ui::game_icon_view art, std::string value, const float gap) {
                const float iconWidth = art.texture != nullptr ? iconSize + (value.empty() ? 0 : 4) : 0;
                const float width = iconWidth + context.Design().MeasureText(value.c_str(), text.style);
                cells.push_back({index, art, std::move(value), text, width, cellHeight, iconSize, iconWidth, gap});
            };
            if (group.headings && !row.label.empty()) add({}, row.label, 8);
            for (const auto& cell : row.values)
            {
                const auto art = CellArt(cell, context);
                add(art, art.texture != nullptr ? cell.value : cell.label + (cell.value.empty() ? "" : " " + cell.value), group.cell_spacing);
            }
        }
        const auto split = [&](const std::vector<header_cell>& cells, const bool wrap) {
            std::vector<header_line> lines;
            for (size_t i = 0; i < cells.size(); ++i)
            {
                if (lines.empty() || (wrap && lines.back().width + cells[i - 1].gap + cells[i].width > area.width)) lines.push_back({i, i});
                else lines.back().width += cells[i - 1].gap;
                lines.back().last = i + 1;
                lines.back().width += cells[i].width;
                lines.back().height = std::max(lines.back().height, cells[i].height);
            }
            return lines;
        };
        const auto unite = [](ui::rect& into, const ui::rect& bounds) {
            if (into.width == 0) { into = bounds; return; }
            const float right = std::max(into.x + into.width, bounds.x + bounds.width);
            const float bottom = std::max(into.y + into.height, bounds.y + bounds.height);
            into.x = std::min(into.x, bounds.x); into.y = std::min(into.y, bounds.y);
            into.width = right - into.x; into.height = bottom - into.y;
        };
        ui::rect used{};
        float bottom = area.y;
        const auto drawLine = [&](const std::vector<header_cell>& cells, const header_line& line) {
            const ui::rect strip{area.x + area.width - line.width, bottom - line.height, line.width, line.height};
            float x = strip.x;
            for (size_t i = line.first; i < line.last; ++i)
            {
                const auto& cell = cells[i];
                const float left = x + cell.text.offset_x;
                const float top = strip.y + (line.height - cell.height) * .5f + cell.text.offset_y;
                if (cell.art.texture != nullptr)
                    context.Design().DrawTexture(cell.art.texture, {left, top + (cell.height - cell.icon_size) * .5f, cell.icon_size, cell.icon_size}, 0xFFFFFFFF, cell.art.uv);
                auto text = cell.text; text.offset_x = text.offset_y = 0;
                const ui::rect at{left + cell.icon_width, top, cell.width - cell.icon_width, cell.height};
                if (!cell.value.empty()) context.DrawText(cell.value.c_str(), at, at, text);
                unite(this->groups[cell.group].attached_bounds, {left, top, cell.width, cell.height});
                x += cell.width + cell.gap;
            }
            unite(used, strip);
            bottom = strip.y - 2;
        };
        for (const auto& line : split(header, false)) drawLine(header, line);
        const auto damageLines = split(damage, true);
        for (auto line = damageLines.rbegin(); line != damageLines.rend(); ++line) drawLine(damage, *line);
        return used;
    }

    auto target_mob_info::Draw(const mob_record* mob, const ui::rect& area, const module_render_context& context, const size_t onlyGroup, const bool headerIcons) -> float
    {
        float x{}, y{}, lineHeight{}, lineSpacing{this->spacing}, previousSpacing{};
        size_t previousGroup = mob_info_group_count;
        const bool detached = onlyGroup < this->groups.size();
        const auto nextLine = [&] { y += lineHeight + lineSpacing; x = lineHeight = 0; lineSpacing = this->spacing; };
        for (const auto& row : this->Rows(mob))
        {
            const auto index = static_cast<size_t>(row.group);
            auto& group = this->groups[index];
            if (detached ? index != onlyGroup : group.placement == mob_info_group_settings::independent) continue;
            if (!detached && headerIcons && InHeader(index, group)) continue;
            if (x > 0 && group.placement != mob_info_group_settings::inline_row) nextLine();
            // Detail groups that continue a row keep a separator gap from the previous group.
            if (x > 0 && index != previousGroup && !HeaderGroup(index)) x += std::max(0.0f, 8.0f - previousSpacing);
            previousGroup = index; previousSpacing = group.cell_spacing;
            const float available = std::max(1.0f, detached || group.width == 0 ? area.width : std::min(area.width, group.width));
            auto text = group.text;
            if (group.follow_text) text.style = this->text.style;
            const float left = area.x + this->text.offset_x + text.offset_x;
            const float top = area.y + this->text.offset_y + text.offset_y;
            text.offset_x = text.offset_y = 0; text.placement = 0; text.style.alignment = ui::text_alignment::left;
            const float iconSize = group.icon_size > 0 ? group.icon_size : std::clamp(context.Design().TextHeight(text.style), 16.0f, 32.0f);
            const float height = std::max(iconSize, context.Design().TextHeight(text.style));
            group.attached_bounds = {area.x + x, area.y + y, std::max(64.0f, available - x), height};
            const auto drawText = [&](const std::string& value, const float at, const float width) {
                const ui::rect bounds{left + at, top + y, std::max(0.0f, width), height};
                context.DrawText(value.c_str(), bounds, bounds, text);
            };
            const auto reserve = [&](const float width) {
                if (x > 0 && x + width > available) { nextLine(); lineSpacing = group.row_spacing; }
                lineHeight = std::max(lineHeight, height);
            };
            if (row.values.empty())
            {
                std::istringstream words{row.label}; std::string word, line;
                while (words >> word)
                {
                    const auto candidate = line.empty() ? word : line + " " + word;
                    if (!line.empty() && x + context.Design().MeasureText(candidate.c_str(), text.style) > available)
                    {
                        reserve(context.Design().MeasureText(line.c_str(), text.style)); drawText(line, x, available - x);
                        nextLine(); lineSpacing = group.row_spacing; line = word;
                    }
                    else line = candidate;
                }
                if (!line.empty())
                {
                    const auto width = context.Design().MeasureText(line.c_str(), text.style);
                    reserve(width); drawText(line, x, available - x); x += width + group.cell_spacing;
                }
                if (group.placement == mob_info_group_settings::own_row) nextLine();
                continue;
            }
            if (group.headings && !row.label.empty())
            {
                const auto width = context.Design().MeasureText(row.label.c_str(), text.style);
                reserve(width); drawText(row.label, x, std::min(available - x, width)); x += width + 8;
            }
            for (const auto& cell : row.values)
            {
                const auto art = CellArt(cell, context);
                auto value = art.texture != nullptr ? cell.value : cell.label + (cell.value.empty() ? "" : " " + cell.value);
                auto* badge = ElementArt(context, cell.element);
                if (art.texture != nullptr && cell.element >= 0 && badge == nullptr)
                    value = std::string{cell.element == 6 ? "Light" : "Dark"} + (value.empty() ? "" : " " + value);
                const auto valueWidth = context.Design().MeasureText(value.c_str(), text.style);
                const auto iconWidth = art.texture != nullptr ? iconSize + (value.empty() ? 0 : 4) : 0;
                const auto width = iconWidth + valueWidth;
                reserve(width);
                if (art.texture != nullptr)
                {
                    context.Design().DrawTexture(art.texture, {left + x, top + y + (height - iconSize) * 0.5f, iconSize, iconSize}, 0xFFFFFFFF, art.uv);
                    if (badge != nullptr)
                        context.Design().DrawTexture(badge, {left + x + iconSize * 0.5f, top + y + height - iconSize * 0.5f, iconSize * 0.5f, iconSize * 0.5f});
                }
                if (!value.empty()) drawText(value, x + iconWidth, std::min(valueWidth, available - x - iconWidth));
                x += width + group.cell_spacing;
            }
        }
        return lineHeight > 0 ? y + lineHeight + lineSpacing : y;
    }

    auto target_mob_info_module::Id(void) const -> const char* { return target_mob_info::GroupId(this->group_); }
    auto target_mob_info_module::DisplayName(void) const -> const char*
    {
        static constexpr const char* names[]{"Mob Job", "Mob Behavior", "Mob Links", "Mob Detection", "Mob Weaknesses", "Mob Resistances", "Mob Element Ranks", "Mob Status Ranks", "Mob Immunities", "Mob Traits", "Mob Server ID", "Mob Spawn Interval"};
        static_assert(std::size(names) == mob_info_group_count);
        return names[this->group_];
    }
    auto target_mob_info_module::State(void) -> module_state& { return this->parent_.settings_.mob_info.groups[this->group_].state; }
    auto target_mob_info_module::State(void) const -> const module_state& { return const_cast<target_mob_info_module*>(this)->State(); }
    auto target_mob_info_module::HasEditableBounds(void) const -> bool
    {
        const auto& info = this->parent_.settings_.mob_info;
        return this->parent_.state_.enabled && info.enabled && info.groups[this->group_].state.enabled && info.groups[this->group_].placement == mob_info_group_settings::independent;
    }
    auto target_mob_info_module::ShouldRender(const hud_snapshot&) const -> bool
    {
        if (!this->HasEditableBounds() || !this->parent_.mob_info_display_allowed_) return false;
        const auto& info = this->parent_.settings_.mob_info;
        for (const auto& row : info.Rows(this->parent_.views_[0].mob))
            if (static_cast<size_t>(row.group) == this->group_) return true;
        return false;
    }
    auto target_mob_info_module::PreviewVisible(void) const -> bool { return this->ShouldRender({}); }
    auto target_mob_info_module::Render(const hud_snapshot&, const module_render_context& context) -> void
    {
        auto& state = this->State();
        const auto height = this->parent_.settings_.mob_info.Draw(this->parent_.views_[0].mob, state.bounds, context, this->group_);
        state.minimum_height = state.bounds.height = std::max(16.0f, height);
    }
}
