#pragma once

#include "modules/module.hpp"
#include "modules/game_hud/native_window_module.hpp"

namespace xiui
{
    class command_menu_module final : public hud_module
    {
    public:
        auto Id(void) const -> const char* override { return "commands"; }
        auto DisplayName(void) const -> const char* override { return "Menus and Dialogs"; }
        auto HasEditableBounds(void) const -> bool override { return false; }
        auto State(void) -> module_state& override { return this->state_; }
        auto State(void) const -> const module_state& override { return this->state_; }
        auto SupportsScale(void) const -> bool override { return false; }
        auto EditGrip(void) const -> edit_grip_kind override { return edit_grip_kind::none; }
        auto ResetDefaults(const ui::theme& theme) -> void override;
        auto Load(const settings_store& store) -> void override;
        auto Save(const settings_store& store) const -> void override;
        auto ContentOptionCount(void) const -> size_t override { return 6; }
        auto ContentOption(size_t index) -> module_option override;
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ContentConfigSectionContains(size_t section, size_t index) -> bool override
        { this->SelectSection(section); return section < native_dialog_labels.size() && index < this->ContentOptionCount(); }
        auto SetWindows(std::span<native_window_module> windows) -> void { this->windows_ = windows; }
        auto SelectSection(size_t section) -> void { this->section_ = std::min(section, native_dialog_labels.size() - 1); }
        auto SelectedWindow(void) const -> native_window_module*;
        auto SelectWindow(size_t window) -> void;
        auto OnContentOptionChanged(size_t index) -> void override { if (index >= 1 && this->SelectedWindow() != nullptr) this->SelectedWindow()->OnOptionChanged(index - 1); }
        auto ShouldRender(const hud_snapshot&) const -> bool override { return false; }
        auto Render(const hud_snapshot&, const module_render_context& context) -> void override;

    private:
        module_state state_{true, {}, 32, 24};
        std::span<native_window_module> windows_{};
        size_t section_{};
        std::array<int, static_cast<size_t>(native_dialog::count)> selected_windows_{};
    };
}
