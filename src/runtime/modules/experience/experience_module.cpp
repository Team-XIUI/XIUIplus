#include "experience_module.hpp"
#include "experience_readout_layout.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>

namespace xiui
{
    auto experience_module::ShouldRender(const hud_snapshot&) const -> bool { return this->state_.enabled; }

    auto experience_module::RefreshProgression(void) -> void
    {
        const auto next = SelectProgression(this->input_, static_cast<progression_mode>(this->settings_.mode));
        if (!this->progression_ready_ || next.kind != this->progression_.kind || next.available != this->progression_.available)
        { this->animation_ = {}; this->readout_widths_.fill(0); }
        this->progression_ = next;
    }

    auto experience_module::OnOptionChanged(const size_t index) -> void
    {
        this->info_dirty_ = true; this->next_readouts_ = 0;
        this->readout_widths_.fill(0);
        if (index == 1 && this->settings_.show_current) this->settings_.readouts[static_cast<size_t>(experience_readout::current_required)] = false;
        if (index == 2 && this->settings_.show_percent) this->settings_.readouts[static_cast<size_t>(experience_readout::percent)] = false;
        if (index == 3 && this->settings_.show_remaining) this->settings_.readouts[static_cast<size_t>(experience_readout::remaining)] = false;
        if (index == mode_option) this->RefreshProgression();
        if (index == readout_option) this->settings_.readout_position = this->settings_.order[this->settings_.selected_readout] + 1.0f;
        if (index == readout_order_option)
        {
            const auto selected = static_cast<size_t>(this->settings_.selected_readout);
            const auto before = this->settings_.order[selected];
            const auto after = static_cast<uint8_t>(std::clamp(this->settings_.readout_position - 1, 0.0f, static_cast<float>(experience_readout_count - 1)));
            for (auto& position : this->settings_.order)
                if (after < before && position >= after && position < before) ++position;
                else if (after > before && position > before && position <= after) --position;
            this->settings_.order[selected] = after;
        }
        if (index == pause_option) this->source_.tracker.Pause(this->settings_.paused, ::GetTickCount64());
        if (index == reset_option)
        {
            this->source_.tracker.Reset(::GetTickCount64(), static_cast<uint32_t>(std::time(nullptr)));
            this->goal_alerted_ = this->merit_alerted_ = false;
        }
        if (index == goal_type_option)
        {
            this->settings_.goal_target = this->settings_.goal_type == 2 ? 75.0f : this->settings_.goal_type == 3 || this->settings_.goal_type == 4 ? 10.0f : 10000.0f;
            this->goal_alerted_ = false;
        }
        if (index == goal_target_option) this->goal_alerted_ = false;
    }

    auto experience_module::HandlePacket(IAshitaCore* core, const uint16_t id, const std::span<const uint8_t> data, const uint64_t now) -> void
    {
        this->source_.HandlePacket(core, id, data, now);
    }

    auto experience_module::NeedsSave(const uint64_t now) -> bool
    {
        if (now < this->next_save_ || !this->source_.tracker.Started()) return false;
        this->next_save_ = now + 30000; return true;
    }

    auto experience_module::GoalRemaining(void) const -> std::optional<uint64_t>
    {
        if (!this->progression_ready_ || this->settings_.goal_type == 0) return std::nullopt;
        const auto target = static_cast<uint64_t>(this->settings_.goal_target);
        uint64_t current{};
        switch (this->settings_.goal_type)
        {
        case 1: current = this->source_.tracker.Totals(progression_kind::experience).received; break;
        case 2:
            if (this->input_.exp_needed != LevelRequirement(this->input_.level)) return std::nullopt;
            return LevelDistance(this->input_.level, this->input_.exp, static_cast<uint32_t>(target));
        case 3: current = this->source_.tracker.Record(this->now_).merits; break;
        case 4: if (this->input_.merit_max == 0) return std::nullopt; current = this->input_.merits; break;
        case 5: current = this->input_.exp; break;
        default: return std::nullopt;
        }
        return current >= target ? 0 : target - current;
    }

    auto experience_module::Notify(IAshitaCore* core, const char* message) -> void
    {
        if (this->settings_.mute_paused && this->source_.tracker.Paused()) return;
        if (core != nullptr && core->GetChatManager() != nullptr) core->GetChatManager()->Writef(1, false, "[XIUI EXP] %s", message);
        if (this->settings_.sound)
        {
            if (!this->sound_) this->sound_ = std::make_unique<ready_check_sound>();
            this->sound_->Play(this->settings_.volume);
        }
    }

    auto experience_module::Update(IAshitaCore* core, const uint64_t now) -> void
    {
        this->now_ = now;
        if (this->sound_) this->sound_->Tick();
        this->source_.Update(core, now);
        this->progression_ready_ = this->source_.ready;
        this->settings_.paused = this->source_.tracker.Paused();
        if (!this->progression_ready_) return;
        this->input_ = this->source_.tracker.Sample().progress; this->RefreshProgression();
        const auto& chain = this->source_.tracker.Chain();
        if (chain.Remaining(now) != 0 && chain.Remaining(now) <= static_cast<uint32_t>(this->settings_.chain_warning) && chain.deadline != this->warned_chain_deadline_)
        {
            this->warned_chain_deadline_ = chain.deadline;
            if (this->settings_.alert_chain) this->Notify(core, chain.estimated ? "Estimated EXP chain window ending." : "EXP chain window ending.");
        }
        const bool meritFull = this->input_.merit_max != 0 && this->input_.merits >= this->input_.merit_max;
        if (meritFull && !this->merit_alerted_ && this->settings_.alert_merits) this->Notify(core, "Merit storage full. Spend merits before the next conversion.");
        this->merit_alerted_ = meritFull;
        const auto goal = this->GoalRemaining();
        if (goal && *goal == 0 && !this->goal_alerted_ && this->settings_.alert_goal) this->Notify(core, "Goal reached.");
        this->goal_alerted_ = goal && *goal == 0;
        if (this->source_.food_ended && this->settings_.alert_food) this->Notify(core, "Food effect ended.");
        if (this->source_.dedication_ended && this->settings_.alert_dedication) this->Notify(core, "Dedication ended.");
        this->source_.food_ended = this->source_.dedication_ended = false;
    }

    auto experience_module::HandleCommand(IAshitaCore* core, const std::string_view command) -> bool
    {
        if (command == "pause" || command == "resume")
        {
            this->settings_.paused = command == "pause"; this->OnOptionChanged(pause_option);
            if (core && core->GetChatManager()) core->GetChatManager()->Writef(1, false, "[XIUI EXP] Session %s.", this->settings_.paused ? "paused" : "resumed");
        }
        else if (command == "reset")
        {
            this->OnOptionChanged(reset_option);
            if (core && core->GetChatManager()) core->GetChatManager()->Write(1, false, "[XIUI EXP] Session saved and reset; active timers retained.");
        }
        else if (command == "history")
        {
            if (core && core->GetChatManager())
            {
                const auto& records = this->source_.tracker.History();
                if (records.empty()) core->GetChatManager()->Write(1, false, "[XIUI EXP] No completed sessions yet.");
                for (size_t i = 0; i < records.size(); ++i)
                {
                    const auto& r = records[i];
                    core->GetChatManager()->Writef(1, false, "[XIUI EXP] %zu. Job %u; %llum; EXP %llu received, %lld retained, %llu lost, %llu recovered; LP %llu; merits %u; best chain %u.",
                        i + 1, r.job, r.elapsed / 60000, r.totals[0].received, r.totals[0].Net(), r.totals[0].lost, r.totals[0].recovered, r.totals[1].received, r.merits, r.best_chain);
                }
            }
        }
        else if (command == "status")
        {
            if (core && core->GetChatManager())
            {
                const auto record = this->source_.tracker.Record(::GetTickCount64());
                core->GetChatManager()->Writef(1, false, "[XIUI EXP] %s; %s %u/%u; %llum active; EXP %llu; LP %llu; merits earned %u; %zu saved sessions; server status %s.",
                    this->source_.tracker.Paused() ? "paused" : this->source_.ready ? "tracking" : "waiting", this->progression_.Label(), this->progression_.current, this->progression_.needed,
                    record.elapsed / 60000, record.totals[0].received, record.totals[1].received, record.merits, this->source_.tracker.History().size(), this->source_.sync_penalty ? "available" : "unavailable");
            }
        }
        else if (command.starts_with("mode "))
        {
            const auto mode = command.substr(5);
            if (mode == "auto") this->settings_.mode = 0;
            else if (mode == "exp") this->settings_.mode = 1;
            else if (mode == "lp" || mode == "merits") this->settings_.mode = 2;
            else if (mode == "cp") this->settings_.mode = 3;
            else if (mode == "ep") this->settings_.mode = 4;
            else return false;
            this->OnOptionChanged(mode_option);
        }
        else return false;
        return true;
    }

    auto experience_module::ReadoutWidth(const experience_readout_text& readout, const float measured, const float scale) -> float
    {
        const auto index = static_cast<size_t>(readout.id);
        if (!this->settings_.stable_numbers || index >= this->readout_widths_.size()) return measured;
        auto& width = this->readout_widths_[index];
        width = std::max(width, measured / std::max(0.01f, scale));
        return width * scale;
    }

    auto experience_module::Render(const hud_snapshot& snapshot, const module_render_context& context) -> void
    {
        const auto& panel = this->state_.bounds;
        if (this->settings_.style.height <= 0) this->settings_.style.height = std::clamp(panel.height - 39, 16.0f, 80.0f);
        const progression_input fallback{0, 0, snapshot.exp_current, snapshot.exp_needed};
        const auto progress = this->progression_ready_ ? this->progression_ : SelectProgression(fallback, static_cast<progression_mode>(this->settings_.mode));
        const auto flags = static_cast<uint8_t>((this->settings_.show_current ? 1 : 0) | (this->settings_.show_percent ? 2 : 0) |
            (this->settings_.show_remaining ? 4 : 0) | (this->settings_.show_points ? 8 : 0) | (this->settings_.show_master_level ? 16 : 0));
        if (this->info_dirty_ || progress != this->displayed_progression_ || this->info_flags_ != flags)
        {
            this->info_text_ = FormatProgressionInfo(progress, this->settings_.show_current, this->settings_.show_percent, this->settings_.show_remaining,
                this->settings_.show_points, this->settings_.show_master_level);
            this->displayed_progression_ = progress; this->info_dirty_ = false; this->info_flags_ = flags;
        }
        const float meterHeight = this->settings_.style.height + 2;
        const bool below = this->settings_.info_position == 1;
        const float infoHeight = std::max(16.0f, context.Design().TextHeight(this->settings_.details_text.style));
        const float labelHalfHeight = this->settings_.show_label ? std::max(16.0f, context.Design().TextHeight(this->settings_.header_text.style)) * 0.5f : 0;
        const bool hasInfo = !this->info_text_.empty();
        const float meterY = panel.y + 4 + std::max(labelHalfHeight, !below && hasInfo ? infoHeight * 0.5f : 0);
        const ui::rect meter{panel.x + 7, meterY, panel.width - 14, meterHeight};
        auto header = this->settings_.header_text, details = this->settings_.details_text;
        header.style.tabular_numbers = details.style.tabular_numbers = this->settings_.stable_numbers;
        context.DrawMeter(meter, this->settings_.show_label ? progress.Label() : "", this->info_text_.c_str(),
            progress.available ? module_render_context::Ratio(progress.current, progress.needed) : 0, this->settings_.color_top, this->settings_.color,
            this->settings_.style, this->animation_, true, &header, &details,
            ui::meter_text_anchor::top, below ? ui::meter_text_anchor::bottom : ui::meter_text_anchor::top);
        const auto now = this->now_ != 0 ? this->now_ : ::GetTickCount64();
        if (now >= this->next_readouts_)
        { this->BuildReadouts(progress, context.IsEditing() && !this->source_.tracker.Started()); this->next_readouts_ = now + 250; }
        const float rowHeight = std::max(16.0f, context.Design().TextHeight(this->settings_.readout_text.style));
        auto profile = this->settings_.readout_text; profile.style.alignment = ui::text_alignment::left;
        profile.style.tabular_numbers = this->settings_.stable_numbers;
        const float gap = this->settings_.divider == 0 ? 12.0f : 8.0f;
        const char* separator = this->settings_.divider == 1 ? "|" : this->settings_.divider == 2 ? "/" : this->settings_.divider == 3 ? "." : "";
        const float separatorWidth = *separator ? context.Design().MeasureText(separator, profile.style) + gap * 2 : gap;
        std::array<float, experience_readout_count> widths{};
        for (size_t i = 0; i < this->readout_values_.size(); ++i)
            widths[i] = this->ReadoutWidth(this->readout_values_[i], context.Design().MeasureText(this->readout_values_[i].text.c_str(), profile.style), profile.style.scale);
        const auto flow = LayoutExperienceReadouts({widths.data(), this->readout_values_.size()}, meter.width, rowHeight,
            this->settings_.row_spacing, static_cast<size_t>(this->settings_.readout_rows), separatorWidth);
        const auto area = this->ReadoutArea(context.Design(), meter, hasInfo, flow.height);
        const float alignment = profile.placement == 2 ? 0.5f : profile.placement == 3 ? 1.0f : 0;
        // Offsets and alignment position the block once, preserving each text profile's effects.
        profile.offset_x = profile.offset_y = 0; profile.placement = 0;
        for (size_t i = 0; i < flow.count; ++i)
        {
            const auto& value = this->readout_values_[i];
            const auto& item = flow.items[i];
            auto bounds = item.bounds;
            bounds.x += area.x + (area.width - flow.row_widths[item.row]) * alignment;
            bounds.y += area.y;
            if (item.divider && *separator)
            {
                const ui::rect divider{bounds.x - separatorWidth + gap, bounds.y, separatorWidth - gap * 2, rowHeight};
                context.DrawText(separator, divider, divider, profile);
            }
            auto fitted = value.warning ? TintTextProfile(profile, this->settings_.warning_color) : profile;
            if (this->settings_.stable_numbers && this->settings_.label_position == 1) fitted.style.alignment = ui::text_alignment::right;
            fitted.style.scale *= item.scale;
            context.DrawText(value.text.c_str(), bounds, bounds, fitted);
        }
        if (flow.overflow)
        {
            auto bounds = flow.ellipsis; bounds.x += area.x; bounds.y += area.y;
            context.DrawText("...", bounds, bounds, profile);
        }
        const float barBottom = meter.y + meter.height + (below && hasInfo ? infoHeight * 0.5f + std::max(0.0f, this->settings_.details_text.offset_y) : 0);
        const float bottom = std::max(barBottom, flow.rows != 0 ? area.y + area.height : barBottom) + 6;
        const float top = std::min(panel.y, flow.rows != 0 ? area.y : panel.y);
        const float left = std::min(panel.x, flow.rows != 0 ? area.x : panel.x);
        const float right = std::max(panel.x + panel.width, flow.rows != 0 ? area.x + area.width : panel.x + panel.width);
        this->state_.minimum_height = std::max(62.0f, bottom - panel.y);
        this->state_.bounds.height = std::max(this->state_.bounds.height, this->state_.minimum_height);
        this->state_.content_bounds = {left - panel.x, top - panel.y, right - left, bottom - top};
    }

    auto experience_module::ReadoutTop(const ui::design_system& design, const ui::rect& meter, const bool hasInfo, const float blockHeight) const -> float
    {
        if (this->settings_.readout_anchor == 2) return meter.y + (meter.height - blockHeight) * 0.5f;
        float bottom = meter.y + meter.height;
        float top = meter.y;
        if (hasInfo)
        {
            const float anchor = this->settings_.info_position == 1 ? meter.y + meter.height : meter.y;
            const float halfHeight = std::max(16.0f, design.TextHeight(this->settings_.details_text.style)) * 0.5f;
            bottom = std::max(bottom, anchor + halfHeight + this->settings_.details_text.offset_y);
            top = std::min(top, anchor - halfHeight + this->settings_.details_text.offset_y);
        }
        if (this->settings_.show_label)
        {
            bottom = std::max(bottom, meter.y + std::max(16.0f, design.TextHeight(this->settings_.header_text.style)) * 0.5f + this->settings_.header_text.offset_y);
            top = std::min(top, meter.y - std::max(16.0f, design.TextHeight(this->settings_.header_text.style)) * 0.5f + this->settings_.header_text.offset_y);
        }
        return this->settings_.readout_anchor == 1 ? top - this->settings_.bar_gap - blockHeight : bottom + this->settings_.bar_gap;
    }

    auto experience_module::ReadoutArea(const ui::design_system& design, const ui::rect& meter, const bool hasInfo, const float blockHeight) const -> ui::rect
    {
        return {meter.x + this->settings_.readout_text.offset_x, this->ReadoutTop(design, meter, hasInfo, blockHeight) + this->settings_.readout_text.offset_y, meter.width, blockHeight};
    }
}
