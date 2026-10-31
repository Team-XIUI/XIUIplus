#pragma once

#include "modules/module.hpp"
#include "modules/party/party_source.hpp"
#include "ui/embedded_sound_player.hpp"
#include "blue_magic_alert.hpp"

namespace xiui
{
    class blue_magic_module final : public hud_module
    {
    public:
        auto Id(void) const -> const char* override { return "bluemagic"; }
        auto DisplayName(void) const -> const char* override { return "Blue Magic Learned"; }
        auto State(void) -> module_state& override { return this->state_; }
        auto State(void) const -> const module_state& override { return this->state_; }
        auto SetSource(const party_source* source) -> void { this->source_ = source; }
        auto Update(uint64_t now, bool configOpen) -> void;
        auto HandlePacket(IAshitaCore* core, uint16_t id, std::span<const uint8_t> data, uint64_t now) -> void;
        auto Clear(void) -> void;
        auto ApplyTextDefaults(const ui::theme& theme) -> void override { this->fallback_ = {theme.title_text}; this->fallback_.style.alignment = ui::text_alignment::center; }
        auto VisitTextSettings(const text_settings_visitor& visit) -> void override { visit(this->fallback_.style); }
        auto ResetDefaults(const ui::theme& theme) -> void override;
        auto Load(const settings_store& settings) -> void override;
        auto Save(const settings_store& settings) const -> void override;
        auto ContentOptionCount(void) const -> size_t override { return base_options + text_profile_option_count; }
        auto ContentOption(size_t index) -> module_option override;
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ContentConfigSectionContains(size_t section, size_t index) -> bool override;
        auto OnContentOptionChanged(size_t index) -> void override;
        auto ShouldRender(const hud_snapshot&) const -> bool override;
        auto Render(const hud_snapshot&, const module_render_context& context) -> void override;

    private:
        enum option : size_t { enabled, scale, duration, sound, preview, test_sound, base_options };
        module_state state_{false, {-1, -1, 720, 200}, 180, 50};
        const party_source* source_{};
        blue_magic_alert alert_{};
        module_text_profile fallback_{};
        ui::embedded_sound_player audio_{};
        float scale_{1}, duration_{3};
        bool sound_{true}, audio_ready_{}, preview_active_{};
        uint64_t preview_started_{}, now_{};
        uint32_t player_id_{}, zone_{};
        uint8_t job_{}, subjob_{};
        auto PlaySound(void) -> void;
    };
}
