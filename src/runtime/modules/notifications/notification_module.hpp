#pragma once

#include "../module.hpp"
#include "notifications/notification_renderer.hpp"

namespace xiui
{
    class notification_module final : public hud_module
    {
    public:
        auto Id(void) const -> const char* override { return "notifications"; }
        auto DisplayName(void) const -> const char* override { return "Notifications"; }
        auto State(void) -> module_state& override { return this->state_; }
        auto State(void) const -> const module_state& override { return this->state_; }
        auto Expire(uint64_t now) -> void { this->center_.Update(now, static_cast<uint64_t>(this->style_.duration * 1000.0f)); }
        auto Center(void) -> notification_center& { return this->center_; }
        auto Demo(uint64_t now) -> void;
        auto IsScreenAnchored(void) const -> bool { return this->screen_anchor_; }
        auto VisitTextSettings(const text_settings_visitor& visit) -> void override
        { visit(this->style_); }
        auto ResetDefaults(const ui::theme&) -> void override;
        auto Load(const settings_store& store) -> void override;
        auto Save(const settings_store& store) const -> void override;
        auto OptionCount(void) const -> size_t override { return 27; }
        auto Option(size_t index) -> module_option override;
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ConfigSectionContains(size_t section, size_t index) -> bool override;
        auto ShouldRender(const hud_snapshot&) const -> bool override;
        auto Render(const hud_snapshot& snapshot, const module_render_context& context) -> void override;

    private:
        module_state state_{true, {700.0f, 500.0f, 400.0f, 110.0f}, 220.0f, 80.0f};
        notification_center center_{};
        notification_style style_{};
        bool screen_anchor_{true};
        float offset_x_{};
        float offset_y_{};
        int anchor_{1};
    };
} // namespace xiui
