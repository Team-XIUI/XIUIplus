#pragma once

#include "../module.hpp"
#include "native_hud_options.hpp"
#include "native_window_module.hpp"

#include <string_view>

namespace xiui
{
    class game_hud_module final : public hud_module
    {
    public:
        auto Id(void) const -> const char* override { return "gamehud"; }
        auto DisplayName(void) const -> const char* override { return "HUD Windows"; }
        auto State(void) -> module_state& override { return this->state_; }
        auto State(void) const -> const module_state& override { return this->state_; }
        auto HasEditableBounds(void) const -> bool override { return false; }
        auto SupportsScale(void) const -> bool override { return false; }
        auto ResetDefaults(const ui::theme&) -> void override;
        auto Load(const settings_store& store) -> void override;
        auto Save(const settings_store& store) const -> void override;
        static constexpr size_t hide_all_option = native_hud_options.size() + 1;
        static constexpr size_t show_all_option = hide_all_option + 1;
        static constexpr size_t instant_macro_option = show_all_option + 1;
        static constexpr size_t reset_positions_option = instant_macro_option + 9;
        auto ContentOptionCount(void) const -> size_t override { return reset_positions_option + 1; }
        auto ContentOption(size_t index) -> module_option override;
        auto OnContentOptionChanged(size_t index) -> void override;
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ContentConfigSectionContains(size_t section, size_t option) -> bool override
        {
            if (option == instant_macro_option || option == instant_macro_option + 8) return section == 2;
            if (option == reset_positions_option) return section == 1;
            return section == (option > instant_macro_option && option < instant_macro_option + 7 ? size_t{1} : size_t{0});
        }
        auto BindNativeOptions(hud_module& names, size_t nameOption, hud_module& macros, size_t macroOption) -> void
        { this->names_ = &names; this->name_option_ = nameOption; this->macros_ = &macros; this->macro_option_ = macroOption; }
        auto SetWindows(std::span<native_window_module> windows) -> void { this->windows_ = windows; }
        auto SelectedWindow(void) const -> native_window_module* { return this->windows_.empty() ? nullptr : &this->windows_[this->selected_window_]; }
        auto SelectWindow(size_t index) -> void { this->selected_window_ = static_cast<int>(index); }
        auto Shown(size_t index) const -> bool { return this->preferences_.shown[index]; }
        auto TakeVisibilityEdits(void) -> uint32_t { const auto edits = this->visibility_edits_; this->visibility_edits_ = 0; return edits; }
        auto SetWindowShown(size_t index, bool shown) -> bool
        { if (!this->SetShown(native_hud_options[index].key, shown)) return false; this->preferences_.enabled = true; return true; }
        auto ShouldRender(const hud_snapshot&) const -> bool override { return false; }
        auto Render(const hud_snapshot&, const module_render_context&) -> void override {}
        auto SetAvailable(uint32_t mask) -> void { this->available_ = mask; }
        auto SetDefaults(const settings_store& store) -> void;
        auto HiddenMask(void) const -> uint32_t { return this->preferences_.HiddenMask(); }
        auto InstantMacroBar(void) const -> bool { return this->instant_macro_bar_; }
        auto SetShown(std::string_view key, bool shown) -> bool;
        auto HideAll(void) -> void { this->preferences_.enabled = true; this->preferences_.shown.fill(false); this->visibility_edits_ |= (uint32_t{1} << native_hud_window_count) - 1; }
        auto ShowAll(void) -> void { this->preferences_.shown.fill(true); this->visibility_edits_ |= (uint32_t{1} << native_hud_window_count) - 1; }

    private:
        module_state state_{};
        native_hud_preferences preferences_{};
        native_hud_preferences defaults_{};
        bool instant_macro_bar_{}, default_instant_macro_bar_{};
        uint32_t available_{};
        uint32_t visibility_edits_{};
        std::span<native_window_module> windows_{};
        int selected_window_{};
        hud_module* names_{};
        hud_module* macros_{};
        size_t name_option_{}, macro_option_{};
    };
} // namespace xiui
