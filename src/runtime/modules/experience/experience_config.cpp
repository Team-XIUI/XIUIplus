#include "experience_module.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace xiui
{
    auto experience_module::Id(void) const -> const char* { return "experience"; }
    auto experience_module::DisplayName(void) const -> const char* { return "EXP Bar"; }
    auto experience_module::State(void) -> module_state& { return this->state_; }
    auto experience_module::State(void) const -> const module_state& { return this->state_; }
    auto experience_module::Settings(void) -> settings& { return this->settings_; }

    auto experience_module::ApplyMeterDefaults(const ui::meter_style& style) -> void
    {
        this->settings_.style = style;
    }

    auto experience_module::ApplyTextDefaults(const ui::theme& theme) -> void
    {
        this->settings_.header_text = {theme.meter_text};
        this->settings_.header_text.style.alignment = ui::text_alignment::left;
        this->settings_.details_text = {theme.meter_text};
        this->settings_.details_text.style.alignment = ui::text_alignment::right;
        this->settings_.readout_text = {theme.meter_text};
        this->settings_.readout_text.style.alignment = ui::text_alignment::left;
        this->legacy_readout_style_ = theme.caption_text;
    }

    auto experience_module::ResetDefaults(const ui::theme& themeDefaults) -> void
    {
        const ui::rect bounds = this->state_.bounds;
        auto source = std::move(this->source_);
        *this = experience_module{};
        this->source_ = std::move(source);
        this->state_.bounds = bounds;
        this->ApplyMeterDefaults(themeDefaults.meter);
        this->ApplyTextDefaults(themeDefaults);
    }

    auto experience_module::SelectedTextProfile(void) -> module_text_profile&
    {
        return this->settings_.selected_text == 2 ? this->settings_.readout_text :
            this->settings_.selected_text == 1 ? this->settings_.details_text : this->settings_.header_text;
    }

    auto experience_module::Load(const settings_store& store) -> void
    {
        this->state_.enabled = store.ReadBool("exp", "enabled", true);
        this->settings_.show_current = store.ReadBool("exp", "show_current", true);
        this->settings_.show_percent = store.ReadBool("exp", "show_percent", true);
        this->settings_.show_remaining = store.ReadBool("exp", "show_remaining", true);
        this->settings_.show_label = store.ReadBool("exp", "show_label", true);
        this->settings_.stable_numbers = store.ReadBool("exp", "stable_numbers", true);
        this->readout_widths_.fill(0);
        const int mode = store.ReadInt("exp", "mode", 0);
        this->settings_.mode = mode >= 0 && mode <= 4 ? mode : 0;
        this->settings_.show_points = store.ReadBool("exp", "show_points", this->settings_.show_current);
        this->settings_.show_master_level = store.ReadBool("exp", "show_master_level", this->settings_.show_current);
        this->settings_.info_position = std::clamp(store.ReadInt("exp", "info_position", 0), 0, 1);
        const D3DCOLOR colorFallback = store.ReadArgb("exp", "color", D3DCOLOR_XRGB(216, 240, 138));
        this->settings_.color_top = store.ReadArgb("meter.experience", "fill_top", MeterGradientTop(colorFallback));
        this->settings_.color = store.ReadArgb("meter.experience", "fill_bottom", colorFallback);
        this->settings_.style = LoadMeterStyle(store, "meter.experience", this->settings_.style);
        this->settings_.header_text = LoadTextProfile(store, "text.experience.header", this->settings_.header_text);
        this->settings_.details_text = LoadTextProfile(store, "text.experience.details", this->settings_.details_text);
        if (store.ReadInt("exp", "text_layout_version", 0) < 1)
        {
            this->settings_.header_text = LoadTextProfile(store, "text.player.labels", this->settings_.header_text);
            this->settings_.details_text = LoadTextProfile(store, "text.player.values", this->settings_.details_text);
            for (auto* profile : {&this->settings_.header_text, &this->settings_.details_text})
            {
                profile->offset_x = profile->offset_y = 0;
                profile->placement = 0;
            }
            this->settings_.header_text.style.alignment = ui::text_alignment::left;
            this->settings_.details_text.style.alignment = ui::text_alignment::right;
        }
        const auto readoutDefaults = this->settings_.readout_text.style;
        this->settings_.readout_text = LoadTextProfile(store, "text.experience.readouts", this->settings_.readout_text);
        if (store.ReadInt("exp", "readout_text_version", 0) < 1)
        {
            auto& style = this->settings_.readout_text.style;
            const auto& previous = this->legacy_readout_style_;
            const auto migrate = []<typename T>(T& value, const T& oldValue, const T& newValue) { if (value == oldValue) value = newValue; };
            migrate(style.scale, 0.75f, readoutDefaults.scale);
            migrate(style.letter_spacing, previous.letter_spacing, readoutDefaults.letter_spacing);
            migrate(style.top_color, previous.top_color, readoutDefaults.top_color);
            migrate(style.bottom_color, previous.bottom_color, readoutDefaults.bottom_color);
            migrate(style.shadow.enabled, previous.shadow.enabled, readoutDefaults.shadow.enabled);
            migrate(style.shadow.color, previous.shadow.color, readoutDefaults.shadow.color);
            migrate(style.shadow.soft, previous.shadow.soft, readoutDefaults.shadow.soft);
            migrate(style.stroke.enabled, previous.stroke.enabled, readoutDefaults.stroke.enabled);
            migrate(style.stroke.color, previous.stroke.color, readoutDefaults.stroke.color);
            migrate(style.glow.enabled, previous.glow.enabled, readoutDefaults.glow.enabled);
            migrate(style.glow.color, previous.glow.color, readoutDefaults.glow.color);
        }
        this->settings_.selected_text = std::clamp(store.ReadInt("exp", "selected_text", 0), 0, 2);
        for (size_t i = 0; i < experience_readout_count; ++i)
        {
            this->settings_.readouts[i] = store.ReadBool("exp.readouts", experience_readouts[i].key, this->settings_.readouts[i]);
            this->settings_.order[i] = static_cast<uint8_t>(std::clamp(store.ReadInt("exp.readout_order", experience_readouts[i].key, this->settings_.order[i]), 0, static_cast<int>(experience_readout_count - 1)));
        }
        if (this->settings_.readouts[static_cast<size_t>(experience_readout::current_required)]) this->settings_.show_current = false;
        if (this->settings_.readouts[static_cast<size_t>(experience_readout::percent)]) this->settings_.show_percent = false;
        if (this->settings_.readouts[static_cast<size_t>(experience_readout::remaining)]) this->settings_.show_remaining = false;
        auto order = DefaultExperienceReadoutOrder();
        std::stable_sort(order.begin(), order.end(), [&](auto a, auto b) { return this->settings_.order[a] < this->settings_.order[b]; });
        for (size_t i = 0; i < order.size(); ++i) this->settings_.order[order[i]] = static_cast<uint8_t>(i);
        const auto jobReadout = static_cast<size_t>(experience_readout::job);
        if (store.ReadInt("exp", "job_readout_version", 0) < 1 && !this->settings_.readouts[jobReadout])
        {
            this->settings_.readouts[jobReadout] = true;
            const auto previous = this->settings_.order[jobReadout];
            for (auto& position : this->settings_.order) if (position < previous) ++position;
            this->settings_.order[jobReadout] = 0;
        }
        const auto integer = [&](const char* key, int fallback, int maximum) { return std::clamp(store.ReadInt("exp", key, fallback), 0, maximum); };
        this->settings_.rate_window = integer("rate_window", 1, 3); this->settings_.rate_basis = integer("rate_basis", 0, 1);
        this->settings_.goal_type = integer("goal_type", 0, 5); this->settings_.label_position = integer("label_position", 0, 2);
        this->settings_.divider = integer("divider", 1, 3);
        this->settings_.readout_anchor = integer("readout_anchor", 0, 2);
        const auto number = [&](const char* key, float& value, float minimum, float maximum) {
            const float loaded = store.ReadFloat("exp", key, value); value = std::clamp(std::isfinite(loaded) ? loaded : value, minimum, maximum);
        };
        number("readout_rows", this->settings_.readout_rows, 1, 8); number("row_spacing", this->settings_.row_spacing, 0, 24);
        number("bar_gap", this->settings_.bar_gap, 0, 48);
        number("recent_count", this->settings_.recent_count, 1, 100); number("goal_target", this->settings_.goal_target, 1, 10000000);
        number("sync_level", this->settings_.sync_level, 10, 75); number("sync_population", this->settings_.sync_population, 0, 5000);
        number("chain_warning", this->settings_.chain_warning, 1, 30); number("volume", this->settings_.volume, 0, 150);
        for (const auto& [key, value] : {std::pair{"short_numbers", &this->settings_.short_numbers}, {"alert_food", &this->settings_.alert_food},
            {"alert_dedication", &this->settings_.alert_dedication}, {"alert_chain", &this->settings_.alert_chain}, {"alert_merits", &this->settings_.alert_merits},
            {"alert_goal", &this->settings_.alert_goal}, {"sound", &this->settings_.sound}, {"mute_paused", &this->settings_.mute_paused}})
            *value = store.ReadBool("exp", key, *value);
        this->settings_.warning_color = store.ReadArgb("exp", "warning_color", this->settings_.warning_color);
        this->source_.Load(store);
        this->animation_ = {};
        this->state_.bounds = store.ReadRect("layout.exp", {24.0f, 226.0f, 540.0f, 74.0f});
        this->state_.content_bounds = {};
        this->OnOptionChanged(mode_option);
    }

    auto experience_module::Save(const settings_store& store) const -> void
    {
        store.WriteBool("exp", "enabled", this->state_.enabled);
        store.WriteBool("exp", "show_current", this->settings_.show_current);
        store.WriteBool("exp", "show_percent", this->settings_.show_percent);
        store.WriteBool("exp", "show_remaining", this->settings_.show_remaining);
        store.WriteBool("exp", "show_label", this->settings_.show_label);
        store.WriteBool("exp", "stable_numbers", this->settings_.stable_numbers);
        store.WriteInt("exp", "mode", this->settings_.mode);
        store.WriteBool("exp", "show_points", this->settings_.show_points);
        store.WriteBool("exp", "show_master_level", this->settings_.show_master_level);
        store.WriteInt("exp", "info_position", this->settings_.info_position);
        store.WriteInt("exp", "readout_anchor", this->settings_.readout_anchor);
        store.WriteInt("exp", "text_layout_version", 1);
        store.WriteInt("exp", "selected_text", this->settings_.selected_text);
        store.WriteInt("exp", "readout_text_version", 1);
        store.WriteInt("exp", "job_readout_version", 1);
        store.WriteArgb("exp", "color", this->settings_.color);
        store.WriteArgb("meter.experience", "fill_top", this->settings_.color_top);
        store.WriteArgb("meter.experience", "fill_bottom", this->settings_.color);
        SaveMeterStyle(store, "meter.experience", this->settings_.style);
        SaveTextProfile(store, "text.experience.header", this->settings_.header_text);
        SaveTextProfile(store, "text.experience.details", this->settings_.details_text);
        SaveTextProfile(store, "text.experience.readouts", this->settings_.readout_text);
        for (size_t i = 0; i < experience_readout_count; ++i)
        {
            store.WriteBool("exp.readouts", experience_readouts[i].key, this->settings_.readouts[i]);
            store.WriteInt("exp.readout_order", experience_readouts[i].key, this->settings_.order[i]);
        }
        for (const auto& [key, value] : {std::pair{"rate_window", this->settings_.rate_window}, {"rate_basis", this->settings_.rate_basis},
            {"goal_type", this->settings_.goal_type}, {"label_position", this->settings_.label_position}, {"divider", this->settings_.divider}}) store.WriteInt("exp", key, value);
        for (const auto& [key, value] : {std::pair{"readout_rows", this->settings_.readout_rows}, {"row_spacing", this->settings_.row_spacing},
            {"bar_gap", this->settings_.bar_gap},
            {"recent_count", this->settings_.recent_count}, {"goal_target", this->settings_.goal_target}, {"sync_level", this->settings_.sync_level},
            {"sync_population", this->settings_.sync_population}, {"chain_warning", this->settings_.chain_warning}, {"volume", this->settings_.volume}}) store.WriteFloat("exp", key, value);
        for (const auto& [key, value] : {std::pair{"short_numbers", this->settings_.short_numbers}, {"alert_food", this->settings_.alert_food},
            {"alert_dedication", this->settings_.alert_dedication}, {"alert_chain", this->settings_.alert_chain}, {"alert_merits", this->settings_.alert_merits},
            {"alert_goal", this->settings_.alert_goal}, {"sound", this->settings_.sound}, {"mute_paused", this->settings_.mute_paused}}) store.WriteBool("exp", key, value);
        store.WriteArgb("exp", "warning_color", this->settings_.warning_color);
        this->source_.Save(store);
        store.WriteRect("layout.exp", this->state_.bounds);
    }

    auto experience_module::OptionCount(void) const -> size_t { return option_count; }

    auto experience_module::ConfigSections(void) const -> std::span<const module_config_section>
    {
        static constexpr module_config_section sections[]{
            {"General", "Choose the progression mode, pause tracking or save and reset the session."},
            {"Display", "Current progress on the bar; session stats, job details and timers below it."},
            {"Appearance", "Bar style, bookends, fill and track colors."},
            {"Effects", "Fill animation, highlights, shadows and glow."},
            {"Text", "Independent label, bar-info and readout text styles."},
            {"Tracking", "Rate calculation, recent rewards, EXP or merit goals and Level Sync planning."},
            {"Alerts", "Optional local notices and chimes. All alerts start disabled."},
            {"History", "The last ten completed sessions for this character."}};
        return sections;
    }

    auto experience_module::ConfigSectionContains(const size_t section, const size_t index) -> bool
    {
        if (section >= this->ConfigSections().size() || index >= this->OptionCount()) return false;
        if (index == stable_numbers_option) return section == text;
        if (index >= bar_gap_option) return section == display;
        if (index >= history_option) return section == history;
        if (index >= alert_food_option && index <= mute_paused_option) return section == alerts;
        if (index == chain_warning_option || index == warning_color_option) return section == alerts;
        if (index == pause_option || index == reset_option) return section == general;
        if (index >= rate_window_option && index <= sync_population_option) return section == tracking;
        if (index >= readout_option && index <= short_numbers_option) return section == display;
        if (index >= 9 && index < 9 + meter_style_option_count)
        {
            const auto style = index - 9;
            if (style == 1) return section == display;
            if (style <= 5 || (style >= 18 && style <= 26)) return section == appearance;
            return section == effects;
        }
        if (index >= 9 + meter_style_option_count && index < legacy_option_count - 1) return section == text;
        if (index == 0 || index == mode_option || index == summary_option) return section == general;
        if (index == 4) return section == text;
        if (index == 5 || index == 6) return section == appearance;
        return section == display;
    }

    auto experience_module::Option(const size_t index) -> module_option
    {
        if (index == stable_numbers_option)
            return module_option::Toggle("Stable numbers", "Equal-width digits and steady readout slots keep timers and changing values from shifting nearby text.", &this->settings_.stable_numbers, module_option_group::text);
        if (index == 9 + meter_style_option_count + text_profile_option_count)
            return ModuleWidthOption(this->state_);
        static constexpr const char* textNames[] = {"Label", "Info", "Readouts"};
        static constexpr const char* infoPositions[] = {"Above", "Below"};
        static constexpr const char* readoutAnchors[] = {"Below", "Above", "Overlay"};
        static constexpr const char* modes[]{"Auto", "EXP", "Limit Points", "Capacity Pts", "Exemplar Pts"};
        static const auto readings = [] { std::array<const char*, experience_readout_count> names{}; for (size_t i = 0; i < names.size(); ++i) names[i] = experience_readouts[i].name; return names; }();
        static constexpr const char* windows[]{"5 minutes", "15 minutes", "30 minutes", "Whole session"};
        static constexpr const char* bases[]{"Points received", "Points retained"};
        static constexpr const char* labels[]{"Before value", "After value", "Hidden"};
        static constexpr const char* dividers[]{"Space", "Line", "Slash", "Dot"};
        static constexpr const char* goalTypes[]{"None", "Session EXP", "Target level", "Merits earned", "Stored merits", "EXP buffer"};
        switch (index)
        {
        case 0: return module_option::Toggle("Enabled", "Show the EXP HUD.", &this->state_.enabled).Ordered(-50);
        case 1: return module_option::Toggle("Current / required", "Show on the bar. Enabling this moves it out of the readout list.", &this->settings_.show_current);
        case 2: return module_option::Toggle("Percent", "Show on the bar. Enabling this moves it out of the readout list.", &this->settings_.show_percent);
        case 3: return module_option::Toggle("Remaining", "Show on the bar. Enabling this moves it out of the readout list.", &this->settings_.show_remaining);
        case 4: return module_option::Element("Selected text", "Text element configured in Text.", &this->settings_.selected_text, textNames, 3);
        case 5: return module_option::Color("Fill top", "Upper fill gradient.", &this->settings_.color_top, module_option_group::colors).Ordered(500, true);
        case 6: return module_option::Color("Fill bottom", "Lower fill gradient.", &this->settings_.color, module_option_group::colors).Ordered(501);
        case 7: return module_option::Toggle("Mode label", "Show EXP, LIMIT, CAPACITY or EXEMPLAR on the bar's upper edge.", &this->settings_.show_label);
        case 8: return module_option::Choice("Info position", "Inset info at the upper or lower bar edge.", &this->settings_.info_position, infoPositions, 2);
        case mode_option: return module_option::Choice("Progression mode", "Auto, Experience, Limit, Capacity or Exemplar Points. This only controls the XIUI bar.", &this->settings_.mode, modes, 5);
        case points_option:
        {
            auto option = module_option::Toggle("Merit / JP totals", "Show accumulated merit points or job points independently of current/required points.", &this->settings_.show_points);
            return option.VisibleWhen(this->settings_.mode == 0 || this->settings_.mode == 2 || this->settings_.mode == 3);
        }
        case master_level_option:
        {
            auto option = module_option::Toggle("Master level", "Show the current master level with Exemplar Points.", &this->settings_.show_master_level);
            return option.VisibleWhen(this->settings_.mode == 0 || this->settings_.mode == 4);
        }
        case summary_option:
        {
            std::snprintf(this->mode_summary_, sizeof(this->mode_summary_), "Active: %s", this->progression_.Name());
            module_option option{};
            option.label = this->mode_summary_;
            option.description = !this->progression_ready_ ? "Waiting for character data." : !this->progression_.available ? this->progression_.UnavailableReason() :
                this->settings_.mode == 0 ? "Auto follows your current job and EXP/limit mode." : "Manual selection stays on this progression type.";
            option.available = false;
            return option;
        }
        case readout_option: return module_option::Choice("Selected readout", "Choose the reading to show, hide or reorder.", &this->settings_.selected_readout, readings.data(), readings.size());
        case readout_show_option: return module_option::Toggle("Show readout", "Unknown readings show --. Disabled readings leave no gap.", &this->settings_.readouts[this->settings_.selected_readout]);
        case readout_order_option: return module_option::Slider("Position", "Position among all readings; hidden readings retain their place.", &this->settings_.readout_position, 1, static_cast<float>(experience_readout_count), 1).Ordered(200);
        case readout_rows_option: return module_option::Slider("Maximum rows", "Wrap the readout block up to this limit.", &this->settings_.readout_rows, 1, 8, 1).Ordered(122);
        case bar_gap_option:
        {
            auto option = module_option::Slider("Readout gap", "Space from the bar when readouts are Above or Below.", &this->settings_.bar_gap, 0, 48, 1).Ordered(302);
            return option.VisibleWhen(this->settings_.readout_anchor != 2);
        }
        case readout_anchor_option: return module_option::Choice("Readout position", "Place the whole block above, below or over the bar.", &this->settings_.readout_anchor, readoutAnchors, 3).Ordered(200);
        case readout_x_option: return module_option::Slider("Readout X offset", "Move the entire readout block horizontally.", &this->settings_.readout_text.offset_x, -200, 200, 1).Ordered(210, true);
        case readout_y_option: return module_option::Slider("Readout Y offset", "Move the entire block vertically; negative values move it upward.", &this->settings_.readout_text.offset_y, -100, 100, 1).Ordered(211);
        case row_spacing_option: return module_option::Slider("Row spacing", "Gap between readout rows.", &this->settings_.row_spacing, 0, 24, 1).Ordered(301);
        case label_position_option: return module_option::Choice("Label position", "Show labels before or after values, or hide them to display values only.", &this->settings_.label_position, labels, std::size(labels));
        case divider_option: return module_option::Choice("Divider", "Separator between visible readings.", &this->settings_.divider, dividers, 4);
        case short_numbers_option: return module_option::Toggle("Short numbers", "Use k and m for large readout values.", &this->settings_.short_numbers);
        case rate_window_option: return module_option::Choice("Rate window", "Last 5, 15 or 30 active minutes, or the full session.", &this->settings_.rate_window, windows, 4);
        case rate_basis_option: return module_option::Choice("Rate calculation", "Received counts reported awards; retained includes observed progress, losses and recovery.", &this->settings_.rate_basis, bases, 2);
        case recent_count_option: return module_option::Slider("Recent rewards", "Number of rewards used for recent average, median, minimum and maximum.", &this->settings_.recent_count, 1, 100, 1);
        case pause_option: return module_option::Toggle("Pause session", "Exclude gains and time while balances, chains and buff timers continue updating.", &this->settings_.paused);
        case reset_option: return module_option::Action("Reset session", "Save this session in History and clear its totals and rates. Active timers continue.", "Reset");
        case goal_type_option: return module_option::Choice("Goal", "Choose an EXP or merit target. This does not change gameplay mode.", &this->settings_.goal_type, goalTypes, 6);
        case goal_target_option: return module_option::Slider("Target", "Target value; level goals use Phoenix's level-75 EXP requirements.", &this->settings_.goal_target, 1, this->settings_.goal_type == 2 ? 75.0f : this->settings_.goal_type == 3 || this->settings_.goal_type == 4 ? 1000.0f : 10000000.0f, 1).VisibleWhen(this->settings_.goal_type != 0);
        case sync_level_option: return module_option::Slider("Planned sync level", "Target level for the separate penalty calculator.", &this->settings_.sync_level, 10, 75, 1);
        case sync_population_option: return module_option::Slider("Planned population", "Server population for the calculator; does not claim an applied penalty.", &this->settings_.sync_population, 0, 5000, 1);
        case alert_food_option: return module_option::Toggle("Food ended", "Notify after a confirmed local food removal message.", &this->settings_.alert_food);
        case alert_dedication_option: return module_option::Toggle("Dedication ended", "Notify after a confirmed local Dedication removal message.", &this->settings_.alert_dedication);
        case alert_chain_option: return module_option::Toggle("Chain ending", "Warn once near the end of the chain countdown.", &this->settings_.alert_chain);
        case alert_merits_option: return module_option::Toggle("Merit storage full", "Notify when stored merits reach the client-reported capacity.", &this->settings_.alert_merits);
        case alert_goal_option: return module_option::Toggle("Goal reached", "Notify once when the selected goal is reached.", &this->settings_.alert_goal).VisibleWhen(this->settings_.goal_type != 0);
        case sound_option: return module_option::Toggle("Alert sound", "Play XIUI's local notification chime with enabled alerts.", &this->settings_.sound);
        case volume_option: return module_option::Slider("Sound volume", "Local alert chime level.", &this->settings_.volume, 0, 150, 1).VisibleWhen(this->settings_.sound);
        case mute_paused_option: return module_option::Toggle("Mute while paused", "Suppress optional notices and sounds during a manual pause.", &this->settings_.mute_paused);
        case chain_warning_option: return module_option::Slider("Chain warning", "Seconds remaining before the countdown uses its warning color.", &this->settings_.chain_warning, 1, 30, 1);
        case warning_color_option: return module_option::Color("Warning color", "Tint chain countdown warnings while retaining normal text effects.", &this->settings_.warning_color);
        default:
            if (index >= history_option && index < bar_gap_option)
            {
                const auto row = index - history_option; const auto& records = this->source_.tracker.History();
                auto& label = this->history_labels_[row];
                if (row < records.size())
                {
                    const auto& record = records[row];
                    std::snprintf(label.data(), label.size(), "%zu. Job %u | %llum | EXP %llu | LP %llu", row + 1, record.job,
                        record.elapsed / 60000, record.totals[0].received, record.totals[1].received);
                }
                else std::snprintf(label.data(), label.size(), "%zu. --", row + 1);
                module_option option{}; option.label = label.data(); option.available = false;
                option.description = "Completed sessions stay local and separate for each character. /xiui exp history prints their details."; return option;
            }
            if (index < 9 + meter_style_option_count)
                return MeterStyleOption(index - 9, this->settings_.style);
            return TextProfileOption(index - 9 - meter_style_option_count, this->SelectedTextProfile());
        }
    }

    auto experience_module::DisplayOption(const experience_display_view view, const size_t index) const -> bool
    {
        if (view == experience_display_view::bar_info)
            return index == 1 || index == 2 || index == 3 || index == 7 || index == 8 || index == points_option || index == master_level_option;
        if (view == experience_display_view::layout)
            return index == 10 || index == 9 + meter_style_option_count + text_profile_option_count ||
                (index >= readout_rows_option && index <= short_numbers_option) || (index >= bar_gap_option && index < stable_numbers_option);
        return false;
    }

    auto experience_module::SetReadoutVisible(const size_t index, const bool visible) -> void
    {
        if (index >= experience_readout_count || this->settings_.readouts[index] == visible) return;
        this->settings_.selected_readout = static_cast<int>(index);
        if (visible)
        {
            const auto before = this->settings_.order[index];
            int last = -1;
            for (size_t i = 0; i < experience_readout_count; ++i)
                if (this->settings_.readouts[i]) last = std::max(last, static_cast<int>(this->settings_.order[i]));
            this->settings_.readout_position = last < 0 ? 1.0f : static_cast<float>(last + (before > last ? 2 : 1));
            this->OnOptionChanged(readout_order_option);
        }
        this->settings_.readouts[index] = visible;
        if (visible && index == static_cast<size_t>(experience_readout::current_required)) this->settings_.show_current = false;
        if (visible && index == static_cast<size_t>(experience_readout::percent)) this->settings_.show_percent = false;
        if (visible && index == static_cast<size_t>(experience_readout::remaining)) this->settings_.show_remaining = false;
        this->OnOptionChanged(readout_option);
    }

    auto experience_module::MoveReadout(const size_t index, const int direction) -> bool
    {
        if (index >= experience_readout_count || !this->settings_.readouts[index] || direction == 0) return false;
        const auto before = this->settings_.order[index];
        size_t neighbor = experience_readout_count;
        for (size_t i = 0; i < experience_readout_count; ++i)
        {
            if (!this->settings_.readouts[i] || (direction < 0 ? this->settings_.order[i] >= before : this->settings_.order[i] <= before)) continue;
            if (neighbor == experience_readout_count || (direction < 0 ? this->settings_.order[i] > this->settings_.order[neighbor] : this->settings_.order[i] < this->settings_.order[neighbor])) neighbor = i;
        }
        if (neighbor == experience_readout_count) return false;
        std::swap(this->settings_.order[index], this->settings_.order[neighbor]);
        this->settings_.selected_readout = static_cast<int>(index);
        this->OnOptionChanged(readout_option);
        return true;
    }

    auto experience_module::ReadoutRepeatsBar(const size_t index) const -> bool
    {
        return (index == static_cast<size_t>(experience_readout::buffer) && this->settings_.show_current && this->progression_.kind == progression_kind::experience) ||
            (index == static_cast<size_t>(experience_readout::lp_needed) && this->settings_.show_remaining && this->progression_.kind == progression_kind::limit) ||
            (index == static_cast<size_t>(experience_readout::current_required) && this->settings_.show_current) ||
            (index == static_cast<size_t>(experience_readout::percent) && this->settings_.show_percent) ||
            (index == static_cast<size_t>(experience_readout::remaining) && this->settings_.show_remaining);
    }

} // namespace xiui
