#pragma once

#include "../module.hpp"
#include "party_source.hpp"
#include "modules/healing/heal_prediction.hpp"
#include "party_timing_settings.hpp"
#include "combat/party_timing_source.hpp"
#include "modules/castcost/mp_cost_preview.hpp"
#include "ui/status_strip.hpp"
#include "ui/status_classification.hpp"

namespace xiui
{
    class party_module final : public hud_module
    {
    public:
        explicit party_module(size_t group = 0);
        auto Id(void) const -> const char* override;
        auto DisplayName(void) const -> const char* override;
        auto State(void) -> module_state& override { return this->state_; }
        auto State(void) const -> const module_state& override { return this->state_; }
        auto SetSource(const party_source* source) -> void { this->source_ = source; }
        auto SetTiming(const party_timing_source* source) -> void { this->timing_ = source; }
        auto SetHealPrediction(heal_prediction_source* source) -> void { this->heal_prediction_ = source; }
        auto SetMPPreview(const cast_cost_source* source) -> void { this->mp_preview_source_ = source; }
        auto SetPresetAlliances(party_module& first, party_module& second) -> void { this->preset_alliances_ = {&first, &second}; }
        auto SetModuleEnabled(const bool* enabled) -> void { this->module_enabled_ = enabled; }
        auto ApplyMeterDefaults(const ui::meter_style& style) -> void override;
        auto VisitTextSettings(const text_settings_visitor& visit) -> void override
        { VisitTextProfiles(visit, this->text_); this->tickers_.VisitTextSettings(visit); }
        auto ApplyTextDefaults(const ui::theme& theme) -> void override;
        auto ResetDefaults(const ui::theme& theme) -> void override;
        auto ApplyPreset(size_t preset) -> void;
        auto CopyPreferences(const party_module& party) -> void;
        auto PreviewVisible(void) const -> bool override { return this->ModuleEnabled() && this->state_.enabled && (this->group_ == 0 || !this->hide_preview_); }
        auto Load(const settings_store& settings) -> void override;
        auto Save(const settings_store& settings) const -> void override;
        auto OptionCount(void) const -> size_t override { return job_text_options_start + 2; }
        auto Option(size_t index) -> module_option override;
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ConfigSectionContains(size_t section, size_t index) -> bool override;
        auto OnOptionChanged(size_t index) -> void override;
        auto ShouldRender(const hud_snapshot&) const -> bool override;
        auto Render(const hud_snapshot& snapshot, const module_render_context& context) -> void override;
        auto Count(void) const -> size_t { return this->source_ != nullptr ? this->source_->Count(this->group_) : 0; }
        auto VisualBounds(bool editing) const -> ui::rect;
        auto PlacementBounds(bool editing) const -> ui::rect;
        auto DescribeLayout(void) const -> std::string;
        auto UseCompactLayout(void) -> void;
        auto HitTarget(float x, float y) const -> std::pair<uint32_t, uint32_t>;
        auto GroupIndex(void) const -> size_t { return this->group_; }
        auto BeginControllerBrowse(uint32_t preferredId = 0, uint32_t preferredSlot = UINT32_MAX) -> bool;
        auto MoveControllerBrowse(int direction) -> bool;
        auto EndControllerBrowse(void) -> void;
        auto ControllerFocused(void) const -> bool { return this->controller_browsing_ && this->ShouldRender({}); }
        auto ControllerMember(void) -> std::optional<party_member>;
        auto ControllerBounds(void) const -> ui::rect;
        auto RenderStatusTooltip(const module_render_context& context, IResourceManager* resources, float x, float y, bool controller = false) const -> void;
        auto BeginControllerStatus(void) -> bool;
        auto MoveControllerStatus(int direction) -> bool;
        auto EndControllerStatus(void) -> void { this->controller_status_id_ = 0; this->controller_status_occurrence_ = 0; this->controller_status_active_ = false; }
        auto ControllerStatusBounds(void) const -> ui::rect;

    private:
#if defined(XIUI_ENABLE_PARITY_TESTS)
        friend struct module_parity_test;
        friend struct status_display_test;
#endif
#if defined(XIUI_ENABLE_INVENTORY_TESTS)
        friend struct inventory_test;
#endif
#if defined(XIUI_ENABLE_INPUT_TESTS)
        friend struct config_input_test;
        friend struct controller_module_test;
        friend struct party_timing_test;
#endif
        static constexpr size_t base_option_count = 64;
        static constexpr size_t icon_follow_option = base_option_count + meter_style_option_count + text_profile_option_count;
        static constexpr size_t sync_option = icon_follow_option + 1;
        static constexpr size_t ticker_options_start = sync_option + 1;
        static constexpr size_t status_options_start = ticker_options_start + party_timing_settings::option_count;
        static constexpr size_t status_preview_option = status_options_start + 6 + ui::status_strip_settings::option_count;
        static constexpr size_t icon_offset_options_start = status_preview_option + 1;
        static constexpr size_t mp_preview_option = icon_offset_options_start + 2;
        static constexpr size_t status_theme_option = mp_preview_option + 1;
        static constexpr size_t heal_options_start = status_theme_option + 1;
        static constexpr size_t preset_options_start = heal_options_start + heal_preview_settings::option_count;
        static constexpr size_t job_text_options_start = preset_options_start + 2;
        std::array<party_module*, 2> preset_alliances_{};
        static constexpr size_t heading_text = 6;
        static constexpr size_t tp_text = 9;
        static constexpr size_t mp_text = 10;
        static constexpr size_t zone_text = 11;
        enum config_section : size_t { general_section, layout_section, jobs_section, resources_section, bar_style_section,
            animation_section, bar_effects_section, colors_section, status_section, text_section, selection_section, tickers_section, window_section, section_count };
        static constexpr int status_left = 0, status_right = 1, status_inline = 2;
        static constexpr int cast_separate = 0, cast_name = 1, cast_mp = 2, cast_tp = 3;
        static constexpr int jobs_name = 0, jobs_below = 1;
        static constexpr int job_anchor_name = 0, job_anchor_hp = 1;
        static constexpr int bars_horizontal = 0, bars_vertical = 1, bars_mp_below_tp_left = 2;
        static constexpr int position_layout = 0, position_left = 1, position_center = 2, position_right = 3;
        size_t group_{};
        const party_source* source_{};
        const party_timing_source* timing_{};
        const cast_cost_source* mp_preview_source_{};
        heal_prediction_source* heal_prediction_{};
        heal_preview_settings heal_preview_{};
        bool mp_cost_preview_{true};
        party_timing_settings tickers_{};
        const bool* module_enabled_{};
        bool controller_browsing_{};
        uint32_t controller_member_id_{}, controller_member_slot_{UINT32_MAX};
        auto ModuleEnabled(void) const -> bool { return this->module_enabled_ == nullptr || *this->module_enabled_; }
        module_state state_{true, {900, 240, 420, 110}, 360, 80, 800};
        ui::container_style container_{};
        ui::container_style status_container_{1, 5, 4, true};
        bool status_frame_{};
        int status_placement_{status_left};
        float status_columns_{8}, status_spacing_{2}, status_gap_{8}, status_row_spacing_{2};
        int status_order_{}, selected_status_group_{};
        int status_theme_{};
        float split_status_gap_{4};
        bool status_timers_{}, status_tooltips_{true}, controller_status_active_{};
        uint16_t controller_status_id_{};
        size_t controller_status_occurrence_{};
        std::array<ui::status_strip_settings, 2> split_statuses_{};
        mutable std::vector<ui::status_tooltip> status_hits_{};
        bool solo_{true}, preview_{true}, title_{true}, distance_{true}, job_icons_{true};
        bool hide_preview_{};
        bool status_preview_{};
        bool icon_follow_name_{}, show_sync_{true};
        bool jobs_{true}, main_level_{true}, sub_job_{true}, sub_level_{true}, master_level_{true};
        bool tp_{true}, flash_tp_{true}, always_mp_{false}, buffs_{true}, casts_{true}, selection_{true};
        ui::selection_style selection_style_{};
        bool labels_{};
        bool fit_width_{true};
        bool bottom_{false}, expand_{false}, expand_alliance_{false};
        int layout_{}, hp_display_{2}, mp_display_{0}, selected_bar_{}, style_mode_{}, selected_text_{};
        int cast_placement_{cast_separate};
        int job_placement_{jobs_name};
        int job_anchor_{job_anchor_name};
        bool stack_jobs_{};
        int tp_display_{};
        float spacing_{8}, minimum_rows_{1}, icon_size_{32}, buff_size_{18}, buff_limit_{12}, distance_highlight_{20.8f};
        float icon_offset_x_{}, icon_offset_y_{};
        std::array<ui::meter_style, 4> meters_{};
        std::array<float, 4> bar_widths_{};
        std::array<int, 4> bar_positions_{};
        std::array<float, 4> bar_offsets_x_{}, bar_offsets_y_{};
        std::array<module_text_profile, 12> text_{};
        std::array<D3DCOLOR, 4> fills_{0xFFFF797D, 0xFFBFE07D, 0xFF78C4EE, 0xFF9CCEFF};
        std::array<D3DCOLOR, 4> fill_tops_{0xFF723638, 0xFF556438, 0xFF36586B, 0xFF465D73};
        D3DCOLOR low_hp_{0xFFFF8C8C}, range_color_{0xFF91D7FF};
        D3DCOLOR ready_tp_{0xFFD6EDFF};
        struct row_animation { uint32_t id{}; std::array<meter_animation_state, 3> meters{}; };
        std::array<row_animation, 6> animations_{};
        struct status_layout
        {
            ui::rect bounds{}, content{};
            size_t count{}, overflow{}, columns{};
            bool framed{}, reverse{};
        };
        struct split_status_layout { ui::status_strip_layout strip{}; ui::rect frame{}; float bottom{}; };
        struct status_group { std::array<combat_status, 32> effects{}; size_t count{}; };
        auto StatusGroups(const party_member& member) const -> std::array<status_group, 2>;
        auto SplitStatusLayouts(const ui::rect& row, const ui::rect& party, const std::array<size_t, 2>& counts) const -> std::array<split_status_layout, 2>;
        struct resource_slots { bool mp{}, tp{}, mp_values{}, tp_values{}, casting{}, swing{}, resting{}; };
        struct resource_layout { std::array<ui::rect, 3> bars{}; ui::rect ticker_text{}; float height{}; };
        struct entry_layout
        {
            ui::rect header{}, jobs{}, zone{}, cast{}, icon{};
            resource_layout resources{};
            std::array<ui::rect, 2> tickers{};
            float height{};
        };
        struct container_layout { float width{}, canvas_width{}, inset{}; };
        struct row_layout { resource_slots slots{}; entry_layout entry{}; std::string detail{}; };
        struct party_layout
        {
            ui::rect bounds{};
            std::array<float, 6> heights{};
            std::array<row_layout, 6> entries{};
            size_t rows{};
            float leading_space{};
            float content_width{}, content_inset{}, right_overflow{};
        };
        auto ResourceSlots(const party_member& member) const -> resource_slots;
        auto Ticker(const party_member& member, size_t kind, uint64_t now) const -> attack_prediction;
        auto LayoutTickerText(resource_layout& layout, const resource_slots& slots, float top) const -> void;
        auto DrawTickers(const party_member& member, const entry_layout& entry, const resource_slots& slots, uint64_t now, const module_render_context& context) const -> void;
        static auto LineHeight(const module_text_profile& profile) -> float;
        auto LayoutEntry(const ui::rect& bounds, bool inZone, const resource_slots& slots, bool details = true) const -> entry_layout;
        auto MemberHeight(const party_member& member) const -> float;
        auto DetailText(const party_member& member) const -> std::string;
        auto JobIconBounds(const entry_layout& entry, const ui::rect& name, float leaderSpace) const -> ui::rect;
        auto JobTextBounds(const ui::rect& header, const ui::rect& name, float right) const -> ui::rect;
        auto HPJobTextBounds(const ui::rect& hp) const -> ui::rect;
        auto JobTextHeight(void) const -> float;
        auto JobLines(const party_member& member) const -> std::array<std::string,2>;
        auto NameCastArea(const ui::rect& bounds, float nameX) const -> ui::rect;
        auto TPNumberBounds(const ui::rect& bounds) const -> ui::rect;
        auto CastHeight(void) const -> float;
        auto HeaderHeight(bool casting = false, bool details = true, bool inZone = true) const -> float;
        auto HeaderPadding(bool casting = false) const -> float;
        auto ResourceBarHeight(bool hasMP = true, bool hasTP = true, bool casting = false) const -> float;
        auto BarWidthLimit(void) const -> float;
        auto SelectionPadding(void) const -> ui::point;
        auto BarWidth(size_t bar, float available) const -> float;
        auto PositionBar(size_t bar, const ui::rect& bounds, const ui::rect& area) const -> ui::rect;
        auto CastBounds(const ui::rect& bounds) const -> ui::rect;
        auto LayoutResources(const ui::rect& bounds, const resource_slots& slots) const -> resource_layout;
        auto LayoutContainer(void) const -> container_layout;
        auto EntryHeight(bool inZone = true, bool hasMP = true, bool hasTP = true) const -> float;
        auto ZoneStyle(void) const -> ui::container_style;
        auto ZoneInfoHeight(void) const -> float;
        auto RowHeight(bool inZone = true, size_t statusCount = 0, bool hasMP = true, bool hasTP = true) const -> float;
        auto StatusHeight(float entry, bool inZone, size_t statusCount, const party_member* member = nullptr) const -> float;
        auto PreviewMember(size_t row, const party_member* live = nullptr) const -> party_member;
        auto StatusPreview(bool editing) const -> bool { return editing && this->group_ == 0 && this->status_preview_; }
        auto DisplayCount(bool editing) const -> size_t
        { return editing && this->preview_ ? 6 : std::max(this->Count(), this->StatusPreview(editing) ? size_t{1} : size_t{0}); }
        auto DisplayMembers(size_t count, bool preview, bool statusPreview) const -> std::array<party_member, 6>;
        auto LayoutRows(size_t count, bool preview = false, bool statusPreview = false) const -> party_layout;
        auto LayoutRows(std::span<const party_member> members, bool statusPreview) const -> party_layout;
        auto VisualBounds(const party_layout& layout) const -> ui::rect;
        auto BuffColumns(float width) const -> size_t;
        auto PartyBounds(size_t count) const -> ui::rect;
        auto StatusLayout(const ui::rect& row, const ui::rect& party, size_t count) const -> status_layout;
        auto StatusIconBounds(const status_layout& layout, size_t index) const -> ui::rect;
        auto DrawStatuses(const party_member& member, const ui::rect& row, const ui::rect& party, const module_render_context& context) const -> void;
        auto DrawCast(const party_cast& cast, const ui::rect& bounds, uint64_t now, const module_render_context& context) const -> void;
        auto DrawRow(const party_member& member, const row_layout& prepared, const ui::rect& bounds, const ui::rect& party, size_t row, uint64_t now, const module_render_context& context) -> void;
        auto Section(const char* prefix, const char* suffix = nullptr) const -> std::string;
        auto ResolveControllerMember(void) -> const party_member*;
    };
}
