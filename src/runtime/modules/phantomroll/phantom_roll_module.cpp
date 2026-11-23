#include "localization/localization.hpp"
#include "ashita_compat.hpp"
#include "phantom_roll_module.hpp"
#include "combat/server_rules.hpp"
#include "phantom_roll_data.hpp"
#include "modules/bar_config.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
    constexpr const char* profiles[]{"name", "state", "potency", "timer", "die"};
    auto Remaining(const uint64_t expiry, const uint64_t now, const bool estimated = false) -> std::string
    {
        if (expiry == 0) return "Unknown";
        const auto seconds = expiry > now ? (expiry - now + 999) / 1000 : 0;
        char text[32]{}; std::snprintf(text, sizeof(text), "%s%llu:%02llu", estimated ? "~" : "", seconds / 60, seconds % 60);
        return text;
    }
}

namespace xiui
{
    auto phantom_roll_module::ApplyTextDefaults(const ui::theme& theme) -> void
    {
        this->text_ = {module_text_profile{theme.title_text}, module_text_profile{theme.caption_text}, module_text_profile{theme.caption_text},
            module_text_profile{theme.meter_text}, module_text_profile{theme.title_text}};
        this->text_[0].style.scale = 0.9f; this->text_[1].style.scale = this->text_[2].style.scale = 0.8f;
        this->text_[3].style.scale = 0.8f; this->text_[4].style.scale = 1.6f;
        this->text_[4].style.alignment = ui::text_alignment::center;
    }

    auto phantom_roll_module::ResetDefaults(const ui::theme& theme) -> void
    {
        const auto bounds = this->state_.bounds;
        const auto* source = this->source_;
        auto* settings = this->combat_settings_;
        *this = {};
        this->state_.bounds = bounds;
        this->source_ = source;
        this->combat_settings_ = settings;
        this->ApplyTextDefaults(theme);
        this->ApplyMeterDefaults(theme.meter);
    }

    auto phantom_roll_module::Update(const uint64_t now, const bool configOpen) -> void
    {
        this->now_ = now;
        const auto* local = this->source_ != nullptr && this->source_->Available() ? &this->source_->Members()[0] : nullptr;
        const auto id = local != nullptr && local->active && local->in_zone ? local->id : 0;
        const auto zone = id != 0 ? local->zone : 0;
        const auto job = id != 0 ? local->main_job : 0, subjob = id != 0 ? local->sub_job : 0;
        if (id != this->player_id_ || zone != this->zone_ || job != this->job_ || subjob != this->subjob_)
        {
            this->Clear(); this->animation_ = {}; this->player_id_ = id; this->zone_ = zone;
            this->job_ = static_cast<uint8_t>(job); this->subjob_ = static_cast<uint8_t>(subjob);
        }
        this->level_ = id != 0 ? local->main_level : 0;
        // A subjob Corsair rolls at its subjob level.
        this->roller_level_ = id != 0 && job != 17 && subjob == 17 ? local->sub_level : 0;
        if (!configOpen) this->preview_ = false;
        if (id == 0 || !local->buffs_known) return;
        std::array<phantom_roll_status, 32> statuses{};
        const auto count = std::min(local->buff_count, statuses.size());
        for (size_t i = 0; i < count; ++i) statuses[i] = {local->buffs[i], local->buff_expires[i]};
        std::array<phantom_roll_member, 5> party{};
        const auto& members = this->source_->Members();
        size_t partyCount{};
        for (size_t slot = 1; slot <= party.size(); ++slot)
        {
            const auto& member = members[slot];
            if (!member.active) continue;
            party[partyCount++] = {member.id, member.in_zone && member.buffs_known, {member.buffs.data(), std::min(member.buff_count, member.buffs.size())}};
        }
        this->tracker_.Reconcile({statuses.data(), count}, now, {party.data(), partyCount});
    }

    auto phantom_roll_module::HandlePacket(IAshitaCore* core, const uint16_t id, const std::span<const uint8_t> data, const uint64_t now, const combat_action_packet* decoded) -> void
    {
        const combat_action_packet local{data};

        if (id == 0xA || id == 0xB) { this->Clear(); return; }
        if (id != 0x28 || this->player_id_ == 0) return;
        auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        auto player = memory != nullptr ? xiui::ashita::View(memory->GetPlayer()) : nullptr;
        auto party = memory != nullptr ? xiui::ashita::View(memory->GetParty()) : nullptr;
        if (player == nullptr || party == nullptr ||
            player->GetLoginStatus() != static_cast<uint8_t>(Ashita::FFXI::Enums::LoginStatus::LoggedIn) ||
            party->GetMemberServerId(0) != this->player_id_ || party->GetMemberZone(0) != this->zone_ ||
            player->GetMainJob() != this->job_ || player->GetSubJob() != this->subjob_) return;
        if (const auto& action = (decoded != nullptr ? *decoded : local).Action()) this->tracker_.Observe(*action, this->player_id_, now, this->PartyIds());
    }

    auto phantom_roll_module::Load(const settings_store& settings) -> void
    {
        this->Clear(); this->animation_ = {};
        this->state_.bounds = settings.ReadRect("layout.phantomroll", this->state_.bounds);
        if (this->combat_settings_ == nullptr)
            this->local_combat_settings_.SetRules(ServerRulesFromIndex(settings.ReadInt("phantomroll", "rules", 0)));
        this->layout_ = std::clamp(settings.ReadInt("phantomroll", "layout", 0), 0, 1);
        this->selected_text_ = std::clamp(settings.ReadInt("phantomroll", "selected_text", 0), 0, 4);
        this->state_.enabled = settings.ReadBool("phantomroll", "enabled", false);
        for (const auto& [key, value] : {std::pair{"container", &this->container_}, {"potency", &this->potency_}, {"double_up", &this->double_up_}, {"party", &this->party_}})
            *value = settings.ReadBool("phantomroll", key, *value);
        const auto number = [&](const char* key, const float fallback, const float minimum, const float maximum) {
            const auto value = settings.ReadFloat("phantomroll", key, fallback);
            return std::clamp(std::isfinite(value) ? value : fallback, minimum, maximum);
        };
        this->scale_ = number("scale", 1, 0.6f, 1.8f);
        this->container_style_.opacity = number("opacity", 0.95f, 0, 1); this->container_style_.padding = number("padding", 10, 0, 24);
        for (const auto& [key, value] : {std::pair{"lucky", &this->lucky_}, {"unlucky", &this->unlucky_}, {"bust", &this->bust_}, {"fill_top", &this->top_}, {"fill_bottom", &this->bottom_}})
            *value = settings.ReadArgb("phantomroll", key, *value);
        this->meter_ = LoadMeterStyle(settings, "meter.phantomroll", this->meter_);
        for (size_t i = 0; i < this->text_.size(); ++i)
            this->text_[i] = LoadTextProfile(settings, (std::string{"text.phantomroll."} + profiles[i]).c_str(), this->text_[i]);
    }

    auto phantom_roll_module::Save(const settings_store& settings) const -> void
    {
        settings.WriteRect("layout.phantomroll", this->state_.bounds);
        if (this->combat_settings_ == nullptr)
            settings.WriteInt("phantomroll", "rules", ServerRulesIndex(this->Rules()));
        settings.WriteInt("phantomroll", "layout", this->layout_);
        settings.WriteInt("phantomroll", "selected_text", this->selected_text_);
        for (const auto& [key, value] : {std::pair{"enabled", this->state_.enabled}, {"container", this->container_}, {"potency", this->potency_}, {"double_up", this->double_up_}, {"party", this->party_}})
            settings.WriteBool("phantomroll", key, value);
        settings.WriteFloat("phantomroll", "scale", this->scale_); settings.WriteFloat("phantomroll", "opacity", this->container_style_.opacity);
        settings.WriteFloat("phantomroll", "padding", this->container_style_.padding);
        for (const auto& [key, value] : {std::pair{"lucky", this->lucky_}, {"unlucky", this->unlucky_}, {"bust", this->bust_}, {"fill_top", this->top_}, {"fill_bottom", this->bottom_}})
            settings.WriteArgb("phantomroll", key, value);
        SaveMeterStyle(settings, "meter.phantomroll", this->meter_);
        for (size_t i = 0; i < this->text_.size(); ++i)
            SaveTextProfile(settings, (std::string{"text.phantomroll."} + profiles[i]).c_str(), this->text_[i]);
    }

    auto phantom_roll_module::ConfigSections(void) const -> std::span<const module_config_section> { return BarConfigSections(); }

    auto phantom_roll_module::ContentConfigSectionContains(const size_t section, const size_t index) -> bool
    {
        if (section >= this->ConfigSections().size() || index >= this->OptionCount()) return false;
        const auto topic = static_cast<bar_config_section>(section);
        if (index >= base_options && index < base_options + meter_style_option_count) return topic == MeterConfigSection(index - base_options);
        if (index >= base_options + meter_style_option_count || index == selected_text) return topic == bar_config_section::text;
        if (index >= layout && index <= padding) return topic == bar_config_section::layout;
        if (index >= lucky_color && index <= fill_bottom) return topic == bar_config_section::colors;
        return topic == bar_config_section::general;
    }

    auto phantom_roll_module::ContentOption(const size_t index) -> module_option
    {
        static constexpr const char* layouts[]{"Side by side", "Stacked"};
        static constexpr const char* textNames[]{"Roll name", "Roll state", "Base potency", "Timer", "Die total"};
        switch (index)
        {
        case enabled: return module_option::Toggle("Enabled", "Show your two roll effects and your party's other rolls; tracking continues while hidden.", &this->state_.enabled).Ordered(-50);
        case rules: return module_option::Choice("Rules", "Shared with Action Bars / Skillchains. Base potency includes party job bonuses (none on Horizon); equipment and special ability modifiers remain unknown.", this->CombatSettings().RulesChoice(), server_rule_names, 3);
        case preview: return module_option::Action("Preview rolls", "Show separate samples until configuration closes; never changes live rolls.", "Preview");
        case layout: return module_option::Choice("Roll layout", "Arrange the two seats horizontally or vertically.", &this->layout_, layouts, 2);
        case width: return ModuleWidthOption(this->state_);
        case scale: return module_option::Slider("Content scale", "Scale dice, text, bars and spacing together.", &this->scale_, 0.6f, 1.8f, 0.05f);
        case container: return module_option::Toggle("Container", "Use the shared native Window Type container.", &this->container_);
        case opacity: return module_option::Slider("Container opacity", "Opacity of the native frame and backing.", &this->container_style_.opacity, 0, 1, 0.05f);
        case padding: return module_option::Slider("Container padding", "Space inside the native frame.", &this->container_style_.padding, 0, 24, 1);
        case potency: return module_option::Toggle("Base potency", "Show the base value, including the job bonus when a party member has the roll's job. Equipment modifiers and bust penalties remain unknown.", &this->potency_);
        case double_up: return module_option::Toggle("Double-Up window", "Native countdown and ordinary six-face die bust chance; special ability modifiers are not included.", &this->double_up_);
        case party_rolls: return module_option::Toggle("Party rolls", "List rolls your party holds that you do not, such as a second Corsair's or ones you rolled before moving away. Timers keep running while members are out of sight; ~ marks an estimate.", &this->party_);
        case lucky_color: return module_option::Color("Lucky / eleven", "Lucky and eleven state tint, retaining text effects.", &this->lucky_);
        case unlucky_color: return module_option::Color("Unlucky", "Unlucky state tint.", &this->unlucky_);
        case bust_color: return module_option::Color("Bust", "Bust state tint.", &this->bust_);
        case fill_top: return module_option::Color("Duration top", "Top of the remaining-duration gradient.", &this->top_);
        case fill_bottom: return module_option::Color("Duration bottom", "Bottom of the remaining-duration gradient.", &this->bottom_);
        case selected_text: return module_option::Element("Selected text", "Style roll labels, state, base potency, timer and die total.", &this->selected_text_, textNames, 5);
        default:
            if (index < base_options + meter_style_option_count) return MeterStyleOption(index - base_options, this->meter_);
            return TextProfileOption(index - base_options - meter_style_option_count, this->text_[this->selected_text_]);
        }
    }

    auto phantom_roll_module::OnContentOptionChanged(const size_t index) -> void
    { if (index == preview) this->preview_ = !this->preview_; }

    // Rolls reach only the roller's party, so its main jobs decide the job bonus. A solo roller has no party and no bonus.
    auto phantom_roll_module::PartyJobs(void) const -> std::array<uint8_t, 6>
    {
        std::array<uint8_t, 6> jobs{};
        if (this->source_ == nullptr || !this->source_->Available()) return jobs;
        const auto& members = this->source_->Members();
        if (std::none_of(members.begin() + 1, members.begin() + jobs.size(), [](const auto& member) { return member.active; })) return jobs;
        for (size_t slot = 0; slot < jobs.size(); ++slot)
            if (members[slot].active) jobs[slot] = members[slot].main_job;
        return jobs;
    }

    // Empty slots stay zero, which matches no actor or target.
    auto phantom_roll_module::PartyIds(void) const -> std::array<uint32_t, 5>
    {
        std::array<uint32_t, 5> ids{};
        if (this->source_ == nullptr || !this->source_->Available()) return ids;
        const auto& members = this->source_->Members();
        for (size_t slot = 0; slot < ids.size(); ++slot)
            if (members[slot + 1].active) ids[slot] = members[slot + 1].id;
        return ids;
    }

    auto phantom_roll_module::StatusLines(void) const -> std::vector<std::string>
    {
        std::vector<std::string> lines;
        const auto jobs = this->PartyJobs();
        for (const auto& seat : this->tracker_.Seats())
        {
            if (!seat.Present()) continue;
            const auto* roll = PhantomRollByAbility(seat.ability, this->Rules());
            const bool bonus = roll != nullptr && PhantomRollHasJobBonus(*roll, jobs);
            lines.push_back((roll != nullptr ? std::string{roll->name} + " Roll" : std::string{"Unknown roll"}) + " total " + std::to_string(seat.total) +
                "; " + PhantomRollPotencyText(roll, seat.total, this->level_, seat.busted, bonus, this->roller_level_));
        }
        for (const auto& entry : this->tracker_.PartyRolls())
        {
            if (!this->tracker_.PartyVisible(entry)) continue;
            const auto* roll = PhantomRollByAbility(entry.ability, this->Rules());
            lines.push_back("Party " + (roll != nullptr ? std::string{roll->name} + " Roll" : std::string{"Unknown roll"}) + " total " +
                (entry.total != 0 ? std::to_string(entry.total) : std::string{"unknown"}) + "; remaining " + Remaining(entry.expires, this->now_, entry.estimated) +
                "; " + std::to_string(entry.holders) + " members");
        }
        return lines;
    }

    auto phantom_roll_module::ShouldRender(const hud_snapshot&) const -> bool
    {
        if (!this->state_.enabled) return false;
        for (const auto& seat : this->tracker_.Seats()) if (seat.Present()) return true;
        if (this->party_) for (const auto& entry : this->tracker_.PartyRolls()) if (this->tracker_.PartyVisible(entry)) return true;
        return this->preview_;
    }

    auto phantom_roll_module::Render(const hud_snapshot&, const module_render_context& context) -> void
    {
        const bool sample = context.IsEditing() || this->preview_;
        const std::array<phantom_roll_seat, 2> samples{{{108, 320, 4, false, true, this->now_ + 268000, 300000, 0, 1},
            {105, 317, 8, false, true, this->now_ + 154000, 300000, 0, 2}}};
        const auto& seats = sample ? samples : this->tracker_.Seats();
        const auto partyJobs = sample ? std::array<uint8_t, 6>{} : this->PartyJobs();
        auto text = this->text_;
        for (auto& profile : text) profile.style.scale *= this->scale_;
        const auto& design = context.Design();
        const auto rowHeight = [&](const size_t index) { return std::max(16.0f * this->scale_, design.TextHeight(text[index].style) + 2 * this->scale_); };
        const auto gap = 8 * this->scale_, dieSize = 48 * this->scale_;
        auto meter = this->meter_; meter.height *= this->scale_;
        const auto meterHeight = std::max(meter.height, rowHeight(3)) + 4 * this->scale_;
        const auto seatHeight = rowHeight(0) + 4 * this->scale_ + std::max(dieSize, rowHeight(1) * 2) +
            (this->potency_ ? rowHeight(2) + 4 * this->scale_ : 0) + meterHeight + 8 * this->scale_;
        std::array<phantom_roll_party, 4> partyRolls{};
        size_t partyCount{};
        if (this->party_ && sample)
        {
            partyRolls[partyCount++] = {98, 310, 5, 3, false, false, false, 1, this->now_ + 201000, 300000, 0};
            partyRolls[partyCount++] = {109, 321, 9, 3, true, false, false, 1, this->now_ + 97000, 300000, 0};
        }
        else if (this->party_)
            for (const auto& entry : this->tracker_.PartyRolls()) if (this->tracker_.PartyVisible(entry)) partyRolls[partyCount++] = entry;
        const auto smallDie = 22 * this->scale_;
        const auto partyRow = std::max({smallDie, rowHeight(0), rowHeight(3)}) + 4 * this->scale_;
        const size_t partyColumns = this->layout_ == 0 ? 2 : 1;
        const auto partyHeight = partyCount != 0 ? gap + rowHeight(1) + partyRow * ((partyCount + partyColumns - 1) / partyColumns) : 0.0f;
        const auto seatsHeight = seatHeight * (this->layout_ == 0 ? 1 : 2) + (this->layout_ == 0 ? 0 : gap);
        const auto until = sample ? this->now_ + 32000 : this->tracker_.DoubleUpUntil();
        const bool showWindow = this->double_up_ && until > this->now_;
        const float inset = this->container_ ? this->container_style_.edge_size + this->container_style_.padding : 0;
        this->state_.bounds.height = inset * 2 + seatsHeight + partyHeight +
            (showWindow ? rowHeight(1) + meterHeight + gap : 0);
        const auto content = this->container_ ? context.DrawContainer(this->state_.bounds, this->container_style_) : this->state_.bounds;
        const auto columnWidth = this->layout_ == 0 ? (content.width - gap) / 2 : content.width;
        const auto drawLine = [&](const std::string& value, const ui::rect& row, const size_t profile, const D3DCOLOR* tint = nullptr) {
            context.DrawText(value.c_str(), row, row, tint != nullptr ? TintTextProfile(text[profile], *tint) : text[profile]);
        };
        for (size_t i = 0; i < seats.size(); ++i)
        {
            const auto& seat = seats[i];
            const auto* roll = PhantomRollByAbility(seat.ability, this->Rules());
            const auto x = content.x + (this->layout_ == 0 ? i * (columnWidth + gap) : 0);
            auto y = content.y + (this->layout_ == 1 ? i * (seatHeight + gap) : 0);
            drawLine(seat.Present() ? roll != nullptr ? std::string{roll->name} + " Roll" : "Bust" : "No roll", {x, y, columnWidth, rowHeight(0)}, 0);
            y += rowHeight(0) + 4 * this->scale_;
            if (!seat.Present()) continue;
            const bool lucky = roll != nullptr && (seat.total == 11 || seat.total == roll->lucky);
            const bool unlucky = roll != nullptr && seat.total == roll->unlucky;
            const auto color = seat.busted ? this->bust_ : lucky ? this->lucky_ : unlucky ? this->unlucky_ : design.GetTheme().colors.text;
            const ui::rect die{x, y, dieSize, dieSize};
            design.FillRoundedRect(die, 0xD01A1A1A, 6 * this->scale_);
            design.FillRoundedRect({x + 2, y + 2, dieSize - 4, dieSize - 4}, (color & 0xFFFFFF) | 0x50000000, 5 * this->scale_);
            drawLine(seat.busted ? "B" : seat.total == 0 ? "?" : std::to_string(seat.total), die, 4, &color);
            const auto state = seat.busted ? "Bust" : seat.total == 0 ? "Total unknown" : seat.total == 11 ? "Eleven" : lucky ? "Lucky" : unlucky ? "Unlucky" : "Normal";
            drawLine(state, {x + dieSize + gap, y, columnWidth - dieSize - gap, rowHeight(1)}, 1, &color);
            if (roll != nullptr) drawLine("Lucky " + std::to_string(roll->lucky) + " / Unlucky " + std::to_string(roll->unlucky),
                {x + dieSize + gap, y + rowHeight(1), columnWidth - dieSize - gap, rowHeight(1)}, 1);
            y += std::max(dieSize, rowHeight(1) * 2) + 4 * this->scale_;
            if (this->potency_)
            {
                const bool jobBonus = roll != nullptr && PhantomRollHasJobBonus(*roll, partyJobs);
                drawLine(PhantomRollPotencyText(roll, seat.total, this->level_, seat.busted, jobBonus, sample ? uint8_t{} : this->roller_level_), {x, y, columnWidth, rowHeight(2)}, 2);
                y += rowHeight(2) + 4 * this->scale_;
            }
            const auto ratio = seat.expires > this->now_ && seat.duration > 0 ? std::clamp(static_cast<float>(seat.expires - this->now_) / seat.duration, 0.0f, 1.0f) : 0;
            context.DrawMeter({x, y + meterHeight / 2, columnWidth, meter.height}, i18n::Text("Remaining"), Remaining(seat.expires, this->now_, !seat.observed).c_str(),
                ratio, this->top_, this->bottom_, meter, this->animation_[i], true, &text[3], &text[3], ui::meter_text_anchor::center);
        }
        if (partyCount != 0)
        {
            auto y = content.y + seatsHeight + gap;
            drawLine(i18n::Text("Party rolls"), {content.x, y, content.width, rowHeight(1)}, 1);
            y += rowHeight(1);
            auto dieText = text[4], timerText = text[3];
            dieText.style.scale *= smallDie / dieSize;
            timerText.style.alignment = ui::text_alignment::right;
            const auto timerWidth = std::min(columnWidth * 0.4f, 64 * this->scale_);
            for (size_t i = 0; i < partyCount; ++i)
            {
                const auto& entry = partyRolls[i];
                const auto* roll = PhantomRollByAbility(entry.ability, this->Rules());
                const auto x = content.x + (i % partyColumns) * (columnWidth + gap), rowY = y + (i / partyColumns) * partyRow;
                const bool lucky = roll != nullptr && (entry.total == 11 || entry.total == roll->lucky);
                const bool unlucky = roll != nullptr && entry.total == roll->unlucky;
                const auto color = lucky ? this->lucky_ : unlucky ? this->unlucky_ : design.GetTheme().colors.text;
                const ui::rect die{x, rowY + (partyRow - smallDie) / 2, smallDie, smallDie};
                design.FillRoundedRect(die, 0xD01A1A1A, 3 * this->scale_);
                design.FillRoundedRect({die.x + 1, die.y + 1, smallDie - 2, smallDie - 2}, (color & 0xFFFFFF) | 0x50000000, 3 * this->scale_);
                context.DrawText(entry.total == 0 ? "?" : std::to_string(entry.total).c_str(), die, die, TintTextProfile(dieText, color));
                const auto nameX = x + smallDie + gap / 2;
                drawLine(roll != nullptr ? std::string{roll->name} + " Roll" : "?", {nameX, rowY, std::max(0.0f, x + columnWidth - timerWidth - nameX), partyRow}, 0);
                const ui::rect timer{x + columnWidth - timerWidth, rowY, timerWidth, partyRow};
                context.DrawText(entry.expires != 0 ? Remaining(entry.expires, this->now_, entry.estimated).c_str() : "--:--", timer, timer, timerText);
            }
        }
        if (showWindow)
        {
            const auto y = content.y + seatsHeight + partyHeight + gap;
            const auto* latest = sample ? &samples[1] : this->tracker_.LatestOpen();
            const auto chance = latest != nullptr && latest->total != 0 ? std::to_string(static_cast<int>(std::round(PhantomBustChance(latest->total) * 100))) + "%" : "unknown";
            drawLine("Base bust chance: " + chance + " (ordinary die)", {content.x, y, content.width, rowHeight(1)}, 1);
            context.DrawMeter({content.x, y + rowHeight(1) + meterHeight / 2, content.width, meter.height}, i18n::Text("Double-Up"), Remaining(until, this->now_).c_str(),
                std::clamp(static_cast<float>(until - this->now_) / 45000, 0.0f, 1.0f), this->top_, this->bottom_, meter, this->animation_[2], true,
                &text[3], &text[3], ui::meter_text_anchor::center);
        }
    }
}
