#pragma once

#include "combat/action_packet.hpp"

#include "../module.hpp"
#include "enemy_source.hpp"
#include "combat/combat_source.hpp"
#include "ui/combat_timing_display.hpp"
#include "ui/status_strip.hpp"

#include <unordered_map>

namespace xiui
{
    class enemy_list_module final : public hud_module
    {
    public:
        auto Id(void) const -> const char* override { return "enemylist"; }
        auto DisplayName(void) const -> const char* override { return "Enemy List"; }
        auto State(void) -> module_state& override { return this->state_; }
        auto State(void) const -> const module_state& override { return this->state_; }
        auto ResetDefaults(const ui::theme& theme) -> void override;
        auto VisitTextSettings(const text_settings_visitor& visit) -> void override
        { VisitTextProfiles(visit, this->text_); VisitTextStyles(visit, {&this->timers_.text.style}); }
        auto ApplyTextDefaults(const ui::theme& theme) -> void override;
        auto ApplyMeterDefaults(const ui::meter_style& style) -> void override
        { const float height = this->meter_.height; this->meter_ = style; this->meter_.height = height; }
        auto Load(const settings_store& store) -> void override;
        auto Save(const settings_store& store) const -> void override;
        auto ContentOptionCount(void) const -> size_t override { return status_options_start + ui::status_strip_settings::option_count; }
        auto ContentOption(size_t index) -> module_option override;
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ContentConfigSectionContains(size_t section, size_t index) -> bool override;
        auto ShouldRender(const hud_snapshot&) const -> bool override { return this->state_.enabled && !this->source_.Entries().empty(); }
        auto Render(const hud_snapshot& snapshot, const module_render_context& context) -> void override;
        auto Update(IAshitaCore* core, uint64_t now) -> void;
        auto HandlePacket(IAshitaCore* core, uint16_t id, std::span<const uint8_t> data, uint64_t now, const combat_action_packet* decoded = nullptr) -> void;
        auto Count(void) const -> size_t { return this->source_.Entries().size(); }
        auto SetCombat(const combat_source* source) -> void { this->combat_ = source; }
        auto SetTiming(const combat_timing_source* source) -> void { this->timing_ = source; }
        auto HitTarget(float x, float y, uint64_t now) const -> std::pair<uint32_t, uint32_t>;
        auto BeginControllerBrowse(uint32_t preferredId = 0, uint32_t preferredIndex = 0) -> bool;
        auto MoveControllerBrowse(int direction) -> bool;
        auto MoveControllerBrowseHorizontal(int direction) -> bool;
        auto BeginControllerStatus(void) -> bool;
        auto EndControllerStatus(void) -> void;
        auto MoveControllerStatus(int direction) -> bool;
        auto ControllerStatusBounds(void) const -> ui::rect;
        auto EndControllerBrowse(void) -> void;
        auto ControllerFocused(void) const -> bool { return this->controller_browsing_ && this->ShouldRender({}); }
        auto ControllerMember(void) -> std::optional<enemy_entry>;
        auto ControllerBounds(uint64_t now = 0) const -> ui::rect;
        auto RenderStatusTooltip(const module_render_context& context, IResourceManager* resources, float x, float y, bool controller = false) const -> void;

    private:
#if defined(XIUI_ENABLE_INPUT_TESTS)
        friend struct config_input_test;
#endif
#if defined(XIUI_ENABLE_PARITY_TESTS)
        friend struct module_parity_test;
        friend struct enemy_list_layout_test;
#endif
#if defined(XIUI_ENABLE_INPUT_TESTS)
        friend struct controller_module_test;
#endif
        module_state state_{true, {24.0f, 240.0f, 220.0f, 240.0f}, 180.0f, 50.0f};
        enemy_source source_{};
        bool controller_browsing_{};
        uint32_t controller_enemy_id_{}, controller_enemy_index_{};
        bool controller_status_browsing_{};
        uint16_t controller_status_id_{};
        size_t controller_status_occurrence_{};
        const combat_source* combat_{};
        const combat_timing_source* timing_{};
        ui::enemy_timing_settings timers_{};
        bool timing_preview_{};
        static constexpr size_t timing_options_start = 16 + meter_style_option_count + text_profile_option_count;
        static constexpr size_t layout_options_start = timing_options_start + ui::enemy_timing_settings::option_count;
        static constexpr size_t layout_option_count = 12;
        static constexpr size_t status_options_start = layout_options_start + layout_option_count;
        std::unordered_map<uint32_t, meter_animation_state> animations_{};
        bool distance_{true}, percent_{true}, targets_{true}, casts_{true}, include_selected_{};
        float max_rows_{8.0f}, gap_{6.0f};
        float max_columns_{1}, column_gap_{12}, target_offset_x_{10}, target_offset_y_{}, target_width_{100};
        bool target_panel_{}, borders_{true}, claim_border_{}, click_target_{true}, target_borders_{true};
        ui::container_style target_container_{.padding = 4};
        ui::status_strip_settings statuses_{};
        ui::rect viewport_{};
        int selected_text_{};
        ui::container_style container_{.padding = 6.0f};
        ui::meter_style meter_{ui::theme::CreateDefault().meter};
        std::array<module_text_profile, 4> text_{};
        D3DCOLOR hp_top_{0xFF704242}, hp_bottom_{0xFFFB9494};
        D3DCOLOR aggro_color_{0xFFFF9B8E}, cast_color_{0xFFFFCA78};

        auto HeaderHeight(const enemy_entry& entry) const -> float;
        struct entry_layout
        {
            size_t index{}, column{};
            ui::rect bounds{}, target{}, statuses{}, footprint{};
        };
        struct entries_layout
        {
            std::array<entry_layout, enemy_source::capacity> entries{};
            size_t count{};
            ui::rect bounds{};
        };
        auto RowHeight(const enemy_entry& entry, uint64_t now, float width = 0) const -> float;
        auto StatusBounds(const enemy_entry& entry, const ui::rect& bounds) const -> ui::rect;
        auto LayoutEntries(std::span<const enemy_entry> entries, uint64_t now) const -> entries_layout;
        auto Statuses(uint32_t id) const -> std::span<const combat_status>;
        auto Cast(const enemy_entry& entry) const -> combat_cast;
        auto DrawRow(const enemy_entry& entry, const entry_layout& layout, uint64_t now, const module_render_context& context) -> void;
        auto ResolveControllerMember(void) -> const enemy_entry*;
        auto ControllerStatusIndex(std::span<const combat_status> statuses) const -> std::optional<size_t>;
        auto FocusControllerStatus(std::span<const combat_status> statuses, size_t index) -> bool;
    };
} // namespace xiui
