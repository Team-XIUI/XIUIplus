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
    namespace
    {
        constexpr const char* groupNames[]{"Behavior", "Links", "Detection", "Physical damage", "Element damage", "Element ranks", "Status ranks", "Immunities", "Server ID", "Spawn interval"};
        constexpr const char* groupIds[]{"mobbehavior", "moblinks", "mobdetection", "mobphysical", "mobelements", "mobelementranks", "mobstatusranks", "mobimmunities", "mobserverid", "mobspawn"};
        auto GroupSection(const size_t index) -> std::string { return std::string{"target.mobinfo."} + groupIds[index]; }
    }

    target_mob_info::target_mob_info()
    {
        for (size_t i = 0; i < this->groups.size(); ++i)
        {
            auto& group = this->groups[i];
            group.state.enabled = i <= 2 || i == 7;
            if (i <= 2) group.placement = mob_info_group_settings::inline_row;
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
        const bool defaults[]{read("behavior", true), read("behavior", true), read("behavior", true), read("damage", false),
            read("damage", false), read("ranks", false), read("ranks", false), read("immunities", true), read("server_id", false), read("respawn", false)};
        const auto finite = [&](const char* section, const char* key, float fallback, float low, float high) {
            const auto value = store.ReadFloat(section, key, fallback);
            return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
        };
        this->spacing = finite("mobinfo", "spacing", 2, 0, 32);
        this->spacing = finite("target.mobinfo", "spacing", this->spacing, 0, 32);
        this->text = LoadTextProfile(store, "text.target.mobinfo", LoadTextProfile(store, "text.mobinfo.details", this->text));
        for (size_t i = 0; i < this->groups.size(); ++i)
        {
            auto& group = this->groups[i]; const auto section = GroupSection(i);
            group.state.enabled = store.ReadBool(section.c_str(), "enabled", defaults[i]);
            group.placement = std::clamp(store.ReadInt(section.c_str(), "placement", i <= 2 ? 0 : 1), 0, 2);
            group.follow_text = store.ReadBool(section.c_str(), "follow_text", true);
            group.headings = store.ReadBool(section.c_str(), "headings", true);
            group.positioned = store.ReadBool(section.c_str(), "positioned", group.placement == 2);
            group.width = finite(section.c_str(), "width", 0, 0, 2000);
            group.icon_size = finite(section.c_str(), "icon_size", 0, 0, 64);
            group.cell_spacing = finite(section.c_str(), "cell_spacing", 12, 0, 64);
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
        if (index == 9 && this->selected_group > 0 && (this->selected_group <= 3 || this->selected_group >= 9)) return false;
        if (this->selected_group == 0) return index <= 2 || index > 11;
        return index >= 3 && index != 11;
    }
    auto target_mob_info::Option(const size_t index) -> module_option
    {
        static constexpr const char* tabs[]{"Common", "Behavior", "Links", "Detection", "Physical damage", "Element damage", "Element ranks", "Status ranks", "Immunities", "Server ID", "Spawn interval"};
        if (index == 0) return module_option::Element("Information group", "Choose common settings or customize one information group.", &this->selected_group, tabs, std::size(tabs));
        if (index == 1) return module_option::Toggle("Mob information", "Show available monster information with Target.", &this->enabled).Ordered(-50);
        if (index == 2) return module_option::Slider("Row spacing", "Space between attached information rows.", &this->spacing, 0, 32, 1).Ordered(220);
        if (this->selected_group == 0) return index >= 11 ? TextProfileOption(index - 11, this->text) : module_option{};
        return this->GroupOption(static_cast<size_t>(this->selected_group - 1), index - 3);
    }
    auto target_mob_info::GroupOption(const size_t groupIndex, const size_t index) -> module_option
    {
        auto& group = this->groups[groupIndex];
        static constexpr const char* positions[]{"Together", "Own row", "Independent"};
        switch (index)
        {
        case 0: return module_option::Toggle("Show group", "Show this information when the target has recorded data.", &group.state.enabled).Ordered(-50);
        case 1: return module_option::Choice("Position", "Together continues the preceding row. Independent can be dragged or moved in Movement.", &group.placement, positions, 3).Ordered(200);
        case 2: return module_option::Slider("Width", "Attached: zero follows Target width. Independent: width of this group.",
            group.placement == 2 ? &group.state.bounds.width : &group.width, group.placement == 2 ? 64.0f : 0.0f, 2000, 1).Ordered(100, true);
        case 3: return module_option::Slider("Icon size", "Zero follows text size; otherwise use this many pixels.", &group.icon_size, 0, 64, 1).Ordered(101);
        case 4: return module_option::Slider("Icon spacing", "Horizontal space between information cells.", &group.cell_spacing, 0, 64, 1).Ordered(220, true);
        case 5: return module_option::Slider("Wrap row spacing", "Vertical space when this group wraps to another line.", &group.row_spacing, 0, 32, 1).Ordered(221);
        case 6: return module_option::Toggle("Headings", "Show the heading for damage, rank and immunity groups.", &group.headings).Ordered(20);
        case 7: return module_option::Toggle("Follow common text", "Use Common's text style. X/Y offsets remain independent for this entire group.", &group.follow_text).Ordered(30);
        default:
        {
            auto option = TextProfileOption(index - 8, group.text);
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
        constexpr const char* statusNames[]{"Paralyze", "Bind", "Silence", "Slow", "Poison", "Light Sleep", "Dark Sleep", "Blind", "Stun", "Gravity"};
        constexpr uint16_t statuses[]{4, 11, 6, 13, 3, 2, 2, 5, 10, 12};
        constexpr const char* immunityNames[]{"Addle", "Gravity", "Bind", "Stun", "Silence", "Paralyze", "Blind", "Slow", "Poison",
            "Elegy", "Requiem", "Light Sleep", "Dark Sleep", "Aspir", "Terror", "Dispel", "Petrify", "Plague", "Curse"};
        constexpr uint16_t immunityStatuses[]{21, 12, 11, 10, 6, 4, 5, 13, 3, 194, 192, 2, 2, 0, 28, 0, 7, 31, 9};
    }

    auto target_mob_info::Rows(const mob_record* mob) const -> std::vector<mob_info_row>
    {
        if (!this->enabled || mob == nullptr) return {};
        std::vector<mob_info_row> rows;
        const auto show = [&](const mob_info_group group) { return this->groups[static_cast<size_t>(group)].state.enabled; };
        if (show(mob_info_group::server_id)) rows.push_back({"Server ID: " + std::to_string(mob->id), {}, mob_info_group::server_id});
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
        for (const auto group : {mob_info_group::physical, mob_info_group::elements})
        {
            if (!show(group)) continue;
            const bool physical = group == mob_info_group::physical;
            mob_info_row row{physical ? "Damage:" : "Element damage:", {}, group};
            for (size_t i = physical ? 0 : 4; i < (physical ? 4u : 12u); ++i)
            {
                char value[24]{};
                std::snprintf(value, sizeof(value), "x%.2f", 1 + mob->damage[i] / 10000.0);
                row.values.push_back({damageNames[i], value, physical ? mob_info_icon::physical : mob_info_icon::element,
                    static_cast<uint16_t>(physical ? i : i - 4)});
            }
            rows.push_back(std::move(row));
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
        if (show(mob_info_group::respawn))
        {
            rows.push_back({mob->respawn != 0 ? "Base spawn interval: " + std::to_string(mob->respawn) + "s" : "Base spawn interval: unknown", {}, mob_info_group::respawn});
            for (auto& hint : MobSpawnHints(mob->id)) rows.push_back({std::move(hint), {}, mob_info_group::respawn});
        }
        return rows;
    }

    auto target_mob_info::Draw(const mob_record* mob, const ui::rect& area, const module_render_context& context, const size_t onlyGroup) -> float
    {
        float x{}, y{}, lineHeight{}, lineSpacing{this->spacing};
        const bool detached = onlyGroup < this->groups.size();
        const auto nextLine = [&] { y += lineHeight + lineSpacing; x = lineHeight = 0; lineSpacing = this->spacing; };
        const auto elementArt = [&](const int index) -> IDirect3DTexture8* {
            return context.NativeAssets() != nullptr && index >= 0 && index < 8 ? context.NativeAssets()->Texture(elements[index]) : nullptr;
        };
        for (const auto& row : this->Rows(mob))
        {
            const auto index = static_cast<size_t>(row.group);
            auto& group = this->groups[index];
            if (detached ? index != onlyGroup : group.placement == mob_info_group_settings::independent) continue;
            if (x > 0 && group.placement != mob_info_group_settings::inline_row) nextLine();
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
                ui::game_icon_view art{};
                if (cell.icon == mob_info_icon::physical && context.Icons() != nullptr) art = context.Icons()->PhysicalDamageIcon(cell.id);
                else if (cell.icon == mob_info_icon::detection && context.Icons() != nullptr) art = context.Icons()->MobDetectionIcon(static_cast<ui::mob_detection_icon>(cell.id));
                else if (cell.icon == mob_info_icon::element) art.texture = elementArt(cell.id);
                else if (cell.icon == mob_info_icon::status && context.Icons() != nullptr) art.texture = context.Icons()->Status(cell.id);
                auto value = art.texture != nullptr ? cell.value : cell.label + (cell.value.empty() ? "" : " " + cell.value);
                auto* badge = elementArt(cell.element);
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
        static constexpr const char* names[]{"Mob Behavior", "Mob Links", "Mob Detection", "Mob Physical Damage", "Mob Element Damage", "Mob Element Ranks", "Mob Status Ranks", "Mob Immunities", "Mob Server ID", "Mob Spawn Interval"};
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
