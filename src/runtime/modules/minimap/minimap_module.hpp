#pragma once

#include "modules/module.hpp"
#include "minimap_maps.hpp"
#include "minimap_model.hpp"

namespace xiui
{
    class minimap_module final : public hud_module
    {
    public:
        auto Id(void) const -> const char* override { return "minimap"; }
        auto DisplayName(void) const -> const char* override { return "Minimap"; }
        auto State(void) -> module_state& override { return this->state_; }
        auto State(void) const -> const module_state& override { return this->state_; }
        auto SetSource(const minimap_state& state, minimap_maps& maps) -> void { this->data_ = &state; this->maps_ = &maps; }
        auto VisitTextSettings(const text_settings_visitor& visit) -> void override { VisitTextStyles(visit, {&this->text_.style, &this->zoom_text_.style}); }
        auto ApplyTextDefaults(const ui::theme& theme) -> void override;
        auto ResetDefaults(const ui::theme& theme) -> void override;
        auto Load(const settings_store& store) -> void override;
        auto Save(const settings_store& store) const -> void override;
        auto OptionCount(void) const -> size_t override { return 35 + text_profile_option_count; }
        auto NpcsEnabled(void) const -> bool { return this->npcs_; }
        auto DistantPartyEnabled(void) const -> bool { return this->distant_party_; }
        auto Option(size_t index) -> module_option override;
        auto OnOptionChanged(size_t index) -> void override;
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ConfigSectionContains(size_t section, size_t index) -> bool override;
        auto HandleMouseWheel(int delta, float x, float y, bool& save) -> bool;
        auto ShouldRender(const hud_snapshot&) const -> bool override { return this->state_.enabled && this->data_ != nullptr && this->data_->zone != 0; }
        auto Render(const hud_snapshot&, const module_render_context& context) -> void override;
    private:
        module_state state_{true, {-1, -1, 220, 244}, 120, 80, 600};
        const minimap_state* data_{};
        minimap_maps* maps_{};
        int shape_{}, artwork_{1}, selected_text_{};
        float zoom_{2}, default_zoom_{2}, map_opacity_{1}, background_opacity_{0.8f}, frame_opacity_{1}, marker_size_{4}, marker_opacity_{1};
        bool frame_{true}, background_{true}, party_{true}, alliance_{true}, pet_{true}, target_{true}, zone_name_{true}, coordinates_{true}, north_{true};
        bool map_number_{true}, zoom_readout_{true};
        bool distant_party_{true};
        bool vision_cone_{}, npcs_{true};
        float cone_angle_{80}, cone_size_{0.9f}, cone_opacity_{0.2f};
        bool rounded_corners_{}, shadow_{};
        float corner_radius_{12}, shadow_opacity_{0.45f}, shadow_softness_{6}, shadow_x_{2}, shadow_y_{3};
        std::array<float, 1024> zone_zoom_{};
        uint16_t zoom_zone_{};
        module_text_profile text_{}, zoom_text_{};
        auto UpdateZoomZone(void) -> void;
    };
}
