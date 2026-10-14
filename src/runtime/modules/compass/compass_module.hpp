#pragma once

#include "modules/module.hpp"
#include "world/world_state.hpp"

namespace xiui
{
    class compass_module final : public hud_module
    {
    public:
        auto Id(void) const -> const char* override { return "compass"; }
        auto DisplayName(void) const -> const char* override { return "Compass"; }
        auto State(void) -> module_state& override { return this->state_; }
        auto State(void) const -> const module_state& override { return this->state_; }
        auto Update(const world_state& value) -> void { this->data_ = &value; }
        auto VisitTextSettings(const text_settings_visitor& visit) -> void override
        { VisitTextStyles(visit, {&this->labels_.style, &this->heading_text_.style}); }
        auto ApplyTextDefaults(const ui::theme& theme) -> void override;
        auto ResetDefaults(const ui::theme& theme) -> void override;
        auto Load(const settings_store& store) -> void override;
        auto Save(const settings_store& store) const -> void override;
        auto OptionCount(void) const -> size_t override { return 7 + text_profile_option_count; }
        auto Option(size_t index) -> module_option override;
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ConfigSectionContains(size_t section, size_t index) -> bool override;
        auto ShouldRender(const hud_snapshot&) const -> bool override { return this->state_.enabled && this->data_ != nullptr && this->data_->heading_valid; }
        auto Render(const hud_snapshot&, const module_render_context& context) -> void override;

    private:
        module_state state_{true, {-1, -1, 128, 67}, 80, 40};
        const world_state* data_{};
        module_text_profile labels_{}, heading_text_{};
        bool labels_visible_{true}, heading_visible_{}, markers_{true};
        float range_{50.0f}, opacity_{1.0f};
    };
}
