#include "localization/localization.hpp"
#include "ashita_compat.hpp"
#include "target_module.hpp"
#include "modules/bar_config.hpp"
#include "chat/chat_encoding.hpp"
#include "ui/status_strip.hpp"
#include "modules/mobinfo/mob_database.hpp"
#include "ui/text_fit.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace xiui
{
    namespace
    {
        constexpr D3DCOLOR distance_top = 0xFFD8D1C2, distance_bottom = 0xFFBFB8A9;

        auto DistanceText(ui::text_style style) -> module_text_profile
        {
            style.top_color = distance_top; style.bottom_color = distance_bottom;
            return {style};
        }

        auto MigrateDistanceColor(module_text_profile& profile) -> void
        {
            if (profile.style.top_color == 0xFF999388 && profile.style.bottom_color == 0xFF807B72)
            {
                profile.style.top_color = distance_top;
                profile.style.bottom_color = distance_bottom;
            }
        }
    }

    auto target_cast_module::ResetSettingsFrom(const hud_module& defaults, const ui::theme&) -> void
    {
        const auto& source = static_cast<const target_cast_module&>(defaults);
        auto& target = this->parent_.settings_.display;
        const auto& preset = source.parent_.settings_.display;
        target.casts = preset.casts; target.custom_cast_meter = preset.custom_cast_meter; target.custom_cast_fill = preset.custom_cast_fill;
        target.cast_placement = preset.cast_placement; target.cast_height = preset.cast_height; target.cast_spacing = preset.cast_spacing;
        target.cast_width = preset.cast_width; target.cast_offset_x = preset.cast_offset_x; target.cast_offset_y = preset.cast_offset_y;
        target.cast_top = preset.cast_top; target.cast_bottom = preset.cast_bottom; target.cast_meter = preset.cast_meter; target.cast_text = preset.cast_text;
        this->parent_.cast_state_ = source.parent_.cast_state_; this->parent_.cast_positioned_ = source.parent_.cast_positioned_;
    }

    target_module::target_module(void)
    {
        this->settings_.distance_text = DistanceText(ui::theme::CreateDefault().caption_text);
        this->ResetChild(true);
        this->settings_.display.fill_top.fill(this->settings_.hp_color_top);
        this->settings_.display.fill_bottom.fill(this->settings_.hp_color);
    }
    auto target_module::Id(void) const -> const char* { return "target"; }
    auto target_module::DisplayName(void) const -> const char* { return "Target"; }
    auto target_module::State(void) -> module_state& { return this->state_; }
    auto target_module::State(void) const -> const module_state& { return this->state_; }
    auto target_module::Settings(void) -> settings& { return this->settings_; }

    auto target_module::ApplyMeterDefaults(const ui::meter_style& style) -> void
    {
        this->settings_.hp_style = style;
        if (this->settings_.hp_style.height <= 0) this->settings_.hp_style.height = 12;
        this->settings_.world_bar.meter = style; this->settings_.world_bar.meter.height = 6;
        this->settings_.recipient.meter = this->settings_.hp_style;
        this->settings_.subtarget.meter = this->settings_.hp_style;
    }

    auto target_module::ApplyTextDefaults(const ui::theme& theme) -> void
    {
        this->timers_.text = {theme.caption_text};
        this->settings_.status_text = {theme.caption_text};
        this->settings_.name_text = {theme.title_text};
        this->settings_.distance_text = DistanceText(theme.caption_text);
        this->settings_.distance_text.style.alignment = ui::text_alignment::right;
        this->settings_.label_text = {theme.meter_text};
        this->settings_.label_text.style.alignment = ui::text_alignment::left;
        this->settings_.value_text = {theme.meter_text};
        this->settings_.value_text.style.alignment = ui::text_alignment::right;
        this->settings_.recipient.text = {this->settings_.name_text, this->settings_.distance_text, this->settings_.label_text, this->settings_.value_text};
        this->settings_.subtarget.text = this->settings_.recipient.text;
        this->subtarget_status_text_ = {theme.caption_text};
        this->settings_.display.cast_text = this->settings_.value_text;
        this->settings_.mob_info.ApplyTextDefaults(theme);
        this->settings_.world_bar.ApplyTextDefaults(theme);
    }

    auto target_module::ResetDefaults(const ui::theme& themeDefaults) -> void
    {
        const ui::rect bounds = this->state_.bounds;
        const auto* timing = this->timing_;
        auto* healing = this->settings_.world_bar.healing;
        const auto subtargetBounds = this->subtarget_state_.bounds, recipientBounds = this->recipient_state_.bounds;
        *this = target_module{};
        this->timing_ = timing;
        this->settings_.world_bar.healing = healing;
        this->state_.bounds = bounds;
        this->subtarget_state_.bounds = subtargetBounds; this->recipient_state_.bounds = recipientBounds;
        this->ApplyMeterDefaults(themeDefaults.meter);
        this->ApplyTextDefaults(themeDefaults);
        shared_entity_name_palette = {};
    }

    auto target_module::SelectedTextProfile(void) -> module_text_profile&
    {
        if (this->settings_.selected_text == 4) return this->timers_.text;
        if (this->settings_.selected_text == 5) return this->settings_.status_text;
        if (this->settings_.selected_text == 6) return this->settings_.display.cast_text;
        if (this->settings_.selected_text == 1)
            return this->settings_.distance_text;
        if (this->settings_.selected_text == 2)
            return this->settings_.label_text;
        if (this->settings_.selected_text == 3)
            return this->settings_.value_text;
        return this->settings_.name_text;
    }

    auto target_module::Load(const settings_store& store) -> void
    {
        this->timers_.Load(store, "timing.target");
        this->settings_.statuses.Load(store, "status.target");
        this->settings_.status_text = LoadTextProfile(store, "text.target.status", this->settings_.status_text);
        this->state_.enabled = store.ReadBool("target", "enabled", true);
        this->settings_.hide_when_empty = store.ReadBool("target", "hide_when_empty", false);
        this->settings_.hide_self = store.ReadBool("target", "hide_self", false);
        this->settings_.show_distance = store.ReadBool("target", "show_distance", true);
        this->settings_.show_percent = store.ReadBool("target", "show_percent", true);
        this->settings_.show_label = store.ReadBool("target", "show_label", true);
        const D3DCOLOR hpFallback = store.ReadArgb("target", "hp_color", D3DCOLOR_XRGB(251, 148, 148));
        this->settings_.hp_color_top = store.ReadArgb("meter.target.hp", "fill_top", MeterGradientTop(hpFallback));
        this->settings_.hp_color = store.ReadArgb("meter.target.hp", "fill_bottom", hpFallback);
        this->settings_.hp_style = LoadMeterStyle(store, "meter.target.hp", this->settings_.hp_style);
        this->settings_.name_text = LoadTextProfile(store, "text.target.name", this->settings_.name_text);
        if (store.ReadInt("target", "name_anchor_version", 0) < 1 && this->settings_.name_text.placement == 0 && this->settings_.name_text.offset_x == 12)
            this->settings_.name_text.offset_x = 0;
        this->settings_.distance_text = LoadTextProfile(store, "text.target.distance", this->settings_.distance_text);
        const bool legacyDistanceColor = store.ReadInt("target", "distance_color_version", 0) < 1;
        if (legacyDistanceColor) MigrateDistanceColor(this->settings_.distance_text);
        this->settings_.label_text = LoadTextProfile(store, "text.target.labels", this->settings_.label_text);
        this->settings_.value_text = LoadTextProfile(store, "text.target.values", this->settings_.value_text);
        this->settings_.recipient.text = {this->settings_.name_text, this->settings_.distance_text, this->settings_.label_text, this->settings_.value_text};
        this->settings_.recipient.meter = this->settings_.hp_style;
        this->settings_.recipient.fill_top = this->settings_.hp_color_top;
        this->settings_.recipient.fill_bottom = this->settings_.hp_color;
        this->settings_.recipient.Load(store, this->settings_.show_distance, this->settings_.show_percent, this->settings_.show_label);
        this->ResetChild(true);
        this->settings_.subtarget.Load(store, this->settings_.show_distance, this->settings_.show_percent, this->settings_.show_label,
            "target.subtarget", "target.subtarget");
        if (legacyDistanceColor)
        {
            MigrateDistanceColor(this->settings_.recipient.text[1]);
            MigrateDistanceColor(this->settings_.subtarget.text[1]);
        }
        this->settings_.subtarget.selected_text = std::clamp(store.ReadInt("target.subtarget", "selected_text", 0), 0, 4);
        this->subtarget_placement_ = std::clamp(store.ReadInt("target.subtarget", "independent", 0), 0, 1);
        this->subtarget_statuses_ = this->settings_.statuses;
        this->subtarget_statuses_.Load(store, "status.subtarget");
        this->subtarget_status_text_ = LoadTextProfile(store, "text.target.subtarget.status", this->settings_.status_text);
        this->settings_.display.Load(store, this->settings_.hp_color_top, this->settings_.hp_color, this->settings_.hp_style, this->settings_.value_text);
        this->settings_.mob_info.Load(store);
        this->settings_.world_bar.Load(store);
        this->settings_.rings.Load(store);
        this->settings_.selected_text = std::clamp(store.ReadInt("target", "selected_text", 0), 0, 6);
        this->hp_animation_ = {};
        this->state_.bounds = store.ReadRect("layout.target", {24.0f, 128.0f, 440.0f, 82.0f});
        this->cast_state_.bounds = store.ReadRect("layout.target.cast", this->AttachedCastBounds(this->state_.bounds));
        this->cast_positioned_ = store.ReadBool("target.cast", "positioned", !store.ReadString("layout.target.cast", "x").empty());
        this->BeginRenderFrame(); this->EndControllerStatus();
    }

    auto target_module::Save(const settings_store& store) const -> void
    {
        this->timers_.Save(store, "timing.target");
        this->settings_.statuses.Save(store, "status.target");
        SaveTextProfile(store, "text.target.status", this->settings_.status_text);
        store.WriteBool("target", "enabled", this->state_.enabled);
        store.WriteInt("target", "name_anchor_version", 1);
        store.WriteInt("target", "distance_color_version", 1);
        store.WriteBool("target", "hide_when_empty", this->settings_.hide_when_empty);
        store.WriteBool("target", "hide_self", this->settings_.hide_self);
        store.WriteBool("target", "show_distance", this->settings_.show_distance);
        store.WriteBool("target", "show_percent", this->settings_.show_percent);
        store.WriteBool("target", "show_label", this->settings_.show_label);
        this->settings_.recipient.Save(store);
        this->settings_.display.Save(store);
        store.WriteBool("target.cast", "positioned", this->cast_positioned_);
        if (this->cast_positioned_) store.WriteRect("layout.target.cast", this->cast_state_.bounds);
        this->settings_.mob_info.Save(store);
        this->settings_.world_bar.Save(store);
        this->settings_.rings.Save(store);
        this->settings_.subtarget.Save(store, "target.subtarget", "target.subtarget");
        store.WriteInt("target.subtarget", "independent", this->subtarget_placement_);
        this->subtarget_statuses_.Save(store, "status.subtarget");
        SaveTextProfile(store, "text.target.subtarget.status", this->subtarget_status_text_);
        store.WriteInt("target", "selected_text", this->settings_.selected_text);
        store.WriteArgb("target", "hp_color", this->settings_.hp_color);
        store.WriteArgb("meter.target.hp", "fill_top", this->settings_.hp_color_top);
        store.WriteArgb("meter.target.hp", "fill_bottom", this->settings_.hp_color);
        SaveMeterStyle(store, "meter.target.hp", this->settings_.hp_style);
        SaveTextProfile(store, "text.target.name", this->settings_.name_text);
        SaveTextProfile(store, "text.target.distance", this->settings_.distance_text);
        SaveTextProfile(store, "text.target.labels", this->settings_.label_text);
        SaveTextProfile(store, "text.target.values", this->settings_.value_text);
        store.WriteRect("layout.target", this->state_.bounds);
    }

    auto target_module::ContentOptionCount(void) const -> size_t { return hide_self_option + 1; }

    auto target_module::OnContentOptionChanged(const size_t index) -> void
    {
        if (index >= recipient_options_start && index < status_options_start)
            this->settings_.recipient.OnOptionChanged(index - recipient_options_start);
        if (index == display_options_start + target_display_settings::layout_options && this->settings_.display.cast_placement == 1 && !this->cast_positioned_)
        {
            this->cast_state_.bounds = this->AttachedCastBounds(this->state_.bounds);
            this->cast_positioned_ = true;
        }
        if (index >= ring_options_start && index < ring_options_start + target_ring_settings::option_count) this->settings_.rings.Changed(index - ring_options_start);
        if (index >= mob_info_options_start && index < world_options_start) this->settings_.mob_info.OnOptionChanged(index - mob_info_options_start, this->state_.bounds);
    }

    auto target_module::ConfigSections(void) const -> std::span<const module_config_section>
    {
        static constexpr module_config_section sections[]{
            {"General", "Visibility and information shown by this module.", "general"}, {"Layout", "Bar width and placement.", "layout"},
            {"Bar Style", "Window theme or Modern bars, shape and bookends.", "bar-style"}, {"Animation", "Resource change animation.", "animation"},
            {"Effects", "Bar shadows and glow.", "effects"}, {"Colors", "Resource fills and bar effects.", "colors"},
            {"Text", "Choose a text element and edit its style, effects and offsets.", "text"}, {"Attack Timing", "Observed attack timing.", "attack-timing"},
            {"Status Effects", "Native icons, countdowns, placement and effect details.", "status-effects"},
            {"Cast Bar", "Independent main-target cast layout and effects.", "cast-bar"}, {"Entity Colors", "Shared name colors and optional HP gradients by entity type and claim.", "entity-colors"},
            {"Mob Info", "Static monster data integrated into Target. Uses the pinned HeadsUp Phoenix snapshot; unavailable records stay hidden.", "mob-info"}};
        if (this->config_view_ == 4)
        {
            static constexpr module_config_section rings[]{
                {"General", "Terrain-following target rings and optional HP fill.", "general"},
                {"Layout", "Model-relative sizing or fixed radii in world units.", "layout"},
                {"Colors", "Ring tint and opacity.", "colors"}};
            return rings;
        }
        if (this->config_view_ == 3) return BarConfigSections();
        if (this->config_view_ == 1) return target_of_target_settings::ConfigSections();
        return this->config_view_ == 2 ? std::span<const module_config_section>{sections}.first(9) : std::span<const module_config_section>{sections};
    }

    auto target_module::ContentConfigSectionContains(const size_t section, const size_t index) -> bool
    {
        if (section >= this->ConfigSections().size() || index >= this->OptionCount()) return false;
        if (index == hide_self_option) return this->config_view_ == 0 && section == 0;
        if (index >= ring_options_start + target_ring_settings::option_count) return this->config_view_ == 0 && section == 8;
        if (this->config_view_ == 4) return index >= ring_options_start && section == target_ring_settings::Section(index - ring_options_start);
        if (this->config_view_ == 3) return index >= world_options_start && index < ring_options_start && section == world_target_bar::Section(index - world_options_start);
        if (index >= world_options_start) return false;
        if (this->config_view_ == 2) return index >= subtarget_options_start && index < display_options_start && this->ChildContains(true, section, index - subtarget_options_start);
        if (this->config_view_ == 1) return index >= recipient_options_start && index < status_options_start && this->settings_.recipient.Contains(index - recipient_options_start, section);
        if (index >= mob_info_options_start) return section == 11 && this->settings_.mob_info.OptionVisible(index - mob_info_options_start);
        if (index == display_options_start + 19) return section == 11 && this->settings_.mob_info.selected_group == 0;
        if (index >= display_options_start) return section == target_display_settings::Section(index - display_options_start);
        if (index >= subtarget_options_start) return false;
        if (index >= status_options_start) return section == 8;
        if (index >= recipient_options_start) return false;
        if (index >= timing_options_start) return section == 7;
        const auto topic = static_cast<bar_config_section>(section);
        if (index >= 8 && index < 8 + meter_style_option_count) return topic == MeterConfigSection(index - 8);
        if (index == timing_options_start - 1) return topic == bar_config_section::layout;
        if (index >= 8 + meter_style_option_count || index == 4) return topic == bar_config_section::text;
        if (index == 5 || index == 6) return topic == bar_config_section::colors;
        return topic == bar_config_section::general;
    }

    auto target_module::ContentOption(const size_t index) -> module_option
    {
        if (index == hide_self_option)
            return module_option::Toggle("Hide when targeting yourself", "Hide the Target window while your own character is the main target.", &this->settings_.hide_self);
        if (index >= ring_options_start + target_ring_settings::option_count)
        {
            const auto family = index - ring_options_start - target_ring_settings::option_count;
            return this->combat_settings_ && family < 6 ? module_option::Slider(song_bonus_names[family], "Song+ ranks for your own debuff duration estimates; each rank adds 10%. Existing timers are unchanged.", &this->combat_settings_->SongBonuses()[family], 0, 9, 1).Ordered(500 + static_cast<int>(family)) : module_option{};
        }
        if (index >= ring_options_start) return this->settings_.rings.Option(index - ring_options_start);
        if (index >= world_options_start) return this->settings_.world_bar.Option(index - world_options_start);
        if (index >= mob_info_options_start) return this->settings_.mob_info.Option(index - mob_info_options_start);
        if (index >= display_options_start) return this->settings_.display.Option(index - display_options_start);
        if (index >= subtarget_options_start) return this->ChildOption(true, index - subtarget_options_start);
        if (index >= status_options_start) return this->settings_.statuses.Option(index - status_options_start);
        if (index >= recipient_options_start) return this->ChildOption(false, index - recipient_options_start);
        if (index >= timing_options_start) return this->timers_.Option(index - timing_options_start);
        if (index == 8 + meter_style_option_count + text_profile_option_count)
            return ModuleWidthOption(this->state_);
        static constexpr const char* textNames[] = {"Name", "Distance", "Bar label", "Bar value", "Attack timing", "Status timers and details", "Cast"};
        switch (index)
        {
        case 0: return module_option::Toggle("Enabled", "Show the target HUD.", &this->state_.enabled).Ordered(-50);
        case 1: return module_option::Toggle("Hide empty", "Hide when no target exists.", &this->settings_.hide_when_empty);
        case 2: return module_option::Toggle("Distance", "Show target distance.", &this->settings_.show_distance);
        case 3: return module_option::Toggle("HP percent", "Show target health percent.", &this->settings_.show_percent);
        case 4: return module_option::Element("Selected text", "Text element configured in Text.", &this->settings_.selected_text, textNames, 7);
        case 5: return module_option::Color("Fill top", "Upper fill gradient.", &this->settings_.hp_color_top, module_option_group::colors).Ordered(500, true);
        case 6: return module_option::Color("Fill bottom", "Lower fill gradient.", &this->settings_.hp_color, module_option_group::colors).Ordered(501);
        case 7: return module_option::Toggle("HP label", "Show HP inset at the bar's lower edge.", &this->settings_.show_label);
        default:
            if (index < 8 + meter_style_option_count)
                return MeterStyleOption(index - 8, this->settings_.hp_style);
            const auto textIndex = index - 8 - meter_style_option_count;
            auto option = TextProfileOption(textIndex, this->SelectedTextProfile());
            if (this->settings_.selected_text == 0 && (textIndex == 5 || textIndex == 6))
                return option.VisibleWhen(false);
            return option;
        }
    }

    auto target_module::ShouldRender(const hud_snapshot& snapshot) const -> bool
    {
        if (this->settings_.hide_self && this->self_target_) return false;
        return this->state_.enabled && (!this->settings_.hide_when_empty || (this->details_ready_ ? this->views_[0].id != 0 ||
            (!this->ChildIndependent(true) && this->ChildVisible(true, false)) : snapshot.target_valid));
    }

    auto target_module::Update(IAshitaCore* core) -> void
    {
        this->name_color_ = unclaimed_name_color;
        this->has_health_ = true;
        auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        auto party = memory != nullptr ? xiui::ashita::View(memory->GetParty()) : nullptr;
        auto entity = memory != nullptr ? xiui::ashita::View(memory->GetEntity()) : nullptr;
        auto target = memory != nullptr ? xiui::ashita::View(memory->GetTarget()) : nullptr;
        if (party != nullptr && entity != nullptr && target != nullptr)
        {
            const auto index = target->GetTargetIndex(0);
            if (index == 0 || index >= entity->GetEntityMapSize())
                return;
            this->name_color_ = ReadEntityNameColor(*party, *entity, index);
            this->has_health_ = EntityHasHealth(entity->GetSpawnFlags(index));
        }
    }

    auto target_module::ReadView(IAshitaCore& core, const uint32_t index, const combat_source& combat, const party_source& party, target_view& view) -> void
    {
        const auto* memory = core.GetMemoryManager();
        const auto entities = xiui::ashita::View(memory->GetEntity());
        const auto group = xiui::ashita::View(memory->GetParty());
        if (index == 0 || index >= entities->GetEntityMapSize() || entities->GetServerId(index) == 0)
        { view = {}; return; }
        const auto id = entities->GetServerId(index);
        if (view.id != id) view = {};
        view.id = id; view.index = index;
        const auto* name = entities->GetName(index);
        view.name = name != nullptr ? FromGameText(name) : "Target";
        view.hp = std::min<uint32_t>(100, entities->GetHPPercent(index));
        view.distance = std::sqrt(std::max(0.0f, entities->GetDistance(index)));
        const auto entityState = ReadEntityNameState(*group, *entities, index);
        view.color = entityState.color; view.kind = entityState.kind;
        view.mob = MobForEntity(id, static_cast<uint16_t>(group->GetMemberZone(0)), IsMonster(view.kind));
        view.health = EntityHasHealth(entities->GetSpawnFlags(index));
        view.status_count = 0;
        const auto* actor = combat.Find(id);
        if (actor != nullptr)
        {
            view.cast = actor->cast;
            view.status_count = std::min(view.statuses.size(), actor->statuses.size());
            std::copy_n(actor->statuses.begin(), view.status_count, view.statuses.begin());
        }
        else { view.cast.name.clear(); view.cast.started = view.cast.interrupted = 0; view.cast.seconds = 0; }
        for (const auto& member : party.Members())
        {
            if (!member.active || !member.in_zone || member.id != id || member.slot >= 6 || !member.buffs_known) continue;
            // Native party status snapshots are authoritative, including an empty list.
            view.status_count = std::min(view.statuses.size(), member.buff_count);
            for (size_t i = 0; i < view.status_count; ++i)
            {
                view.statuses[i] = {member.buffs[i], 0, 0, member.buff_expires[i], false};
            }
            break;
        }
    }

    auto target_module::UpdateDetails(IAshitaCore* core, const combat_source& combat, const party_source& party, const uint64_t) -> void
    {
        const auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        const auto target = memory != nullptr ? xiui::ashita::View(memory->GetTarget()) : nullptr;
        const auto entities = memory != nullptr ? xiui::ashita::View(memory->GetEntity()) : nullptr;
        const auto player = memory != nullptr ? xiui::ashita::View(memory->GetPlayer()) : nullptr;
        this->details_ready_ = target != nullptr && entities != nullptr && xiui::ashita::View(memory->GetParty()) != nullptr && player != nullptr;
        if (!this->details_ready_ || player->GetIsZoning() || player->GetLoginStatus() != static_cast<uint8_t>(Ashita::FFXI::Enums::LoginStatus::LoggedIn))
        { this->views_ = {}; this->detail_animations_ = {}; this->self_target_ = false; return; }
        const auto main = target->GetTargetIndex(0);
        const auto secondary = target->GetIsSubTargetActive() != 0 ? target->GetTargetIndex(1) : 0;
        const auto update = [&](size_t slot, uint32_t index) {
            const auto previous = this->views_[slot].id;
            this->ReadView(*core, index, combat, party, this->views_[slot]);
            if (previous != this->views_[slot].id) this->detail_animations_[slot] = {};
        };
        update(0, main);
        this->self_target_ = this->views_[0].id != 0 && this->views_[0].id == xiui::ashita::View(memory->GetParty())->GetMemberServerId(0);
        update(1, secondary != main ? secondary : 0);
        const auto* actor = combat.Find(this->views_[0].id);
        const auto recipient = actor != nullptr ? actor->target : 0;
        update(2, recipient != 0 && recipient != this->views_[0].id && recipient != this->views_[1].id ? FindEntityIndex(*core, recipient) : 0);
        const auto* autoFollow = memory->GetAutoFollow();
        this->views_[0].locked = this->views_[0].id != 0 && autoFollow != nullptr && autoFollow->GetIsCameraLockedOn() != 0;
    }

    auto target_module::DrawView(const target_view& view, const ui::rect& panel, const char* prefix, meter_animation_state& animation,
        const uint64_t now, const module_render_context& context, const target_of_target_settings* presentation) -> float
    {
        const bool xiv1 = context.Design().IsXiv1Theme();
        const bool valid = view.id != 0;
        animation.Bind(view.id);
        const bool recipient = presentation != nullptr;
        const auto& options = recipient ? *presentation : this->settings_.recipient;
        const bool subtarget = presentation == &this->settings_.subtarget;
        const auto& statusSettings = subtarget ? this->subtarget_statuses_ : this->settings_.statuses;
        const auto& statusText = subtarget ? this->subtarget_status_text_ : this->settings_.status_text;
        const bool showHealth = (!valid || view.health) && (!recipient || options.health);
        const bool showDistance = recipient ? options.distance : this->settings_.show_distance;
        const bool showName = recipient ? options.name : this->settings_.display.name;
        const bool castHeader = xiv1 && !recipient && this->settings_.display.casts && this->settings_.display.cast_placement == 0;
        const bool nameInside = recipient && options.name_placement == 1 && showHealth;
        const bool nameBelow = recipient && options.name_placement == 2;
        const bool nameAbove = showName && !nameInside && !nameBelow;
        const bool captionDistance = xiv1 && (nameAbove || castHeader);
        auto profiles = std::array{this->settings_.name_text, this->settings_.distance_text, this->settings_.label_text, this->settings_.value_text};
        if (recipient && !options.follow_text) profiles = options.text;
        auto nameText = valid && (!recipient || options.tint_name) ? TintTextProfile(profiles[0], view.color) : profiles[0];
        if (xiv1) nameText.style.font = nameText.style.ThemeFont(ui::xiv1_italic_font);
        std::string level;
        if (valid && (recipient ? options.mob_level : this->settings_.display.mob_level))
            if (const auto* mob = view.mob; mob != nullptr && mob->minimum_level != 0 && mob->maximum_level != 0)
                level = " " + MobLevel(*mob);
        const auto label = std::string{recipient && !options.prefix ? "" : prefix} + (valid ? view.name : "NO TARGET") + level +
            (!recipient && IsMonster(view.kind) ? TargetIdSuffix(view.id, this->settings_.display.id_format) : "");
        const float nameInset = !subtarget ? 17.0f : xiv1 ? 22.0f : 7.0f;
        const float nameWidth = std::max(1.0f, showDistance && valid && !captionDistance ? panel.width * 0.6f - nameInset : panel.width - nameInset - 7);
        auto name = label;
        while (name.size() > 3 && context.Design().MeasureText(name.c_str(), nameText.style) > nameWidth)
        {
            size_t last = name.size() - 1;
            while (last > 0 && (static_cast<unsigned char>(name[last]) & 0xC0) == 0x80) --last;
            name.resize(last);
        }
        auto meterStyle = recipient && !options.follow_meter ? options.meter : this->settings_.hp_style;
        if (xiv1) { meterStyle.height = 12; meterStyle.bookends = true; }
        const float meterHeight = xiv1 ? 14 : recipient ? options.BarHeight(this->settings_.hp_style.height + 2) : std::max(1.0f, meterStyle.height) + 2;
        if (recipient) meterStyle.height = std::max(1.0f, meterHeight - 2);
        const ui::rect nameBounds{panel.x + nameInset, panel.y + (xiv1 ? 8.0f : 5.0f), nameWidth, 20};
        ui::rect distanceBounds{panel.x + panel.width * 0.6f, panel.y + 5, panel.width * 0.38f - 7, 20};
        auto distanceText = profiles[1];
        char distance[32]{};
        if (showDistance && valid) std::snprintf(distance, sizeof(distance), "%.1f yalms", view.distance);
        constexpr float tickToRail = 12;
        ui::rect flourish{panel.x + 7 + tickToRail, panel.y + 21, panel.width - 9 - tickToRail, 7};
        ui::rect nameTextBounds{};
        float header = showName || showDistance || castHeader ? xiv1 ? 36.0f : 31.0f : 5.0f;
        if (xiv1 && (nameAbove || castHeader))
        {
            auto headerText = nameText;
            if (castHeader) headerText.visible = true;
            nameTextBounds = context.TextBounds(name.c_str(), nameBounds, panel, headerText);
            const float textBottom = nameTextBounds.y + nameTextBounds.height;
            flourish.y = std::max(panel.y + 5, textBottom) + 1;
            // Keep a one-pixel gap below the flipped rail's flat edge.
            header = flourish.y + 5 - panel.y - (meterHeight - meterStyle.height) * .5f;
            if (castHeader)
            {
                const float minimum = this->CastHeight(context.Design()) + 7;
                flourish.y += std::max(0.0f, minimum - header);
                header = std::max(header, minimum);
            }
        }
        float bottom = panel.y + header - 2;
        char hpValue[24]{};
        const bool percentType = IsMonster(view.kind) ? this->settings_.display.percent_mobs :
            view.kind == entity_class::player || view.kind == entity_class::group_player ? this->settings_.display.percent_players : true;
        if ((recipient ? options.percent : this->settings_.show_percent && percentType) && valid) std::snprintf(hpValue, sizeof(hpValue), "%u%%", view.hp);
        const auto kind = static_cast<size_t>(view.kind);
        const auto fillTop = this->settings_.display.entity_fills ? this->settings_.display.fill_top[kind] : this->settings_.hp_color_top;
        const auto fillBottom = this->settings_.display.entity_fills ? this->settings_.display.fill_bottom[kind] : this->settings_.hp_color;
        if (showHealth)
        {
            const ui::rect hp{panel.x + 7, panel.y + header, panel.width - 14, meterHeight};
            if (view.locked && (recipient || this->settings_.display.lock))
            {
                const auto color = context.Design().GetTheme().colors.accent;
                context.DrawSelection({hp.x - 2, hp.y - 2, hp.width + 4, hp.height + 4}, false, {color, color, 0.06f, 0, 0, false});
            }
            const char* hpLabel = showName && nameInside ? name.c_str() : !xiv1 && (recipient ? options.label : this->settings_.show_label) ? "HP" : "";
            auto hpText = profiles[3];
            if (xiv1) hpText.offset_y += 1;
            context.DrawMeter(hp, hpLabel, hpValue, valid ? module_render_context::Ratio(view.hp, 100) : 0,
                recipient && !options.follow_fill ? options.fill_top : fillTop,
                recipient && !options.follow_fill ? options.fill_bottom : fillBottom,
                meterStyle, animation, true, showName && nameInside ? &nameText : &profiles[2], &hpText,
                nameInside ? ui::meter_text_anchor::center : ui::meter_text_anchor::bottom, ui::meter_text_anchor::automatic, meter_fill_mode::health);
            bottom = hp.y + hp.height + std::max(16.0f, context.Design().TextHeight(profiles[3].style)) * 0.5f + 4;
        }
        if (showName && nameBelow)
        {
            const ui::rect at{panel.x + 7, bottom + 2, panel.width - 14, std::max(16.0f, context.Design().TextHeight(nameText.style))};
            context.DrawText(name.c_str(), at, at, nameText);
            bottom = at.y + at.height + 2;
        }
        if (recipient && options.cast && view.cast.Active(now))
        {
            auto castStyle = !recipient && this->settings_.display.custom_cast_meter ? this->settings_.display.cast_meter : meterStyle;
            castStyle.height = recipient ? options.cast_height : this->settings_.display.cast_height;
            const ui::rect cast{panel.x + 7, bottom + (recipient ? 3 : this->settings_.display.cast_spacing), panel.width - 14, castStyle.height + 2};
            const auto color = context.Design().GetTheme().colors.accent;
            context.DrawBar(cast, view.cast.Ratio(now), !recipient && this->settings_.display.custom_cast_fill ? this->settings_.display.cast_top : MeterGradientTop(color),
                !recipient && this->settings_.display.custom_cast_fill ? this->settings_.display.cast_bottom : color, 0, castStyle);
            auto text = recipient ? profiles[3] : this->settings_.display.cast_text; text.style.alignment = ui::text_alignment::center;
            const ui::rect line{cast.x, cast.y + cast.height + 2, cast.width, std::max(16.0f, context.Design().TextHeight(text.style))};
            context.DrawText(view.cast.interrupted ? i18n::Text("Interrupted") : view.cast.name.c_str(), line, line, text);
            bottom = line.y + line.height + 2;
        }
        if (this->timers_.enabled && (!recipient || options.attack))
        {
            auto attack = this->timing_ != nullptr ? this->timing_->Enemy(view.id, now / 1000.0) : attack_prediction{};
            if (context.IsEditing()) attack = {true, true, false, 0.6f, 1.6f, 4};
            const auto timingHeight = ui::DrawTiming(attack, "Attack", "", {panel.x + 7, bottom + 3, panel.width - 14, 0}, meterStyle,
                recipient ? profiles[1] : this->timers_.text, this->timers_.height, this->timers_.text_only, this->timers_.drain,
                this->timers_.color, this->timers_.ready_color, 0.5f, context);
            if (timingHeight > 0) bottom += timingHeight + 3;
        }
        if (!recipient)
        {
            const auto infoHeight = this->settings_.mob_info.Draw(view.mob, {panel.x + 7, bottom + 3, panel.width - 14, 0}, context,
                mob_info_group_count, xiv1 && nameAbove);
            if (infoHeight > 0) bottom += infoHeight + 3;
        }
        const auto statuses = std::span<const combat_status>{view.statuses}.first(!recipient || options.statuses ? view.status_count : 0);
        const ui::rect strip{panel.x + 7, bottom + 2, panel.width - 14, 0};
        const auto layout = ui::LayoutStatusStrip(statuses, strip, now, statusText, context.Design(), statusSettings);
        ui::DrawStatusStrip(statuses, strip, now, statusText, context, statusSettings, &layout);
        if (statusSettings.tooltips)
            for (size_t i = 0; i < layout.count; ++i) this->status_hits_.push_back({statuses[i], context.Design().Transform().Map(layout.Icon(i)), view.id});
        if (nameAbove || castHeader)
        {
            if (xiv1)
            {
                if (context.Containers() != nullptr) context.Containers()->DrawXiv1TargetHeader(context.Design(), flourish);
                constexpr float captionScale = .72f;
                auto caption = profiles[2]; caption.style.font = caption.style.ThemeFont(ui::xiv1_italic_font);
                caption.style.scale *= captionScale;
                caption.style.alignment = ui::text_alignment::right;
                const char* captionText = recipient ? subtarget ? "Subtarget" : "Target of Target" : "Target";
                const float captionWidth = context.Design().MeasureText(captionText, caption.style);
                const float captionHeight = context.Design().TextHeight(caption.style);
                const ui::rect at{flourish.x + flourish.width - captionWidth - 8, flourish.y + (flourish.height - captionHeight) * .5f - 1, captionWidth, captionHeight};
                context.DrawText(captionText, at, at, TintTextProfile(caption, 0xFFE6D6A6));
                const auto captionBounds = caption.visible ? context.TextBounds(captionText, at, at, caption) : at;
                if (!recipient)
                    this->settings_.mob_info.DrawHeaderIcons(view.mob, {panel.x + 7, captionBounds.y - 2,
                        captionBounds.x + captionBounds.width - panel.x - 7, 0}, context);
                if (showDistance && valid)
                {
                    distanceText.style.scale *= captionScale;
                    distanceText.style.alignment = ui::text_alignment::left;
                    distanceText.placement = 0;
                    const float distanceHeight = context.Design().TextHeight(distanceText.style);
                    distanceBounds = {captionBounds.x + captionBounds.width + 8, captionBounds.y + (captionBounds.height - distanceHeight) * .5f,
                        context.Design().MeasureText(distance, distanceText.style), distanceHeight};
                }
                const auto color = valid ? view.color : 0xFFAAA28D;
                const ui::rect badge{flourish.x - 37, flourish.y + 1, 13, 13};
                context.Design().DrawRing({badge.x + 3, badge.y + 3, 7, 7}, color, 2);
                if (subtarget) nameText.offset_x -= 8;
                nameText.offset_y += 4;
            }
            if (castHeader && view.cast.Active(now))
                this->DrawCast(view.cast, {nameBounds.x, panel.y + 3, nameBounds.width, std::max(1.0f, header - 7)}, now, context);
            else if (nameAbove)
                context.DrawText(name.c_str(), nameBounds, panel, nameText);
        }
        if (showDistance && valid)
        {
            context.DrawText(distance, distanceBounds, captionDistance ? distanceBounds : panel, distanceText);
        }
        bottom += ui::StatusStripHeight(layout, statusSettings);
        return bottom - panel.y;
    }

    auto target_module::AttachedCastBounds(const ui::rect& panel) const -> ui::rect
    {
        const auto& cast = this->settings_.display;
        const float header = cast.name || this->settings_.show_distance ? 31.0f : 5.0f;
        return {panel.x + 7 + cast.cast_offset_x,
            panel.y + header + this->settings_.hp_style.height + 2 + cast.cast_spacing + cast.cast_offset_y,
            cast.cast_width > 0 ? cast.cast_width : std::max(1.0f, panel.width - 14), cast.cast_height + 2};
    }

    auto target_module::DrawCast(const combat_cast& cast, const ui::rect& bounds, const uint64_t now, const module_render_context& context) -> void
    {
        if (!this->settings_.display.casts || (!cast.Active(now) && !context.IsEditing())) return;
        const auto& options = this->settings_.display;
        auto style = options.custom_cast_meter ? options.cast_meter : this->settings_.hp_style;
        style.height = options.cast_height;
        const auto color = context.Design().GetTheme().colors.accent;
        if (context.Design().IsXiv1Theme())
        {
            style.appearance = ui::meter_style::window_theme;
            style.height = std::clamp(options.cast_height, 2.0f, 6.0f);
            style.bookends = true;
            style.themed_bookends = true;
            style.border_width = 1;
            style.corner_radius = 2;
            const ui::rect track{bounds.x, bounds.y + bounds.height - style.height - 2, bounds.width, style.height + 2};
            context.DrawBar(track, cast.Active(now) ? cast.Ratio(now) : .6f,
                options.custom_cast_fill ? options.cast_top : 0xFFEDCAB5,
                options.custom_cast_fill ? options.cast_bottom : 0xFFB98370, 0, style);
            auto text = cast.interrupted ? TintTextProfile(options.cast_text, 0xFFFF8888) : options.cast_text;
            text.style.font = text.style.ThemeFont(ui::xiv1_italic_font);
            text.style.alignment = ui::text_alignment::left;
            text.placement = 0;
            const float available = std::max(1.0f, track.y - bounds.y - 2);
            const float height = context.Design().TextHeight(text.style);
            text.style.scale *= std::min(1.0f, std::min(18.0f, available) / std::max(1.0f, height));
            const auto trackBounds = ui::LayoutMeterTrack(track, style);
            const auto textBounds = context.MeterTextBounds(track, style);
            const float labelHeight = context.Design().TextHeight(text.style);
            const ui::rect label{textBounds.x, trackBounds.y - labelHeight + 3, textBounds.width, labelHeight};
            const auto fitted = ui::FitText(context.Design(), cast.Active(now) ? (cast.interrupted ? "Interrupted" : cast.name.c_str()) : "Stone IV",
                std::max(0.0f, label.width - std::max(0.0f, text.offset_x)), context.Design().ResolveHudText(text.style));
            context.DrawText(fitted.c_str(), label, label, text);
            return;
        }
        context.DrawBar(bounds, cast.Active(now) ? cast.Ratio(now) : 0.6f,
            options.custom_cast_fill ? options.cast_top : MeterGradientTop(color), options.custom_cast_fill ? options.cast_bottom : color, 0, style);
        auto text = options.cast_text;
        text.style.alignment = ui::text_alignment::center;
        const float height = std::max(bounds.height, context.Design().TextHeight(text.style));
        const ui::rect line{bounds.x, bounds.y + (bounds.height - height) * 0.5f, bounds.width, height};
        context.DrawText(cast.Active(now) ? (cast.interrupted ? i18n::Text("Interrupted") : cast.name.c_str()) : i18n::Text("Stone IV"), line, line, text);
    }

    auto target_module::CastHeight(const ui::design_system& design) const -> float
    {
        const auto& options = this->settings_.display;
        if (!design.IsXiv1Theme()) return options.cast_height + 2;
        return std::clamp(design.TextHeight(options.cast_text.style), 12.0f, 18.0f) + std::clamp(options.cast_height, 2.0f, 6.0f) + 4;
    }
    auto target_module::RenderWorldBar(const hud_snapshot& snapshot, const module_render_context& context) -> void
    {
        const auto& display = this->settings_.display;
        auto world = snapshot;
        auto kind = this->views_[0].kind;
        auto nameColor = this->views_[0].color;
        auto& bar = this->settings_.world_bar;
        const auto* heal = bar.heal_preview.enabled && bar.healing != nullptr ? bar.healing->Primary(::GetTickCount64()) : nullptr;
        if (heal != nullptr)
        {
            world.target_server_id = heal->id; world.target_valid = true; world.target_hp_percent = heal->percent;
            std::snprintf(world.target_name, sizeof(world.target_name), "%s", heal->name.c_str());
            world.target_screen_valid = false;
            for (const auto& anchor : snapshot.party_anchors)
                if (anchor.id == heal->id)
                { world.target_screen_valid = anchor.valid; world.target_screen_x = anchor.x; world.target_screen_y = anchor.y; break; }
            kind = entity_class::group_player; nameColor = EntityNameColor(1, 0, false, true);
        }
        const auto top = display.entity_fills ? display.fill_top[static_cast<size_t>(kind)] : this->settings_.hp_color_top;
        const auto bottom = display.entity_fills ? display.fill_bottom[static_cast<size_t>(kind)] : this->settings_.hp_color;
        bar.Draw(world, world.target_server_id, nameColor, top, bottom, context);
    }

    auto target_module::Render(const hud_snapshot& snapshot, const module_render_context& context) -> void
    {
        target_view fallback{};
        if (!this->details_ready_)
        {
            fallback.id = snapshot.target_valid ? 1 : 0; fallback.health = this->has_health_;
            fallback.name = snapshot.target_name; fallback.hp = snapshot.target_hp_percent;
            fallback.color = this->name_color_; fallback.distance = snapshot.target_distance;
        }
        const auto now = ::GetTickCount64();
        const auto& main = this->details_ready_ ? this->views_[0] : fallback;
        float height = this->DrawView(main, this->state_.bounds, "", this->detail_animations_[0], now, context);
        if (this->settings_.display.cast_placement == 0 && !context.Design().IsXiv1Theme())
        {
            this->DrawCast(main.cast, this->AttachedCastBounds(this->state_.bounds), now, context);
        }
        target_view preview{};
        if (context.IsEditing())
        {
            preview.id = 0xFFFFFFFF;
            preview.name = "Party Member";
            preview.hp = 84; preview.health = true; preview.distance = 8.2f;
            preview.color = context.Design().GetTheme().colors.text;
        }
        for (const size_t slot : {size_t{2}, size_t{1}})
        {
            const bool recipient = slot == 2;
            if (this->ChildIndependent(!recipient) || !this->ChildVisible(!recipient, context.IsEditing())) continue;
            const auto& view = this->views_[slot].id == 0 ? preview : this->views_[slot];
            if (view.id == 0) continue;
            const float mainBarHeight = this->settings_.hp_style.height + 2;
            const auto& options = recipient ? this->settings_.recipient : this->settings_.subtarget;
            const ui::rect area = recipient ? options.Area(this->state_.bounds, height, mainBarHeight) :
                ui::rect{this->state_.bounds.x + options.offset_x, this->state_.bounds.y + height + options.gap + options.offset_y,
                    options.width > 0 ? options.width : this->state_.bounds.width, 0};
            if (recipient && this->settings_.recipient.ShowArrow())
            {
                const auto& options = this->settings_.recipient;
                context.Design().DrawIcon(options.Arrow(this->state_.bounds, mainBarHeight), ui::icon_glyph::target_arrow,
                    options.arrow_color, 0, options.placement == target_of_target_settings::inline_left);
            }
            const auto drawn = this->DrawView(view, area, slot == 1 ? "Subtarget: " : "Target: ", this->detail_animations_[slot], now, context, &options);
            height = std::max(height, area.y - this->state_.bounds.y + drawn);
        }
    }

    auto target_cast_module::HasEditableBounds(void) const -> bool
    {
        const auto& options = this->parent_.settings_.display;
        return this->parent_.state_.enabled && options.casts && options.cast_placement == 1;
    }

    auto target_cast_module::ShouldRender(const hud_snapshot&) const -> bool
    {
        return this->HasEditableBounds() && this->parent_.mob_info_display_allowed_ && this->parent_.views_[0].id != 0 &&
            this->parent_.views_[0].cast.Active(::GetTickCount64());
    }

    auto target_cast_module::Render(const hud_snapshot&, const module_render_context& context) -> void
    {
        auto& state = this->State();
        const auto anchor = this->parent_.AttachedCastBounds(this->parent_.state_.bounds);
        state.bounds.width = anchor.width; state.minimum_height = state.bounds.height = this->parent_.CastHeight(context.Design());
        this->parent_.DrawCast(this->parent_.views_[0].cast, state.bounds, ::GetTickCount64(), context);
    }

    auto target_module::BeginControllerStatus(void) -> bool
    {
        this->EndControllerStatus();
        if (!this->HasStatusDetails()) return false;
        this->controller_status_entity_ = this->status_hits_.front().entity;
        this->controller_status_id_ = this->status_hits_.front().status.id;
        return true;
    }

    auto target_module::MoveControllerStatus(const int direction) -> bool
    {
        if (direction == 0 || this->status_hits_.empty()) return false;
        const auto current = ui::StatusHitIndex(this->status_hits_, this->controller_status_entity_, this->controller_status_id_, this->controller_status_occurrence_);
        if (!current) return false;
        const auto index = (*current + (direction > 0 ? 1 : this->status_hits_.size() - 1)) % this->status_hits_.size();
        const auto& next = this->status_hits_[index];
        this->controller_status_entity_ = next.entity; this->controller_status_id_ = next.status.id;
        this->controller_status_occurrence_ = ui::StatusHitOccurrence(this->status_hits_, index);
        return true;
    }

    auto target_module::ControllerStatusBounds(void) const -> ui::rect
    {
        const auto index = ui::StatusHitIndex(this->status_hits_, this->controller_status_entity_, this->controller_status_id_, this->controller_status_occurrence_);
        return index ? this->status_hits_[*index].bounds : ui::rect{};
    }

    auto target_module::RenderStatusTooltip(const module_render_context& context, IResourceManager* resources,
        const float x, const float y, const bool controller) const -> void
    {
        const auto focused = ui::StatusHitIndex(this->status_hits_, this->controller_status_entity_, this->controller_status_id_, this->controller_status_occurrence_);
        for (const auto& hit : this->status_hits_)
        {
            const auto& at = hit.bounds;
            if (controller ? focused && &hit == &this->status_hits_[*focused] :
                x >= at.x && x < at.x + at.width && y >= at.y && y < at.y + at.height)
            {
                const auto& text = hit.entity == this->views_[1].id ? this->subtarget_status_text_ : this->settings_.status_text;
                ui::DrawStatusTooltip(hit, resources, text, context);
                return;
            }
        }
    }
} // namespace xiui
