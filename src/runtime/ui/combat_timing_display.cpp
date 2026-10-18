#include "combat_timing_display.hpp"
#include "modules/bar_config.hpp"
#include "sound_choices.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace xiui::ui
{
    namespace
    {
        auto FiniteFloat(const settings_store& store, const char* section, const char* key, const float fallback) -> float
        {
            const float value = store.ReadFloat(section, key, fallback);
            return std::isfinite(value) ? value : fallback;
        }
    }

    auto player_timing_settings::Load(const settings_store& store) -> void
    {
        constexpr auto section = "timing.player";
        this->attack = store.ReadBool(section, "attack", true); this->resting = store.ReadBool(section, "resting", true);
        this->rounds = store.ReadBool(section, "rounds", true); this->hits = store.ReadBool(section, "hits", false);
        this->tp_time = store.ReadBool(section, "tp_time", true); this->gains = store.ReadBool(section, "gains", true);
        this->recovery_time = store.ReadBool(section, "recovery_time", true); this->hide_full = store.ReadBool(section, "hide_full", true);
        this->flash = store.ReadBool(section, "flash", true); this->resource_times = store.ReadBool(section, "resource_times", false);
        this->tp_goal = std::clamp(store.ReadInt(section, "tp_goal", 0), 0, 2);
        this->sound = std::clamp(store.ReadInt(section, "sound", 0), 0, static_cast<int>(std::size(sound_values)));
        this->display = std::clamp(store.ReadInt(section, "display", 0), 0, 1); this->direction = std::clamp(store.ReadInt(section, "direction", 0), 0, 1);
        this->attack_layout = std::clamp(store.ReadInt(section, "attack_layout", 2), 0, 2);
        this->mini_text = std::clamp(store.ReadInt(section, "mini_text", 0), 0, 2);
        this->mini_height = std::clamp(FiniteFloat(store, section, "mini_height", 3), 1.0f, 12.0f);
        this->mini_gap = std::clamp(FiniteFloat(store, section, "mini_gap", 2), 0.0f, 24.0f);
        this->recovery_layout = std::clamp(store.ReadInt(section, "recovery_layout", 2), 0, 2);
        this->recovery_mini_text = std::clamp(store.ReadInt(section, "recovery_mini_text", 0), 0, 2);
        this->recovery_mini_height = std::clamp(FiniteFloat(store, section, "recovery_mini_height", 3), 1.0f, 12.0f);
        this->recovery_mini_gap = std::clamp(FiniteFloat(store, section, "recovery_mini_gap", 2), 0.0f, 24.0f);
        this->goal = std::clamp(FiniteFloat(store, section, "goal", 100), 1.0f, 100.0f);
        this->height = std::clamp(FiniteFloat(store, section, "height", 6), 2.0f, 24.0f);
        this->offset_x = std::clamp(FiniteFloat(store, section, "offset_x", 0), -1000.0f, 1000.0f);
        this->offset_y = std::clamp(FiniteFloat(store, section, "offset_y", 0), -1000.0f, 1000.0f);
        this->spacing = std::clamp(FiniteFloat(store, section, "spacing", 4), 0.0f, 30.0f);
        this->ready_seconds = std::clamp(FiniteFloat(store, section, "ready_seconds", 0.5f), 0.0f, 2.0f);
        this->attack_color = store.ReadArgb(section, "attack_color", this->attack_color);
        this->rest_color = store.ReadArgb(section, "rest_color", this->rest_color); this->ready_color = store.ReadArgb(section, "ready_color", this->ready_color);
        this->attack_text = LoadTextProfile(store, "text.player.attack", this->attack_text);
        this->rest_text = LoadTextProfile(store, "text.player.recovery", this->rest_text);
    }

    auto player_timing_settings::Save(const settings_store& store) const -> void
    {
        constexpr auto section = "timing.player";
        store.WriteBool(section, "attack", this->attack); store.WriteBool(section, "resting", this->resting);
        store.WriteBool(section, "rounds", this->rounds); store.WriteBool(section, "hits", this->hits);
        store.WriteBool(section, "tp_time", this->tp_time); store.WriteBool(section, "gains", this->gains);
        store.WriteBool(section, "recovery_time", this->recovery_time); store.WriteBool(section, "hide_full", this->hide_full);
        store.WriteBool(section, "flash", this->flash); store.WriteBool(section, "resource_times", this->resource_times);
        store.WriteInt(section, "tp_goal", this->tp_goal); store.WriteInt(section, "sound", this->sound);
        store.WriteInt(section, "display", this->display); store.WriteInt(section, "direction", this->direction);
        store.WriteInt(section, "attack_layout", this->attack_layout); store.WriteInt(section, "mini_text", this->mini_text);
        store.WriteFloat(section, "mini_height", this->mini_height); store.WriteFloat(section, "mini_gap", this->mini_gap);
        store.WriteInt(section, "recovery_layout", this->recovery_layout); store.WriteInt(section, "recovery_mini_text", this->recovery_mini_text);
        store.WriteFloat(section, "recovery_mini_height", this->recovery_mini_height); store.WriteFloat(section, "recovery_mini_gap", this->recovery_mini_gap);
        store.WriteFloat(section, "goal", this->goal); store.WriteFloat(section, "height", this->height);
        store.WriteFloat(section, "offset_x", this->offset_x); store.WriteFloat(section, "offset_y", this->offset_y);
        store.WriteFloat(section, "spacing", this->spacing); store.WriteFloat(section, "ready_seconds", this->ready_seconds);
        store.WriteArgb(section, "attack_color", this->attack_color); store.WriteArgb(section, "rest_color", this->rest_color); store.WriteArgb(section, "ready_color", this->ready_color);
        SaveTextProfile(store, "text.player.attack", this->attack_text); SaveTextProfile(store, "text.player.recovery", this->rest_text);
    }

    auto player_timing_settings::RecoveryOption(const size_t index) -> bool { return (index >= 5 && index <= 10) || index == 18 || index == 19 || index == 21 || (index >= 27 && index < option_count); }

    auto player_timing_settings::Option(const size_t index) -> module_option
    {
        const bool attackDetails = this->attack && (this->attack_layout != 2 || this->mini_text == 2);
        const bool recoveryDetails = this->resting && (this->recovery_layout != 2 || this->recovery_mini_text == 2);
        const bool attackBar = this->attack && (this->attack_layout == 2 || this->display == 0);
        const bool recoveryBar = this->resting && (this->recovery_layout == 2 || this->display == 0);
        const bool fullReadout = (this->attack && this->attack_layout != 2) || (this->resting && this->recovery_layout != 2);
        static constexpr const char* goals[]{"1000", "2000", "3000"}, *displays[]{"Bar + text", "Text only"}, *directions[]{"Fill", "Drain"};
        static constexpr const char* layouts[]{"Below Player Bar", "Above Player Bar", "Mini below TP"};
        static constexpr const char* miniText[]{"None", "Countdown", "Countdown + TP"};
        static constexpr const char* recoveryLayouts[]{"Below Player Bar", "Above Player Bar", "Mini below HP"};
        static constexpr const char* recoveryMiniText[]{"None", "Countdown", "Countdown + Recovery"};
        switch (index)
        {
        case 0: return module_option::Toggle("Auto attack timer", "Predict the next round from weapons, known bonuses and observed attacks. ~ means estimated.", &this->attack).Ordered(-40);
        case 1: return module_option::Choice("TP goal", "Goal for rounds, successful hits and time estimates.", &this->tp_goal, goals, 3).VisibleWhen(attackDetails);
        case 2: return module_option::Toggle("Rounds to goal", "Estimated rounds, including observed TP gains outside your own attacks.", &this->rounds).VisibleWhen(attackDetails);
        case 3: return module_option::Toggle("Successful hits", "Normal successful hits still needed; excludes TP from other sources.", &this->hits).VisibleWhen(attackDetails);
        case 4: return module_option::Toggle("Time to TP goal", "Approximate time at the current attack rate. Hidden while timing is held or unknown.", &this->tp_time).VisibleWhen(attackDetails);
        case 5: return module_option::Toggle("Resting timer", "Watch real recovery ticks and the Phoenix 9 / 9 / 12 second pattern.", &this->resting).Ordered(-40);
        case 6: return module_option::Toggle("Last tick gains", "HP and MP restored by the last observed resting tick.", &this->gains).VisibleWhen(recoveryDetails);
        case 7: return module_option::Toggle("Time to recovery goal", "Estimate from known recovery bonuses, refined by observed uncapped gains.", &this->recovery_time).VisibleWhen(recoveryDetails);
        case 8: return module_option::Toggle("Hide when full", "Hide the resting readout when both HP and MP are full.", &this->hide_full).VisibleWhen(this->resting);
        case 9: return module_option::Slider("Recovery goal %", "Target percentage for recovery time estimates; completion sound is for full HP/MP.", &this->goal, 1, 100, 1).VisibleWhen(recoveryDetails && this->recovery_time);
        case 10:
        {
            auto option = module_option::Dropdown("Full recovery sound", "Play once when resting fills HP and MP. Off by default.", &this->sound, sound_names, std::size(sound_names));
            option.choice_preview = true; option.preview_first = 1; return option.VisibleWhen(this->resting);
        }
        case 11: return module_option::Slider("Timer bar height", "Height of attack and recovery tracks.", &this->height, 2, 24, 1).Ordered(112).VisibleWhen(fullReadout && this->display == 0);
        case 12: return module_option::Slider("Timer X offset", "Move timers relative to Player Bar; negative values are supported.", &this->offset_x, -1000, 1000, 1).Ordered(210, true).VisibleWhen(this->attack || this->resting);
        case 13: return module_option::Slider("Timer Y offset", "Move timers above, over or below Player Bar.", &this->offset_y, -1000, 1000, 1).Ordered(211).VisibleWhen(this->attack || this->resting);
        case 14: return module_option::Slider("Timer spacing", "Gap between Player Bar and its timer readouts.", &this->spacing, 0, 30, 1).Ordered(300, true).VisibleWhen(fullReadout);
        case 15: return module_option::Choice("Timer display", "Full timer readouts: bar and text, or text only. Mini bars have their own text choices.", &this->display, displays, 2).Ordered(-30).VisibleWhen(fullReadout);
        case 16: return module_option::Choice("Timer direction", "Fill toward the next action or drain its remaining time.", &this->direction, directions, 2).VisibleWhen(attackBar || recoveryBar);
        case 17: return module_option::Slider("Almost ready seconds", "Highlight the final part of the attack countdown.", &this->ready_seconds, 0, 2, 0.1f).VisibleWhen(this->attack);
        case 18: return module_option::Toggle("Tick highlight", "Briefly highlight a real resting tick.", &this->flash).VisibleWhen(recoveryBar);
        case 19: return module_option::Toggle("Separate HP / MP times", "Show individual recovery estimates instead of one combined goal time.", &this->resource_times).VisibleWhen(recoveryDetails && this->recovery_time);
        case 20: return module_option::Color("Attack timer color", "Attack fill; HUD text keeps its normal effects.", &this->attack_color).VisibleWhen(attackBar);
        case 21: return module_option::Color("Recovery timer color", "Recovery fill and tick highlight.", &this->rest_color).VisibleWhen(recoveryBar);
        case 22: return module_option::Color("Almost ready color", "Final attack countdown and tick highlight.", &this->ready_color).VisibleWhen(this->attack || (recoveryBar && this->flash));
        case 23: return module_option::Choice("Attack placement", "Full readout below or above Player Bar, or a slim bar attached beneath TP.", &this->attack_layout, layouts, 3).Ordered(200).VisibleWhen(this->attack);
        case 24: return module_option::Slider("Mini bar height", "Track thickness when Attack placement is Mini below TP.", &this->mini_height, 1, 12, 1).Ordered(112).VisibleWhen(this->attack && this->attack_layout == 2);
        case 25: return module_option::Slider("Mini bar gap", "Space between the TP track and its mini attack bar.", &this->mini_gap, 0, 24, 1).Ordered(302).VisibleWhen(this->attack && this->attack_layout == 2);
        case 26: return module_option::Choice("Mini bar text", "Optional styled text beneath the mini bar. None keeps only the thin track.", &this->mini_text, miniText, 3).VisibleWhen(this->attack && this->attack_layout == 2);
        case 27: return module_option::Choice("Resting placement", "Full readout below or above Player Bar, or a slim bar attached beneath HP.", &this->recovery_layout, recoveryLayouts, 3).Ordered(200).VisibleWhen(this->resting);
        case 28: return module_option::Slider("Mini bar height", "Track thickness when Resting placement is Mini below HP.", &this->recovery_mini_height, 1, 12, 1).Ordered(112).VisibleWhen(this->resting && this->recovery_layout == 2);
        case 29: return module_option::Slider("Mini bar gap", "Space between the HP track and its mini resting bar.", &this->recovery_mini_gap, 0, 24, 1).Ordered(302).VisibleWhen(this->resting && this->recovery_layout == 2);
        case 30: return module_option::Choice("Mini bar text", "Optional styled text beneath the resting mini bar. None keeps only the thin track.", &this->recovery_mini_text, recoveryMiniText, 3).VisibleWhen(this->resting && this->recovery_layout == 2);
        default: return {};
        }
    }

    auto enemy_timing_settings::Load(const settings_store& store, const char* section) -> void
    {
        this->enabled = store.ReadBool(section, "enabled", false); this->text_only = store.ReadBool(section, "text_only", false);
        this->drain = store.ReadBool(section, "drain", false); this->height = std::clamp(FiniteFloat(store, section, "height", 4), 2.0f, 24.0f);
        this->color = store.ReadArgb(section, "color", this->color); this->ready_color = store.ReadArgb(section, "ready_color", this->ready_color);
        this->text = LoadTextProfile(store, section, this->text);
    }

    auto enemy_timing_settings::Save(const settings_store& store, const char* section) const -> void
    {
        store.WriteBool(section, "enabled", this->enabled); store.WriteBool(section, "text_only", this->text_only); store.WriteBool(section, "drain", this->drain);
        store.WriteFloat(section, "height", this->height); store.WriteArgb(section, "color", this->color); store.WriteArgb(section, "ready_color", this->ready_color);
        SaveTextProfile(store, section, this->text);
    }

    auto enemy_timing_settings::Option(const size_t index) -> module_option
    {
        switch (index)
        {
        case 0: return module_option::Toggle("Enemy attack timer", "Observed melee timing with cast/action holds and disabling effects. Off by default; ~ means estimated.", &this->enabled).Ordered(-40);
        case 1: return module_option::Slider("Attack timer height", "Height of the enemy attack track.", &this->height, 2, 24, 1).Ordered(112).VisibleWhen(this->enabled && !this->text_only);
        case 2: return module_option::Toggle("Text only", "Keep the countdown and status without drawing a track.", &this->text_only).VisibleWhen(this->enabled);
        case 3: return module_option::Toggle("Drain", "Drain remaining time instead of filling toward the next attack.", &this->drain).VisibleWhen(this->enabled && !this->text_only);
        case 4: return module_option::Color("Attack timer color", "Enemy countdown fill.", &this->color).VisibleWhen(this->enabled && !this->text_only);
        case 5: return module_option::Color("Almost ready color", "Final half second before an expected attack.", &this->ready_color).VisibleWhen(this->enabled);
        default: return {};
        }
    }

    auto TimingSections(const bool recovery) -> std::span<const module_config_section>
    {
        static const auto sections = [] {
            std::array<module_config_section, 9> result{};
            std::copy(BarConfigSections().begin(), BarConfigSections().end(), result.begin());
            result[7] = {"Attack Timing", "Cadence-style observed attack timing and TP estimates. Predictions are marked ~.", "attack-timing"};
            result[8] = {"Recovery", "Resting ticks, HP / MP gains, recovery goals and optional completion sounds.", "recovery"};
            return result;
        }();
        return std::span{sections}.first(recovery ? 9 : 8);
    }

    auto TimingHeight(const module_text_profile& text, const float height, const bool textOnly, const bool details) -> float
    {
        return std::max(18.0f, text.style.scale * 20) * (details ? 2 : 1) + (textOnly ? 0 : height + 4) + 4;
    }

    auto DrawTiming(const attack_prediction& timer, const char* name, const char* details, const rect& bounds,
        const meter_style& meter, const module_text_profile& profile, const float height, const bool textOnly, const bool drain,
        const D3DCOLOR color, const D3DCOLOR ready, const float readySeconds, const module_render_context& context) -> float
    {
        if (!timer.active) return 0;
        const bool soon = timer.known && timer.state.empty() && timer.seconds <= readySeconds;
        const auto tint = soon || timer.state == "Ready" ? ready : color;
        auto text = soon ? TintTextProfile(profile, ready) : profile;
        const float lineHeight = std::max(18.0f, text.style.scale * 20);
        char label[128]{};
        if (!timer.state.empty()) std::snprintf(label, sizeof(label), "%s: %.*s", name, static_cast<int>(timer.state.size()), timer.state.data());
        else if (!timer.known) std::snprintf(label, sizeof(label), "%s: waiting for observation", name);
        else std::snprintf(label, sizeof(label), "%s ~%.1fs", name, timer.seconds);
        text.style.alignment = text_alignment::left;
        rect line{bounds.x, bounds.y, bounds.width, lineHeight};
        context.DrawText(label, line, line, text);
        float y = bounds.y + lineHeight;
        if (!textOnly)
        {
            auto style = meter; style.height = height;
            context.DrawBar({bounds.x, y, bounds.width, height + 2}, drain ? 1 - timer.ratio : timer.ratio,
                MeterGradientTop(tint), tint, 0, style);
            y += height + 4;
        }
        if (details != nullptr && details[0] != '\0')
        {
            line.y = y;
            context.DrawText(details, line, line, profile); y += lineHeight;
        }
        return y - bounds.y + 4;
    }

    auto DrawMiniTiming(const attack_prediction& timer, const char* details, const rect& bounds, const meter_style& meter,
        const module_text_profile& profile, const int textMode, const bool drain, const D3DCOLOR color, const module_render_context& context) -> float
    {
        if (!timer.active) return 0;
        auto style = meter; style.height = bounds.height;
        context.DrawBar(bounds, drain ? 1 - timer.ratio : timer.ratio, MeterGradientTop(color), color, 0, style);
        float y = bounds.y + bounds.height;
        if (textMode > 0)
        {
            auto text = profile;
            text.style.alignment = text_alignment::left;
            const float lineHeight = std::max(18.0f, text.style.scale * 20);
            const auto draw = [&](std::string value, const rect& line) {
                const auto full = value;
                while (!value.empty() && context.Design().MeasureText(value.c_str(), text.style) > line.width) value.pop_back();
                if (value != full)
                {
                    while (!value.empty() && context.Design().MeasureText((value + "...").c_str(), text.style) > line.width) value.pop_back();
                    value += "...";
                }
                context.DrawText(value.c_str(), line, line, text);
            };
            char label[96]{};
            if (!timer.state.empty()) std::snprintf(label, sizeof(label), "%.*s", static_cast<int>(timer.state.size()), timer.state.data());
            else if (timer.known) std::snprintf(label, sizeof(label), "~%.1fs", timer.seconds);
            else std::snprintf(label, sizeof(label), "Waiting for observation");
            y += 2;
            draw(label, {bounds.x, y, bounds.width, lineHeight}); y += lineHeight;
            if (textMode == 2 && details != nullptr && details[0] != '\0')
            { draw(details, {bounds.x, y, bounds.width, lineHeight}); y += lineHeight; }
        }
        return y - bounds.y;
    }

    auto TPDetails(const tp_prediction& prediction, const uint32_t goal, const player_timing_settings& settings, const bool compact) -> std::string
    {
        std::string result = compact ? std::to_string(goal) + " TP" : "TP goal " + std::to_string(goal);
        if (settings.rounds && prediction.rounds >= 0) result += "  ~" + std::to_string(prediction.rounds) + (compact ? "r" : " rounds");
        if (settings.hits && prediction.hits >= 0) result += "  ~" + std::to_string(prediction.hits) + (compact ? "h" : " hits");
        if (settings.tp_time && prediction.seconds >= 0) result += "  ~" + std::to_string(static_cast<int>(std::ceil(prediction.seconds))) + "s";
        return result;
    }

    auto RecoveryDetails(const recovery_clock& recovery, const double now, const player_timing_settings& settings) -> std::string
    {
        std::string result;
        if (settings.gains) result = recovery.LastObservedTick() > 0 ? "Last tick: HP +" + std::to_string(recovery.LastHP()) + "  MP +" + std::to_string(recovery.LastMP()) : "Last tick: --";
        const auto append = [&](const char* label, float seconds) {
            if (seconds >= 0) result += std::string{result.empty() ? "" : "  "} + label + " ~" + std::to_string(static_cast<int>(std::ceil(seconds))) + "s";
        };
        if (settings.recovery_time)
        {
            if (settings.resource_times)
            { append("HP", recovery.SecondsToGoal(now, settings.goal, true, false)); append("MP", recovery.SecondsToGoal(now, settings.goal, false, true)); }
            else append("Goal", recovery.SecondsToGoal(now, settings.goal));
        }
        return result;
    }
}
