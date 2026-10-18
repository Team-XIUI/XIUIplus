#pragma once

#include "../module.hpp"
#include "native_hud_options.hpp"

#include <string_view>

namespace xiui
{
    class game_hud_module final : public hud_module
    {
    public:
        auto Id(void) const -> const char* override { return "gamehud"; }
        auto DisplayName(void) const -> const char* override { return "Game HUD"; }
        auto State(void) -> module_state& override { return this->state_; }
        auto State(void) const -> const module_state& override { return this->state_; }
        auto HasEditableBounds(void) const -> bool override { return false; }
        auto ResetDefaults(const ui::theme&) -> void override;
        auto Load(const settings_store& store) -> void override;
        auto Save(const settings_store& store) const -> void override;
        static constexpr size_t hide_all_option = native_hud_options.size() + 1;
        static constexpr size_t show_all_option = hide_all_option + 1;
        auto OptionCount(void) const -> size_t override { return show_all_option + 1; }
        auto Option(size_t index) -> module_option override;
        auto OnOptionChanged(size_t index) -> void override;
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ConfigSectionContains(size_t section, size_t option) -> bool override { return section == 0 && option < this->OptionCount(); }
        auto ShouldRender(const hud_snapshot&) const -> bool override { return false; }
        auto Render(const hud_snapshot&, const module_render_context&) -> void override {}
        auto SetAvailable(uint32_t mask) -> void { this->available_ = mask; }
        auto HiddenMask(void) const -> uint32_t { return this->preferences_.HiddenMask(); }
        auto SetShown(std::string_view key, bool shown) -> bool;
        auto HideAll(void) -> void { this->preferences_.enabled = true; this->preferences_.shown.fill(false); }
        auto ShowAll(void) -> void { this->preferences_.shown.fill(true); }

    private:
        module_state state_{};
        native_hud_preferences preferences_{};
        uint32_t available_{};
    };
} // namespace xiui
