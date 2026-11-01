#pragma once

#include "target_module.hpp"

namespace xiui
{
    class target_child_module final : public hud_module
    {
    public:
        target_child_module(target_module& parent, bool subtarget) : parent_(parent), subtarget_(subtarget) {}
        auto Id(void) const -> const char* override { return this->subtarget_ ? "subtarget" : "targetrecipient"; }
        auto DisplayName(void) const -> const char* override { return this->subtarget_ ? "Subtarget" : "Target of Target"; }
        auto State(void) -> module_state& override;
        auto State(void) const -> const module_state& override;
        auto HasEditableBounds(void) const -> bool override { return this->parent_.ChildIndependent(this->subtarget_); }
        auto PreviewVisible(void) const -> bool override { return this->HasEditableBounds() && this->parent_.ChildVisible(this->subtarget_, true); }
        auto PositionBoundsReady(void) const -> bool override { return this->PreviewVisible(); }
        auto SetDisplayAllowed(bool allowed) -> void;
        auto ResetDefaults(const ui::theme&) -> void override;
        auto ResetSettingsFrom(const hud_module& defaults, const ui::theme&) -> void override;
        auto Load(const settings_store& store) -> void override;
        auto Save(const settings_store& store) const -> void override;
        auto ContentOptionCount(void) const -> size_t override;
        auto ContentOption(size_t index) -> module_option override { return this->parent_.ChildOption(this->subtarget_, index); }
        auto OnContentOptionChanged(size_t index) -> void override { if (!this->subtarget_) this->parent_.Settings().recipient.OnOptionChanged(index); }
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ContentConfigSectionContains(size_t section, size_t index) -> bool override { return this->parent_.ChildContains(this->subtarget_, section, index); }
        auto ShouldRender(const hud_snapshot&) const -> bool override { return this->HasEditableBounds() && this->parent_.ChildVisible(this->subtarget_, false); }
        auto Render(const hud_snapshot&, const module_render_context& context) -> void override { this->parent_.RenderChild(this->subtarget_, context); }

    private:
        target_module& parent_;
        bool subtarget_{};
        auto LayoutSection(void) const -> const char* { return this->subtarget_ ? "layout.subtarget" : "layout.targetrecipient"; }
    };
}
