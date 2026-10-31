#pragma once

#include "combat/action_packet.hpp"

#include "../module.hpp"
#include "progression.hpp"
#include "experience_source.hpp"
#include "experience_readouts.hpp"
#include "modules/readycheck/ready_check_sound.hpp"

#include <memory>

namespace xiui
{
    enum class experience_display_view : uint8_t { bar_info, readouts, layout };
    class experience_module final : public hud_module
    {
    public:
        struct settings
        {
            bool show_current{true};
            bool show_percent{true};
            bool show_remaining{true};
            bool show_label{true};
            bool stable_numbers{true};
            int info_position{};
            D3DCOLOR color{D3DCOLOR_XRGB(216, 240, 138)};
            D3DCOLOR color_top{D3DCOLOR_XRGB(97, 108, 62)};
            ui::meter_style style{ui::theme::CreateDefault().meter};
            module_text_profile header_text{ui::theme::CreateDefault().meter_text};
            module_text_profile details_text{ui::theme::CreateDefault().meter_text};
            int selected_text{};
            int mode{};
            bool show_points{true}, show_master_level{true};
            module_text_profile readout_text{ui::theme::CreateDefault().meter_text};
            std::array<bool, experience_readout_count> readouts{DefaultExperienceReadouts()};
            std::array<uint8_t, experience_readout_count> order{DefaultExperienceReadoutOrder()};
            int selected_readout{}, rate_window{1}, rate_basis{}, label_position{}, divider{1}, goal_type{}, readout_anchor{};
            float readout_position{1}, readout_rows{3}, row_spacing{3}, bar_gap{8}, recent_count{10}, goal_target{10000}, sync_level{30}, sync_population{}, chain_warning{10}, volume{40};
            bool short_numbers{true}, paused{}, alert_food{}, alert_dedication{}, alert_chain{}, alert_merits{}, alert_goal{}, sound{}, mute_paused{true};
            D3DCOLOR warning_color{0xFFFF8585};
        };

        auto Id(void) const -> const char* override;
        auto DisplayName(void) const -> const char* override;
        auto State(void) -> module_state& override;
        auto State(void) const -> const module_state& override;
        auto Settings(void) -> settings&;
        auto Settings(void) const -> const settings& { return this->settings_; }
        auto DisplayOption(experience_display_view view, size_t index) const -> bool;
        auto UsesDisplayEditor(size_t section) const -> bool { return section == display; }
        auto SetReadoutVisible(size_t index, bool visible) -> void;
        auto MoveReadout(size_t index, int direction) -> bool;
        auto ReadoutRepeatsBar(size_t index) const -> bool;
        auto ReadoutTop(const ui::design_system& design, const ui::rect& meter, bool hasInfo, float blockHeight = 0) const -> float;
        auto ReadoutArea(const ui::design_system& design, const ui::rect& meter, bool hasInfo, float blockHeight) const -> ui::rect;
        auto ApplyMeterDefaults(const ui::meter_style& style) -> void override;
        auto PositionBoundsReady(void) const -> bool override
        { return this->state_.content_bounds.width > 0 && this->state_.content_bounds.height > 0; }
        auto VisitTextSettings(const text_settings_visitor& visit) -> void override
        { VisitTextStyles(visit, {&this->settings_.header_text.style, &this->settings_.details_text.style, &this->settings_.readout_text.style}); }
        auto ApplyTextDefaults(const ui::theme& theme) -> void override;
        auto ResetDefaults(const ui::theme& themeDefaults) -> void override;
        auto Load(const settings_store& store) -> void override;
        auto Save(const settings_store& store) const -> void override;
        auto ContentOptionCount(void) const -> size_t override;
        auto ContentOption(size_t index) -> module_option override;
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ContentConfigSectionContains(size_t section, size_t index) -> bool override;
        auto OnContentOptionChanged(size_t index) -> void override;
        auto ShouldRender(const hud_snapshot& snapshot) const -> bool override;
        auto Render(const hud_snapshot& snapshot, const module_render_context& context) -> void override;
        auto Update(IAshitaCore* core, uint64_t now) -> void;
        auto HandlePacket(IAshitaCore* core, uint16_t id, std::span<const uint8_t> data, uint64_t now, const combat_action_packet* decoded = nullptr) -> void;
        auto HandleCommand(IAshitaCore* core, std::string_view command) -> bool;
        auto NeedsSave(uint64_t now) -> bool;
        auto Progression(void) const -> const progression_state& { return this->progression_; }
        auto Utc(void) const -> uint32_t { return this->source_.Utc(); }

    private:
#if defined(XIUI_ENABLE_PARITY_TESTS)
        friend struct module_parity_test;
#endif
        static constexpr size_t legacy_option_count = 10 + meter_style_option_count + text_profile_option_count;
        enum extra_option : size_t { mode_option = legacy_option_count, points_option, master_level_option, summary_option,
            readout_option, readout_show_option, readout_order_option, readout_rows_option, row_spacing_option, label_position_option, divider_option, short_numbers_option,
            rate_window_option, rate_basis_option, recent_count_option, pause_option, reset_option, goal_type_option, goal_target_option, sync_level_option, sync_population_option,
            alert_food_option, alert_dedication_option, alert_chain_option, alert_merits_option, alert_goal_option, sound_option, volume_option, mute_paused_option,
            chain_warning_option, warning_color_option, history_option, bar_gap_option = history_option + 10,
            readout_anchor_option, readout_x_option, readout_y_option, stable_numbers_option, option_count };
        enum section : size_t { general, display, appearance, effects, text, tracking, alerts, history, layout };
        module_state state_{true, {24.0f, 226.0f, 540.0f, 74.0f}, 340.0f, 62.0f};
        settings settings_{};
        ui::text_style legacy_readout_style_{ui::theme::CreateDefault().caption_text};
        meter_animation_state animation_{};
        progression_state progression_{};
        uint64_t next_save_{}, now_{}, next_readouts_{};
        bool progression_ready_{};
        progression_input input_{};
        progression_state displayed_progression_{};
        std::string info_text_{};
        bool info_dirty_{true};
        uint8_t info_flags_{0xFF};
        char mode_summary_[80]{};
        std::array<std::array<char, 160>, 10> history_labels_{};
        experience_source source_{};
        std::vector<experience_readout_text> readout_values_{};
        std::array<float, experience_readout_count> readout_widths_{};
        std::unique_ptr<ready_check_sound> sound_{};
        uint64_t warned_chain_deadline_{};
        bool merit_alerted_{}, goal_alerted_{};

        auto SelectedTextProfile(void) -> module_text_profile&;
        auto RefreshProgression(void) -> void;
        auto BuildReadouts(const progression_state& progress, bool preview) -> void;
        auto ReadoutWidth(const experience_readout_text& readout, float measured, float scale) -> float;
        auto GoalRemaining(void) const -> std::optional<uint64_t>;
        auto Notify(IAshitaCore* core, const char* message) -> void;
    };
} // namespace xiui
