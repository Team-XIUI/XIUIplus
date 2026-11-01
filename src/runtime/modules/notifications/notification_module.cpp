#include "notification_module.hpp"
#include "ui/game_icon_cache.hpp"
#include "settings/text_style_settings.hpp"

#include <algorithm>
#include <cmath>

namespace
{
    auto PostSamples(xiui::notification_center& center, const uint64_t now) -> void
    {
        center.Post({xiui::notification_kind::experience, 0, 125, "XP"}, now);
        center.Post({xiui::notification_kind::item, 4096, 2, "Fire crystal"}, now);
        center.Post({xiui::notification_kind::skill, 3, 0.3, "Sword"}, now);
        center.Post({xiui::notification_kind::party_invite, 0, 0, "Party Invitation"}, now);
        center.Post({xiui::notification_kind::trade_invite, 0, 0, "Trade Request"}, now);
        center.Post({xiui::notification_kind::key_item, 0, 0, "Key Item: Limit breaker"}, now);
        center.Post({xiui::notification_kind::gil, 65535, 250, "gil"}, now);
        center.Post({xiui::notification_kind::treasure, 4096, 1, "Treasure: Fire crystal"}, now);
    }
}

namespace xiui
{
    notification_module::notification_module(void)
    {
        this->groups_[1].state.enabled = false;
        for (size_t i = 1; i < maximum_groups; ++i)
        {
            this->groups_[i].state.bounds.y += static_cast<float>(i * 96);
            this->groups_[i].offset_y = static_cast<float>(i * 96);
        }
    }

    auto notification_module::ResetDefaults(const ui::theme&) -> void
    {
        const auto bounds = this->groups_[0].state.bounds;
        *this = notification_module{};
        this->groups_[0].state.bounds = bounds;
    }

    auto notification_module::LoadGroup(const settings_store& store, const size_t index) -> void
    {
        auto& group = this->groups_[index];
        const auto section = index == 0 ? std::string{"notifications"} : "notifications.group" + std::to_string(index + 1);
        const auto layout = index == 0 ? std::string{"layout.notifications"} : "layout." + section;
        const auto textSection = index == 0 ? std::string{"text.notifications"} : "text." + section;
        const auto number = [&](const char* key, const float fallback, const float low, const float high) {
            const float value = store.ReadFloat(section.c_str(), key, fallback);
            return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
        };
        group.state.enabled = store.ReadBool(section.c_str(), "enabled", index != 1);
        group.state.bounds = store.ReadRect(layout.c_str(), group.state.bounds);
        group.screen_anchor = store.ReadBool(section.c_str(), "screen_anchor", store.ReadBool(section.c_str(), "follow_player", true));
        group.offset_x = number("offset_x", group.offset_x, -1000.0f, 1000.0f);
        group.offset_y = number("offset_y", group.offset_y, -1000.0f, 1000.0f);
        if (store.ReadInt(section.c_str(), "anchor_version", 0) == 0 && group.offset_y == -48.0f)
            group.offset_y = 0.0f;
        group.style.duration = number("duration", 3.2f, 1.0f, 8.0f);
        group.style.rise = number("rise", 36.0f, 0.0f, 120.0f);
        auto text = InheritGlobalTextStyle(store, group.style);
        text.scale = number("text_scale", text.scale, 0.5f, ui::text_max_scale);
        text.font = ui::FontIndex(store.ReadInt(section.c_str(), "font", text.font));
        group.text = LoadTextProfile(store, textSection.c_str(), {text});
        group.style.visible_count = number("visible_count", 5.0f, 1.0f, 8.0f);
        const float spacing = store.ReadFloat(section.c_str(), "row_spacing", 4.0f);
        group.style.row_spacing = std::clamp(std::isfinite(spacing) ? spacing : 4.0f, 0.0f, 64.0f);
        group.style.items = store.ReadBool(section.c_str(), "items", true);
        group.style.experience = store.ReadBool(section.c_str(), "experience", true);
        group.style.skills = store.ReadBool(section.c_str(), "skills", true);
        group.style.party_invites = store.ReadBool(section.c_str(), "party_invites", true);
        group.style.trade_invites = store.ReadBool(section.c_str(), "trade_invites", true);
        group.style.key_items = store.ReadBool(section.c_str(), "key_items", group.style.items);
        group.style.gil = store.ReadBool(section.c_str(), "gil", group.style.items);
        group.style.treasure = store.ReadBool(section.c_str(), "treasure", group.style.items);
        group.style.background = store.ReadBool(section.c_str(), "container", false);
        group.anchor = std::clamp(store.ReadInt(section.c_str(), "anchor_direction", 1), 0, 1);
        group.style.item_color = store.ReadArgb(section.c_str(), "item_color", group.style.item_color);
        group.style.experience_color = store.ReadArgb(section.c_str(), "experience_color", group.style.experience_color);
        group.style.skill_color = store.ReadArgb(section.c_str(), "skill_color", group.style.skill_color);
        group.style.party_color = store.ReadArgb(section.c_str(), "party_color", group.style.item_color);
        group.style.trade_color = store.ReadArgb(section.c_str(), "trade_color", group.style.item_color);
        group.style.key_item_color = store.ReadArgb(section.c_str(), "key_item_color", group.style.item_color);
        group.style.gil_color = store.ReadArgb(section.c_str(), "gil_color", group.style.item_color);
        group.style.treasure_color = store.ReadArgb(section.c_str(), "treasure_color", group.style.item_color);
        group.direction = std::clamp(store.ReadInt(section.c_str(), "stack_direction", 0), 0, 1);
        group.style.pin_invites = store.ReadBool(section.c_str(), "pin_invites", true);
        const auto minify = store.ReadFloat(section.c_str(), "minify_after", 3);
        group.style.minify_after = std::isfinite(minify) ? std::clamp(minify, .5f, 30.0f) : 3;

    }

    auto notification_module::SaveGroup(const settings_store& store, const size_t index) const -> void
    {
        const auto& group = this->groups_[index];
        const auto section = index == 0 ? std::string{"notifications"} : "notifications.group" + std::to_string(index + 1);
        const auto layout = index == 0 ? std::string{"layout.notifications"} : "layout." + section;
        const auto textSection = index == 0 ? std::string{"text.notifications"} : "text." + section;
        store.WriteBool(section.c_str(), "enabled", group.state.enabled);
        store.WriteRect(layout.c_str(), group.state.bounds);
        store.WriteBool(section.c_str(), "screen_anchor", group.screen_anchor);
        store.WriteInt(section.c_str(), "anchor_version", 2);
        store.WriteFloat(section.c_str(), "offset_x", group.offset_x);
        store.WriteFloat(section.c_str(), "offset_y", group.offset_y);
        store.WriteFloat(section.c_str(), "duration", group.style.duration);
        store.WriteFloat(section.c_str(), "rise", group.style.rise);
        store.WriteFloat(section.c_str(), "text_scale", group.text.style.scale);
        store.WriteInt(section.c_str(), "font", group.text.style.font);
        SaveTextProfile(store, textSection.c_str(), group.text);
        store.WriteFloat(section.c_str(), "visible_count", group.style.visible_count);
        store.WriteFloat(section.c_str(), "row_spacing", group.style.row_spacing);
        store.WriteBool(section.c_str(), "items", group.style.items);
        store.WriteBool(section.c_str(), "experience", group.style.experience);
        store.WriteBool(section.c_str(), "skills", group.style.skills);
        store.WriteBool(section.c_str(), "party_invites", group.style.party_invites);
        store.WriteBool(section.c_str(), "trade_invites", group.style.trade_invites);
        store.WriteBool(section.c_str(), "key_items", group.style.key_items);
        store.WriteBool(section.c_str(), "gil", group.style.gil);
        store.WriteBool(section.c_str(), "treasure", group.style.treasure);
        store.WriteBool(section.c_str(), "container", group.style.background);
        store.WriteInt(section.c_str(), "anchor_direction", group.anchor);
        store.WriteArgb(section.c_str(), "item_color", group.style.item_color);
        store.WriteArgb(section.c_str(), "experience_color", group.style.experience_color);
        store.WriteArgb(section.c_str(), "skill_color", group.style.skill_color);
        store.WriteArgb(section.c_str(), "party_color", group.style.party_color);
        store.WriteArgb(section.c_str(), "trade_color", group.style.trade_color);
        store.WriteArgb(section.c_str(), "key_item_color", group.style.key_item_color);
        store.WriteArgb(section.c_str(), "gil_color", group.style.gil_color);
        store.WriteArgb(section.c_str(), "treasure_color", group.style.treasure_color);
        store.WriteInt(section.c_str(), "stack_direction", group.direction);
        store.WriteBool(section.c_str(), "pin_invites", group.style.pin_invites);
        store.WriteFloat(section.c_str(), "minify_after", group.style.minify_after);

    }

    auto notification_module::ContentOption(const size_t index) -> module_option
    {
        auto& group = this->groups_[this->SelectedGroup()];
        auto& events = this->groups_[0].style;
        static constexpr const char* anchors[]{"Anchored left", "Anchored right"};
        static constexpr const char* counts[]{"2", "3", "4", "5", "6"};
        static constexpr const char* directions[]{"Up", "Down"};
        static constexpr const char* kinds[]{"Items", "Experience / limit points", "Skill gains", "Messages", "Gil", "Key items", "Treasure pool", "Party / alliance invites", "Trade requests"};
        if (index >= 35 && index < 48) return TextProfileOption(index == 35 ? 0 : index - 34, group.text);
        switch (index)
        {
        case 0: return module_option::Toggle("Enabled", "Show the notification types selected under Events.", &this->groups_[0].state.enabled).Ordered(-50);
        case 1: return module_option::Toggle("Screen anchor", "Stable position left of screen center.", &group.screen_anchor).Ordered(-30);
        case 2: return module_option::Toggle("Items", "New items across all bags.", &events.items);
        case 3: return module_option::Toggle("Experience", "XP and limit point gains.", &events.experience);
        case 4: return module_option::Toggle("Skill gains", "Combat and magic skill ups.", &events.skills);
        case 5: return module_option::Slider("Spacing", "Gap between notification rows in pixels, including edit previews.", &group.style.row_spacing, 0.0f, 64.0f, 1.0f, module_option_group::appearance).Ordered(300, true);
        case 6: return module_option::Slider("Duration", "Seconds before regular notifications disappear. Party invites stay until answered or their 90-second timer expires.", &group.style.duration, 1.0f, 8.0f, 0.1f, module_option_group::effects);
        case 7: return module_option::Slider("Visible rows", "Maximum visible notifications.", &group.style.visible_count, 1.0f, 8.0f, 1.0f, module_option_group::appearance).Ordered(122);
        case 8: return module_option::Slider("Rise", "Upward travel in pixels.", &group.style.rise, 0.0f, 120.0f, 1.0f, module_option_group::effects);
        case 9: return TextSizeOption("Text size (px)", "Notification font size in pixels.", &group.text.style.scale).Ordered(101);
        case 10: return ModuleWidthOption(group.state);
        case 11: return module_option::Color("XP color", "Experience text.", &group.style.experience_color, module_option_group::colors).VisibleWhen(events.experience);
        case 12: return module_option::Color("Item color", "Item gain text.", &group.style.item_color, module_option_group::colors);
        case 13: return module_option::Color("Skill color", "Skill gain text.", &group.style.skill_color, module_option_group::colors).VisibleWhen(events.skills);
        case 14: return module_option::Toggle("Container background", "Native themed frame and background, fading away from the text's anchored edge.", &group.style.background, module_option_group::appearance).Ordered(-10);
        case 15: return module_option::Choice("Anchored direction", "Right: enter from the right and fade the container left. Left: enter from the left and fade the container right.", &group.anchor, anchors, 2, module_option_group::appearance).Ordered(200);
        case 16: return module_option::Toggle("Party / alliance invites", "Keep incoming invitations visible with a countdown until answered or expired.", &events.party_invites);
        case 17: return module_option::Toggle("Trade requests", "Show incoming requests to trade.", &events.trade_invites);
        case 18: return module_option::Toggle("Key items", "Show newly obtained key items.", &events.key_items);
        case 19: return module_option::Toggle("Gil", "Show positive gil gains after inventory loading completes.", &events.gil);
        case 20: return module_option::Toggle("Treasure pool", "Show new items that remain available in the treasure pool.", &events.treasure);
        case 21: return module_option::Color("Party invite color", "Party and alliance invitation text.", &group.style.party_color, module_option_group::colors).VisibleWhen(events.party_invites);
        case 22: return module_option::Color("Trade request color", "Incoming trade request text.", &group.style.trade_color, module_option_group::colors).VisibleWhen(events.trade_invites);
        case 23: return module_option::Color("Key item color", "New key-item text.", &group.style.key_item_color, module_option_group::colors).VisibleWhen(events.key_items);
        case 24: return module_option::Color("Gil color", "Gil-gain text.", &group.style.gil_color, module_option_group::colors).VisibleWhen(events.gil);
        case 25: return module_option::Color("Treasure color", "New treasure-pool item text.", &group.style.treasure_color, module_option_group::colors).VisibleWhen(events.treasure);
        case 26: return module_option::Dropdown("Font", "Font face for notification text.", &group.text.style.font, ui::font_names, ui::font_count).Ordered(100, true);
        case 27: return module_option::Dropdown("Group count", "Independent notification stacks. Existing event assignments are retained when a group is temporarily disabled.", &this->group_count_, counts, std::size(counts));
        case 28: return module_option::Element("Group", "Choose the stack to customize or move.", &this->selected_group_, group_names, this->GroupCount()).Ordered(-100).VisibleWhen(this->GroupCount() > 2 || this->groups_[1].state.enabled);
        case 29: return module_option::Dropdown("Stack direction", "Grow above or below the anchor.", &group.direction, directions, 2).Ordered(201);
        case 30: return module_option::Toggle("Pin invitations", "Minify party and trade notices after a delay. Party expires after 90 seconds; trade resolves from native responses or a 30-second fallback.", &group.style.pin_invites).VisibleWhen(events.party_invites || events.trade_invites);
        case 31: return module_option::Slider("Minify after", "Seconds before invitations become compact pinned notices. Hover or focus Notifications to read the sender.", &group.style.minify_after, .5f, 30, .5f).VisibleWhen(group.style.pin_invites && (events.party_invites || events.trade_invites));
        case 32: return module_option::Dropdown("Notification type", "Choose the event kind to route. XP, skills and messages can be assigned too.", &this->selected_kind_, kinds, std::size(kinds));
        case 33: return module_option::Dropdown("Assigned group", "Each event appears in exactly one group. Assignments to inactive groups use Group 1 until re-enabled.", &this->routing_[static_cast<size_t>(this->selected_kind_)], group_names, maximum_groups);
        case 34: return module_option::Action("Clear invitation notices", "Dismisses this group's visual notices only. It does not accept or decline a party or trade request.", "Clear");
        case 48: return module_option::Toggle("Enable Group 2", "Show a second notification stack and its edit preview. Disabled assignments use Group 1 until re-enabled.", &this->groups_[1].state.enabled).Ordered(-40);
        default: return {};
        }
    }

    auto notification_module::Load(const settings_store& store) -> void
    {
        this->LoadGroup(store, 0);
        for (size_t i = 1; i < maximum_groups; ++i)
        {
            this->groups_[i] = this->groups_[0];
            static_cast<ui::text_style&>(this->groups_[i].style) = this->groups_[0].text.style;
            this->groups_[i].state.bounds.y += static_cast<float>(i * 96);
            this->groups_[i].offset_y += static_cast<float>(i * 96);
            this->LoadGroup(store, i);
        }
        this->group_count_ = std::clamp(store.ReadInt("notifications", "group_count", 2), 2, 6) - 2;
        for (size_t kind = 0; kind < this->routing_.size(); ++kind)
            this->routing_[kind] = std::clamp(store.ReadInt("notifications.routing", std::to_string(kind).c_str(), 1), 1, 6) - 1;
        if (store.ReadInt("notifications", "group2_enabled_version", 0) < 1 &&
            std::none_of(this->routing_.begin(), this->routing_.end(), [](const int group) { return group == 1; }))
            this->groups_[1].state.enabled = false;
        this->SelectGroup(this->SelectedGroup());
        if (this->selected_group_ == 1 && !this->GroupEnabled(1)) this->selected_group_ = 0;
    }

    auto notification_module::Save(const settings_store& store) const -> void
    {
        for (size_t i = 0; i < maximum_groups; ++i) this->SaveGroup(store, i);
        store.WriteInt("notifications", "group_count", static_cast<int>(this->GroupCount()));
        store.WriteInt("notifications", "group2_enabled_version", 1);
        for (size_t kind = 0; kind < this->routing_.size(); ++kind)
            store.WriteInt("notifications.routing", std::to_string(kind).c_str(), this->routing_[kind] + 1);
    }

    auto notification_module::GroupFor(const notification_kind kind) const -> int
    {
        const auto index = static_cast<size_t>(kind);
        const auto group = index < this->routing_.size() ? this->routing_[index] : 0;
        return group >= 0 && this->GroupEnabled(static_cast<size_t>(group)) ? group : 0;
    }

    auto notification_module::Style(const size_t index) const -> notification_style
    {
        const auto& group = this->groups_[index];
        auto style = group.style;
        static_cast<ui::text_style&>(style) = group.text.style;
        style.alignment = group.text.placement == 0 ? (group.anchor == 0 ? ui::text_alignment::left : ui::text_alignment::right) : static_cast<ui::text_alignment>(group.text.placement - 1);
        style.stack_down = group.direction == 1;
        style.center_vertically = group.screen_anchor;
        const auto& events = this->groups_[0].style;
        style.items = events.items; style.experience = events.experience; style.skills = events.skills;
        style.party_invites = events.party_invites; style.trade_invites = events.trade_invites;
        style.key_items = events.key_items; style.gil = events.gil; style.treasure = events.treasure;
        return style;
    }

    auto notification_module::Expire(const uint64_t now) -> void
    {
        std::array<uint64_t, 9> lifetimes{};
        for (size_t kind = 0; kind < lifetimes.size(); ++kind)
            lifetimes[kind] = static_cast<uint64_t>(this->groups_[this->GroupFor(static_cast<notification_kind>(kind))].style.duration * 1000);
        this->center_.Update(now, lifetimes);
    }

    auto notification_module::OnContentOptionChanged(const size_t index) -> void
    {
        this->SelectGroup(this->SelectedGroup());
        if (this->selected_group_ == 1 && !this->GroupEnabled(1)) this->selected_group_ = 0;
        if (index == 34)
            for (const auto kind : {notification_kind::party_invite, notification_kind::trade_invite})
                if (this->GroupFor(kind) == static_cast<int>(this->SelectedGroup())) this->center_.Dismiss(kind);
    }

    auto notification_module::ConfigSections(void) const -> std::span<const module_config_section>
    {
        static constexpr module_config_section sections[]{
            {"General", "Enable notifications and choose how many independent groups to use.", "general"},
            {"Events", "Global event switches and exactly one destination group per notification type.", "events"},
            {"Layout", "Select a group, then customize its position, direction, width, rows and spacing.", "layout"},
            {"Appearance", "Independent group backgrounds and event colors.", "appearance"},
            {"Animation", "Per-group lifetime, movement and compact pinned invitations.", "animation"},
            {"Text", "Shared text styling for the selected notification group.", "text"}};
        return sections;
    }

    auto notification_module::ContentConfigSectionContains(const size_t section, const size_t index) -> bool
    {
        if (section >= this->ConfigSections().size() || index >= this->OptionCount()) return false;
        if (index == 28) return section >= 2;
        if (index == 0 || index == 27 || index == 48) return section == 0;
        if ((index >= 2 && index <= 4) || (index >= 16 && index <= 20) || index == 32 || index == 33) return section == 1;
        if (index == 1 || index == 5 || index == 7 || index == 10 || index == 15 || index == 29) return section == 2;
        if (index == 6 || index == 8 || (index >= 30 && index <= 31) || index == 34) return section == 4;
        if (index == 9 || index == 26 || index >= 35) return section == 5;
        return section == 3;
    }

    auto notification_module::GroupVisible(const size_t index) const -> bool
    {
        if (!this->State().enabled || !this->GroupEnabled(index)) return false;
        const auto style = this->Style(index);
        return std::any_of(this->center_.Entries().begin(), this->center_.Entries().end(), [&](const auto& entry) {
            return this->GroupFor(entry.event.kind) == static_cast<int>(index) && notification_renderer::KindEnabled(entry.event.kind, style);
        });
    }

    auto notification_module::Demo(const uint64_t now) -> void { PostSamples(this->center_, now); }

    auto notification_module::RenderGroup(const size_t index, const module_render_context& context) -> void
    {
        auto& group = this->groups_[index];
        group.drawn = {};
        if (!this->State().enabled || !this->GroupEnabled(index)) return;
        const auto now = ::GetTickCount64();
        auto area = group.state.bounds;
        auto style = this->Style(index);
        std::array<int, 9> routing{};
        for (size_t kind = 0; kind < routing.size(); ++kind) routing[kind] = this->GroupFor(static_cast<notification_kind>(kind));
        if (context.IsEditing())
        {
            area.x += group.text.offset_x; area.y += group.text.offset_y;
            notification_center preview;
            PostSamples(preview, now > 400 ? now - 400 : 0);
            bool assigned = std::any_of(routing.begin(), routing.end(), [index](int value) { return value == static_cast<int>(index); });
            if (!assigned) { preview.Clear(); preview.Post({notification_kind::message, 0, 0, group_names[index]}, now > 400 ? now - 400 : 0); }
            area.y += 78;
            notification_renderer::Draw(context.Design(), preview, area, style, now, context.Icons(), &context,
                assigned ? std::span<const int>{routing} : std::span<const int>{}, static_cast<int>(index));
            return;
        }
        if (group.screen_anchor)
        {
            const auto& viewport = context.Viewport();
            if (viewport.width <= 0 || viewport.height <= 0) return;
            area = notification_renderer::ScreenBounds(viewport, area, group.offset_x, group.offset_y, style.alignment);
        }
        else area.y += 78;
        area.x += group.text.offset_x; area.y += group.text.offset_y;
        notification_renderer::Draw(context.Design(), this->center_, area, style, now, context.Icons(), &context,
            routing, static_cast<int>(index), &group.drawn);
        for (size_t at = 0; at < group.drawn.count; ++at) group.drawn.hits[at].bounds = context.Design().Transform().Map(group.drawn.hits[at].bounds);
    }

    auto notification_module::Detail(const uint64_t id) const -> std::optional<notification_draw_result::hit>
    {
        const auto* entry = this->center_.Find(id);
        if (!this->State().enabled || entry == nullptr || !IsInvitation(entry->event.kind)) return std::nullopt;
        const auto group = this->GroupFor(entry->event.kind);
        if (!notification_renderer::KindEnabled(entry->event.kind, this->Style(group)) || entry->Remaining(::GetTickCount64(), 3200) == 0) return std::nullopt;
        const auto& drawn = this->groups_[group].drawn;
        for (size_t i = 0; i < drawn.count; ++i) if (drawn.hits[i].id == id) return drawn.hits[i];
        return std::nullopt;
    }

    auto notification_module::DetailAt(const float x, const float y) const -> std::optional<notification_draw_result::hit>
    {
        for (size_t group = 0; group < this->GroupCount(); ++group)
            for (size_t i = 0; i < this->groups_[group].drawn.count; ++i)
            {
                const auto hit = this->groups_[group].drawn.hits[i];
                if (x >= hit.bounds.x && x < hit.bounds.x + hit.bounds.width && y >= hit.bounds.y && y < hit.bounds.y + hit.bounds.height)
                    if (auto detail = this->Detail(hit.id)) return detail;
            }
        return std::nullopt;
    }

    auto notification_module::Details(const uint64_t id) const -> std::string
    {
        const auto* entry = this->center_.Find(id);
        return entry != nullptr ? entry->event.subject + ". Use the game's menus to respond." : "";
    }

    auto notification_module::FirstDetail(void) const -> uint64_t
    {
        for (const auto& entry : this->center_.Entries()) if (this->Detail(entry.id)) return entry.id;
        return 0;
    }

    auto notification_module::NextDetail(const uint64_t id, const int direction) const -> uint64_t
    {
        std::array<uint64_t, 2> ids{}; size_t count{}, selected{};
        for (const auto& entry : this->center_.Entries())
            if (count < ids.size() && this->Detail(entry.id)) { if (entry.id == id) selected = count; ids[count++] = entry.id; }
        return count != 0 ? ids[(selected + count + direction) % count] : 0;
    }

    auto notification_group_module::Id(void) const -> const char*
    {
        static constexpr const char* ids[]{"notifications", "notifications.group2", "notifications.group3", "notifications.group4", "notifications.group5", "notifications.group6"};
        return ids[this->group_];
    }
}
