#include "target_module.hpp"
#include "modules/bar_config.hpp"
#include "chat/chat_encoding.hpp"
#include "ui/status_strip.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace xiui
{
    auto target_module::Id(void) const -> const char* { return "target"; }
    auto target_module::DisplayName(void) const -> const char* { return "Target Bar"; }
    auto target_module::State(void) -> module_state& { return this->state_; }
    auto target_module::State(void) const -> const module_state& { return this->state_; }
    auto target_module::Settings(void) -> settings& { return this->settings_; }

    auto target_module::ApplyMeterDefaults(const ui::meter_style& style) -> void
    {
        this->settings_.hp_style = style;
    }

    auto target_module::ApplyTextDefaults(const ui::theme& theme) -> void
    {
        this->timers_.text = {theme.caption_text};
        this->settings_.name_text = {theme.title_text};
        this->settings_.distance_text = {theme.caption_text};
        this->settings_.distance_text.style.alignment = ui::text_alignment::right;
        this->settings_.label_text = {theme.meter_text};
        this->settings_.label_text.style.alignment = ui::text_alignment::left;
        this->settings_.value_text = {theme.meter_text};
        this->settings_.value_text.style.alignment = ui::text_alignment::right;
        this->settings_.recipient.text = {this->settings_.name_text, this->settings_.distance_text, this->settings_.label_text, this->settings_.value_text};
    }

    auto target_module::ResetDefaults(const ui::theme& themeDefaults) -> void
    {
        const ui::rect bounds = this->state_.bounds;
        const auto* timing = this->timing_;
        *this = target_module{};
        this->timing_ = timing;
        this->state_.bounds = bounds;
        this->ApplyMeterDefaults(themeDefaults.meter);
        this->ApplyTextDefaults(themeDefaults);
    }

    auto target_module::SelectedTextProfile(void) -> module_text_profile&
    {
        if (this->settings_.selected_text == 4) return this->timers_.text;
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
        this->state_.enabled = store.ReadBool("target", "enabled", true);
        this->settings_.hide_when_empty = store.ReadBool("target", "hide_when_empty", false);
        this->settings_.show_distance = store.ReadBool("target", "show_distance", true);
        this->settings_.show_percent = store.ReadBool("target", "show_percent", true);
        this->settings_.show_label = store.ReadBool("target", "show_label", true);
        const D3DCOLOR hpFallback = store.ReadArgb("target", "hp_color", D3DCOLOR_XRGB(251, 148, 148));
        this->settings_.hp_color_top = store.ReadArgb("meter.target.hp", "fill_top", MeterGradientTop(hpFallback));
        this->settings_.hp_color = store.ReadArgb("meter.target.hp", "fill_bottom", hpFallback);
        this->settings_.hp_style = LoadMeterStyle(store, "meter.target.hp", this->settings_.hp_style);
        this->settings_.name_text = LoadTextProfile(store, "text.target.name", this->settings_.name_text);
        this->settings_.distance_text = LoadTextProfile(store, "text.target.distance", this->settings_.distance_text);
        this->settings_.label_text = LoadTextProfile(store, "text.target.labels", this->settings_.label_text);
        this->settings_.value_text = LoadTextProfile(store, "text.target.values", this->settings_.value_text);
        this->settings_.recipient.text = {this->settings_.name_text, this->settings_.distance_text, this->settings_.label_text, this->settings_.value_text};
        this->settings_.recipient.meter = this->settings_.hp_style;
        this->settings_.recipient.fill_top = this->settings_.hp_color_top;
        this->settings_.recipient.fill_bottom = this->settings_.hp_color;
        this->settings_.recipient.Load(store, this->settings_.show_distance, this->settings_.show_percent, this->settings_.show_label);
        this->settings_.selected_text = std::clamp(store.ReadInt("target", "selected_text", 0), 0, 4);
        this->hp_animation_ = {};
        this->state_.bounds = store.ReadRect("layout.target", {24.0f, 128.0f, 440.0f, 82.0f});
    }

    auto target_module::Save(const settings_store& store) const -> void
    {
        this->timers_.Save(store, "timing.target");
        store.WriteBool("target", "enabled", this->state_.enabled);
        store.WriteBool("target", "hide_when_empty", this->settings_.hide_when_empty);
        store.WriteBool("target", "show_distance", this->settings_.show_distance);
        store.WriteBool("target", "show_percent", this->settings_.show_percent);
        store.WriteBool("target", "show_label", this->settings_.show_label);
        this->settings_.recipient.Save(store);
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

    auto target_module::OptionCount(void) const -> size_t { return recipient_options_start + target_of_target_settings::option_count; }

    auto target_module::ConfigSections(void) const -> std::span<const module_config_section>
    {
        return this->config_view_ == 1 ? target_of_target_settings::ConfigSections() : ui::TimingSections();
    }

    auto target_module::ConfigSectionContains(const size_t section, const size_t index) -> bool
    {
        if (section >= this->ConfigSections().size() || index >= this->OptionCount()) return false;
        if (this->config_view_ == 1) return index >= recipient_options_start && this->settings_.recipient.Contains(index - recipient_options_start, section);
        if (index >= recipient_options_start) return false;
        if (index >= timing_options_start) return section == 7;
        const auto topic = static_cast<bar_config_section>(section);
        if (index >= 8 && index < 8 + meter_style_option_count) return topic == MeterConfigSection(index - 8);
        if (index == timing_options_start - 1) return topic == bar_config_section::layout;
        if (index >= 8 + meter_style_option_count || index == 4) return topic == bar_config_section::text;
        if (index == 5 || index == 6) return topic == bar_config_section::colors;
        return topic == bar_config_section::general;
    }

    auto target_module::Option(const size_t index) -> module_option
    {
        if (index >= recipient_options_start) return this->settings_.recipient.Option(index - recipient_options_start);
        if (index >= timing_options_start) return this->timers_.Option(index - timing_options_start);
        if (index == 8 + meter_style_option_count + text_profile_option_count)
            return ModuleWidthOption(this->state_);
        static constexpr const char* textNames[] = {"Name", "Distance", "Bar label", "Bar value", "Attack timing"};
        switch (index)
        {
        case 0: return module_option::Toggle("Enabled", "Show the target HUD.", &this->state_.enabled);
        case 1: return module_option::Toggle("Hide empty", "Hide when no target exists.", &this->settings_.hide_when_empty);
        case 2: return module_option::Toggle("Distance", "Show target distance.", &this->settings_.show_distance);
        case 3: return module_option::Toggle("HP percent", "Show target health percent.", &this->settings_.show_percent);
        case 4: return module_option::Element("Selected text", "Text element configured in Text.", &this->settings_.selected_text, textNames, 5);
        case 5: return module_option::Color("Fill top", "Upper fill gradient.", &this->settings_.hp_color_top, module_option_group::colors);
        case 6: return module_option::Color("Fill bottom", "Lower fill gradient.", &this->settings_.hp_color, module_option_group::colors);
        case 7: return module_option::Toggle("HP label", "Show HP inset at the bar's lower edge.", &this->settings_.show_label);
        default:
            if (index < 8 + meter_style_option_count)
                return MeterStyleOption(index - 8, this->settings_.hp_style);
            const auto textIndex = index - 8 - meter_style_option_count;
            auto option = TextProfileOption(textIndex, this->SelectedTextProfile());
            if (this->settings_.selected_text == 0 && (textIndex == 5 || textIndex == 6))
            {
                option.available = false;
                option.description = "Name color follows the game state.";
            }
            return option;
        }
    }

    auto target_module::ShouldRender(const hud_snapshot& snapshot) const -> bool
    {
        return this->state_.enabled && (!this->settings_.hide_when_empty || (this->details_ready_ ? this->views_[0].id != 0 || this->views_[1].id != 0 : snapshot.target_valid));
    }

    auto target_module::Update(IAshitaCore* core) -> void
    {
        this->name_color_ = unclaimed_name_color;
        this->has_health_ = true;
        auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        auto* party = memory != nullptr ? memory->GetParty() : nullptr;
        auto* entity = memory != nullptr ? memory->GetEntity() : nullptr;
        auto* target = memory != nullptr ? memory->GetTarget() : nullptr;
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
        const auto* entities = memory->GetEntity();
        const auto* group = memory->GetParty();
        if (index == 0 || index >= entities->GetEntityMapSize() || entities->GetServerId(index) == 0)
        { view = {}; return; }
        const auto id = entities->GetServerId(index);
        if (view.id != id) view = {};
        view.id = id; view.index = index;
        const auto* name = entities->GetName(index);
        view.name = name != nullptr ? FromGameText(name) : "Target";
        view.hp = std::min<uint32_t>(100, entities->GetHPPercent(index));
        view.distance = std::sqrt(std::max(0.0f, entities->GetDistance(index)));
        view.color = ReadEntityNameColor(*group, *entities, index);
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
            if (!member.active || !member.in_zone || member.id != id || member.slot >= 6) continue;
            // Native party status snapshots are authoritative, including an empty list.
            view.status_count = std::min(view.statuses.size(), member.buff_count);
            for (size_t i = 0; i < view.status_count; ++i)
            {
                const auto effect = member.buffs[i];
                const auto known = actor != nullptr ? std::find_if(actor->statuses.begin(), actor->statuses.end(), [effect](const auto& value) { return value.id == effect; }) : std::vector<combat_status>::const_iterator{};
                view.statuses[i] = actor != nullptr && known != actor->statuses.end() ? *known : combat_status{effect};
                view.statuses[i].uncertain = false;
            }
            break;
        }
    }

    auto target_module::UpdateDetails(IAshitaCore* core, const combat_source& combat, const party_source& party, const uint64_t) -> void
    {
        const auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        const auto* target = memory != nullptr ? memory->GetTarget() : nullptr;
        const auto* entities = memory != nullptr ? memory->GetEntity() : nullptr;
        const auto* player = memory != nullptr ? memory->GetPlayer() : nullptr;
        this->details_ready_ = target != nullptr && entities != nullptr && memory->GetParty() != nullptr && player != nullptr;
        if (!this->details_ready_ || player->GetIsZoning() || player->GetLoginStatus() != static_cast<uint8_t>(Ashita::FFXI::Enums::LoginStatus::LoggedIn))
        { this->views_ = {}; this->detail_animations_ = {}; return; }
        const auto main = target->GetTargetIndex(0);
        const auto secondary = target->GetIsSubTargetActive() != 0 ? target->GetTargetIndex(1) : 0;
        const auto update = [&](size_t slot, uint32_t index) {
            const auto previous = this->views_[slot].id;
            this->ReadView(*core, index, combat, party, this->views_[slot]);
            if (previous != this->views_[slot].id) this->detail_animations_[slot] = {};
        };
        update(0, main);
        update(1, secondary != main ? secondary : 0);
        const auto* actor = combat.Find(this->views_[0].id);
        const auto recipient = actor != nullptr ? actor->target : 0;
        update(2, recipient != 0 && recipient != this->views_[0].id && recipient != this->views_[1].id ? FindEntityIndex(*core, recipient) : 0);
        const auto* autoFollow = memory->GetAutoFollow();
        this->views_[0].locked = this->views_[0].id != 0 && autoFollow != nullptr && autoFollow->GetIsCameraLockedOn() != 0;
    }

    auto target_module::DrawView(const target_view& view, const ui::rect& panel, const char* prefix, meter_animation_state& animation,
        const uint64_t now, const module_render_context& context, const bool recipient) -> float
    {
        const bool valid = view.id != 0;
        const auto& options = this->settings_.recipient;
        const bool showHealth = (!valid || view.health) && (!recipient || options.health);
        const bool showDistance = recipient ? options.distance : this->settings_.show_distance;
        const bool showName = !recipient || options.name;
        const bool nameInside = recipient && options.name_placement == 1 && showHealth;
        const bool nameBelow = recipient && options.name_placement == 2;
        auto profiles = std::array{this->settings_.name_text, this->settings_.distance_text, this->settings_.label_text, this->settings_.value_text};
        if (recipient && !options.follow_text) profiles = options.text;
        auto nameText = valid && (!recipient || options.tint_name) ? TintTextProfile(profiles[0], view.color) : profiles[0];
        const auto label = std::string{recipient && !options.prefix ? "" : prefix} + (valid ? view.name : "NO TARGET");
        const float nameWidth = std::max(1.0f, showDistance && valid ? panel.width * 0.6f - 7 : panel.width - 14);
        auto name = label;
        while (name.size() > 3 && context.Design().MeasureText(name.c_str(), nameText.style) > nameWidth)
        {
            size_t last = name.size() - 1;
            while (last > 0 && (static_cast<unsigned char>(name[last]) & 0xC0) == 0x80) --last;
            name.resize(last);
        }
        auto meterStyle = recipient && !options.follow_meter ? options.meter : this->settings_.hp_style;
        const float meterHeight = recipient ? options.BarHeight(this->settings_.hp_style.height + 2) : std::max(1.0f, meterStyle.height) + 2;
        if (recipient) meterStyle.height = std::max(1.0f, meterHeight - 2);
        float bottom = panel.y + 29;
        char hpValue[24]{};
        if ((recipient ? options.percent : this->settings_.show_percent) && valid) std::snprintf(hpValue, sizeof(hpValue), "%u%%", view.hp);
        if (showHealth)
        {
            const ui::rect hp{panel.x + 7, panel.y + 31, panel.width - 14, meterHeight};
            if (view.locked)
            {
                const auto color = context.Design().GetTheme().colors.accent;
                context.DrawSelection({hp.x - 2, hp.y - 2, hp.width + 4, hp.height + 4}, false, {color, color, 0.06f, 0, 0, false});
            }
            const char* hpLabel = showName && nameInside ? name.c_str() : (recipient ? options.label : this->settings_.show_label) ? "HP" : "";
            context.DrawMeter(hp, hpLabel, hpValue, valid ? module_render_context::Ratio(view.hp, 100) : 0,
                recipient && !options.follow_fill ? options.fill_top : this->settings_.hp_color_top,
                recipient && !options.follow_fill ? options.fill_bottom : this->settings_.hp_color,
                meterStyle, animation, true, showName && nameInside ? &nameText : &profiles[2], &profiles[3],
                nameInside ? ui::meter_text_anchor::center : ui::meter_text_anchor::bottom);
            bottom = hp.y + hp.height + std::max(16.0f, context.Design().TextHeight(profiles[3].style)) * 0.5f + 4;
        }
        if (showName && nameBelow)
        {
            const ui::rect at{panel.x + 7, bottom + 2, panel.width - 14, std::max(16.0f, context.Design().TextHeight(nameText.style))};
            context.DrawText(name.c_str(), at, at, nameText);
            bottom = at.y + at.height + 2;
        }
        if ((!recipient || options.cast) && view.cast.Active(now))
        {
            auto castStyle = meterStyle;
            castStyle.height = recipient ? options.cast_height : 8;
            const ui::rect cast{panel.x + 7, bottom + 3, panel.width - 14, castStyle.height + 2};
            const auto color = context.Design().GetTheme().colors.accent;
            context.DrawBar(cast, view.cast.Ratio(now), MeterGradientTop(color), color, 0, castStyle);
            auto text = profiles[3]; text.style.alignment = ui::text_alignment::center;
            const ui::rect line{cast.x, cast.y + cast.height + 2, cast.width, std::max(16.0f, context.Design().TextHeight(text.style))};
            context.DrawText(view.cast.interrupted ? "Interrupted" : view.cast.name.c_str(), line, line, text);
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
        const auto statuses = std::span<const combat_status>{view.statuses}.first(!recipient || options.statuses ? view.status_count : 0);
        ui::DrawStatusStrip(statuses, {panel.x + 7, bottom + 2, panel.width - 14, 0}, now, profiles[1], context);
        if (showName && !nameInside && !nameBelow)
            context.DrawText(name.c_str(), {panel.x + 7, panel.y + 5, nameWidth, 20}, panel, nameText);
        if (showDistance && valid)
        {
            char distance[32]{};
            std::snprintf(distance, sizeof(distance), "%.1f yalms", view.distance);
            context.DrawText(distance, {panel.x + panel.width * 0.6f, panel.y + 5, panel.width * 0.38f - 7, 20}, panel, profiles[1]);
        }
        return bottom - panel.y + ui::StatusStripHeight(statuses.size(), panel.width - 14);
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
        target_view preview{};
        if (context.IsEditing() && this->views_[2].id == 0)
        {
            preview.id = 0xFFFFFFFF;
            preview.name = "Party Member";
            preview.hp = 84; preview.health = true; preview.distance = 8.2f;
            preview.color = context.Design().GetTheme().colors.text;
        }
        for (const size_t slot : {size_t{2}, size_t{1}})
        {
            const bool recipient = slot == 2;
            if (recipient && !this->settings_.recipient.enabled) continue;
            const auto& view = recipient && this->views_[slot].id == 0 ? preview : this->views_[slot];
            if (view.id == 0) continue;
            const float mainBarHeight = this->settings_.hp_style.height + 2;
            const ui::rect area = recipient ? this->settings_.recipient.Area(this->state_.bounds, height, mainBarHeight) :
                ui::rect{this->state_.bounds.x, this->state_.bounds.y + height + 6, this->state_.bounds.width, 0};
            if (recipient && this->settings_.recipient.ShowArrow())
            {
                const auto& options = this->settings_.recipient;
                context.Design().DrawIcon(options.Arrow(this->state_.bounds, mainBarHeight), ui::icon_glyph::target_arrow,
                    options.arrow_color, 0, options.placement == target_of_target_settings::inline_left);
            }
            const auto drawn = this->DrawView(view, area, slot == 1 ? "Subtarget: " : "Target: ", this->detail_animations_[slot], now, context, recipient);
            height = std::max(height, area.y - this->state_.bounds.y + drawn);
        }
    }
} // namespace xiui
