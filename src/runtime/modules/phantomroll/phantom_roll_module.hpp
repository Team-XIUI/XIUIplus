#pragma once

#include "combat/action_packet.hpp"

#include "modules/module.hpp"
#include "combat/combat_settings.hpp"
#include "modules/party/party_source.hpp"
#include "phantom_roll_tracker.hpp"

namespace xiui
{
    class phantom_roll_module final : public hud_module
    {
    public:
        auto Id(void) const -> const char* override { return "phantomroll"; }
        auto DisplayName(void) const -> const char* override { return "Phantom Roll"; }
        auto State(void) -> module_state& override { return this->state_; }
        auto State(void) const -> const module_state& override { return this->state_; }
        auto SetSource(const party_source* source) -> void { this->source_ = source; }
        auto SetCombatSettings(combat_settings* settings) -> void { this->combat_settings_ = settings; }
        auto Rules(void) const -> server_rules { return this->CombatSettings().Rules(); }
        auto Update(uint64_t now, bool configOpen) -> void;
        auto HandlePacket(IAshitaCore* core, uint16_t id, std::span<const uint8_t> data, uint64_t now, const combat_action_packet* decoded = nullptr) -> void;
        auto Clear(void) -> void { this->tracker_.Clear(); this->preview_ = false; }
        auto ApplyMeterDefaults(const ui::meter_style& style) -> void override { this->meter_ = style; this->meter_.height = 10; }
        auto ApplyTextDefaults(const ui::theme& theme) -> void override;
        auto VisitTextSettings(const text_settings_visitor& visit) -> void override { VisitTextProfiles(visit, this->text_); }
        auto ResetDefaults(const ui::theme& theme) -> void override;
        auto Load(const settings_store& settings) -> void override;
        auto Save(const settings_store& settings) const -> void override;
        auto ContentOptionCount(void) const -> size_t override { return base_options + meter_style_option_count + text_profile_option_count; }
        auto ContentOption(size_t index) -> module_option override;
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ContentConfigSectionContains(size_t section, size_t index) -> bool override;
        auto OnContentOptionChanged(size_t index) -> void override;
        auto ShouldRender(const hud_snapshot&) const -> bool override;
        auto Render(const hud_snapshot&, const module_render_context& context) -> void override;

    private:
        enum option : size_t { enabled, rules, preview, layout, width, scale, container, opacity, padding,
            potency, double_up, lucky_color, unlucky_color, bust_color, fill_top, fill_bottom, selected_text, base_options };
        module_state state_{false, {-1, -1, 480, 172}, 260, 60};
        const party_source* source_{};
        phantom_roll_tracker tracker_{};
        bool preview_{}, container_{true}, potency_{true}, double_up_{true};
        int layout_{}, selected_text_{};
        combat_settings local_combat_settings_{};
        combat_settings* combat_settings_{};
        float scale_{1};
        ui::container_style container_style_{0.95f, 5, 10, false};
        ui::meter_style meter_{ui::theme::CreateDefault().meter};
        std::array<module_text_profile, 5> text_{};
        std::array<meter_animation_state, 3> animation_{};
        D3DCOLOR lucky_{0xFFFFCF7D}, unlucky_{0xFF91C7EF}, bust_{0xFFFF8D88}, top_{0xFF4C9B95}, bottom_{0xFF9CDAC5};
        uint32_t player_id_{}, zone_{};
        uint8_t job_{}, subjob_{}, level_{};
        uint64_t now_{};
        auto CombatSettings(void) -> combat_settings& { return this->combat_settings_ != nullptr ? *this->combat_settings_ : this->local_combat_settings_; }
        auto CombatSettings(void) const -> const combat_settings& { return this->combat_settings_ != nullptr ? *this->combat_settings_ : this->local_combat_settings_; }
    };
}
