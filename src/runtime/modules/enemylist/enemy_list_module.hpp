#pragma once

#include "../module.hpp"
#include "enemy_source.hpp"
#include "combat/combat_source.hpp"
#include "ui/combat_timing_display.hpp"

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
        auto Load(const settings_store& store) -> void override;
        auto Save(const settings_store& store) const -> void override;
        auto OptionCount(void) const -> size_t override { return timing_options_start + ui::enemy_timing_settings::option_count; }
        auto Option(size_t index) -> module_option override;
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ConfigSectionContains(size_t section, size_t index) -> bool override;
        auto ShouldRender(const hud_snapshot&) const -> bool override { return this->state_.enabled && !this->source_.Entries().empty(); }
        auto Render(const hud_snapshot& snapshot, const module_render_context& context) -> void override;
        auto Update(IAshitaCore* core, uint64_t now) -> void;
        auto HandlePacket(IAshitaCore* core, uint16_t id, std::span<const uint8_t> data, uint64_t now) -> void;
        auto Count(void) const -> size_t { return this->source_.Entries().size(); }
        auto SetCombat(const combat_source* source) -> void { this->combat_ = source; }
        auto SetTiming(const combat_timing_source* source) -> void { this->timing_ = source; }
        auto HitTarget(float x, float y, uint64_t now) const -> std::pair<uint32_t, uint32_t>;

    private:
#if defined(XIUI_ENABLE_PARITY_TESTS)
        friend struct module_parity_test;
#endif
        module_state state_{true, {24.0f, 240.0f, 220.0f, 240.0f}, 180.0f, 50.0f};
        enemy_source source_{};
        const combat_source* combat_{};
        const combat_timing_source* timing_{};
        ui::enemy_timing_settings timers_{};
        bool timing_preview_{};
        static constexpr size_t timing_options_start = 16 + meter_style_option_count + text_profile_option_count;
        std::unordered_map<uint32_t, meter_animation_state> animations_{};
        bool distance_{true}, percent_{true}, targets_{true}, casts_{true}, include_selected_{};
        float max_rows_{8.0f}, gap_{6.0f};
        int selected_text_{};
        ui::container_style container_{.padding = 6.0f};
        ui::meter_style meter_{ui::theme::CreateDefault().meter};
        std::array<module_text_profile, 4> text_{};
        D3DCOLOR hp_top_{0xFF704242}, hp_bottom_{0xFFFB9494};
        D3DCOLOR aggro_color_{0xFFFF9B8E}, cast_color_{0xFFFFCA78};

        auto HeaderHeight(const enemy_entry& entry) const -> float;
        auto RowHeight(const enemy_entry& entry, uint64_t now) const -> float;
        auto Statuses(uint32_t id) const -> std::span<const combat_status>;
        auto Cast(const enemy_entry& entry) const -> combat_cast;
        auto DrawRow(const enemy_entry& entry, const ui::rect& bounds, uint64_t now, const module_render_context& context) -> void;
    };
} // namespace xiui
