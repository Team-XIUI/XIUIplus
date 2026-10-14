#pragma once

#include "modules/module.hpp"
#include "world/world_state.hpp"

namespace xiui
{
    class world_info_module final : public hud_module
    {
    public:
        auto Id(void) const -> const char* override { return "world"; }
        auto DisplayName(void) const -> const char* override { return "World Info"; }
        auto State(void) -> module_state& override { return this->state_; }
        auto State(void) const -> const module_state& override { return this->state_; }
        auto Update(const world_state& value) -> void { this->data_ = &value; }
        auto VisitTextSettings(const text_settings_visitor& visit) -> void override
        { VisitTextProfiles(visit, this->text_); }
        auto ApplyTextDefaults(const ui::theme& theme) -> void override;
        auto ResetDefaults(const ui::theme& theme) -> void override;
        auto Load(const settings_store& store) -> void override;
        auto Save(const settings_store& store) const -> void override;
        auto OptionCount(void) const -> size_t override { return 6 + text_profile_option_count; }
        auto Option(size_t index) -> module_option override;
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ConfigSectionContains(size_t section, size_t index) -> bool override;
        auto ShouldRender(const hud_snapshot&) const -> bool override { return this->state_.enabled && this->data_ != nullptr && (this->data_->time_valid || this->data_->weather_valid); }
        auto Render(const hud_snapshot&, const module_render_context& context) -> void override;

    private:
        module_state state_{true, {-1, -1, 160, 34}, 120, 16};
        const world_state* data_{};
        std::array<module_text_profile, 3> text_{};
        bool day_{true}, weather_{true}, date_{};
        int selected_text_{};
    };
}
