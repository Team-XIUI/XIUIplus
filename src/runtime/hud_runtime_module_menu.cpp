#include "hud_runtime.hpp"

#include <cstring>
#include <memory>

namespace xiui
{
    auto hud_runtime::ModuleMenuAvailable(const hud_module& module) const -> bool
    {
        return this->visible_ && this->editing_ && module.State().enabled && module.CanMove() && module.HasEditableBounds() &&
            this->ModuleDisplayed(module) && module.PositionBoundsReady();
    }

    auto hud_runtime::OpenModuleMenu(hud_module& module, const rect& anchor) -> bool
    {
        if (!this->ModuleMenuAvailable(module)) return false;
        this->FinishScale(false); this->FinishControllerMove(false); this->FinishConfigResize(false);
        this->FinishInteraction(); this->FinishConfigInput(true, false); this->CloseProfileControls(); this->ReleaseChatInput();
        this->config_.active_color = nullptr; this->config_.header_menu_open = false;
        this->FocusModuleConfig(module);
        static constexpr const char* actions[]{"Reset position", "Reset scale", "Reset settings"};
        static constexpr const char* nativeActions[]{"Reset position", "Reset settings"};
        this->module_menu_target_ = &module; this->module_menu_selection_ = 0; this->module_menu_pressed_.reset();
        this->config_.choice_input.Open(module_option::Choice("Module actions", "", &this->module_menu_selection_,
            module.SupportsScale() ? actions : nativeActions, module.SupportsScale() ? 3 : 2), anchor);
        this->ui_sounds_.MenuOpened(::GetTickCount64());
        return true;
    }

    auto hud_runtime::TryOpenModuleMenu(const D3DVIEWPORT8& viewport) -> bool
    {
        if (!this->editing_ || this->mouse_buttons_ != 0 || this->interaction_ != interaction::none || this->ChatResizing() ||
            this->config_.choice_input.Opened() || this->ProfileSurface(viewport) || this->config_.header_menu_open || this->config_.search_open) return false;
        const auto x = static_cast<float>(this->mouse_x_), y = static_cast<float>(this->mouse_y_);
        if (Contains(this->ConfigVisibleBounds(), x, y) ||
            (this->config_.active_color != nullptr && Contains(this->ConfigColorBounds(viewport), x, y))) return false;
        if (const auto toolbar = this->MovementToolbar(viewport); toolbar && Contains(toolbar->bounds, x, y)) return false;
        if (this->actionbars_.DialogOpen() && this->ActionBarsPointerOwns(x, y)) return false;
        if (this->NativeEditMode())
        {
            auto* window = this->NativeWindowAt(x, y);
            return window != nullptr && this->OpenModuleMenu(*window, {x, y, 176, 0});
        }
        const auto& modules = this->modules_.All();
        for (auto at = modules.rbegin(); at != modules.rend(); ++at)
            if (this->ModuleMenuAvailable(**at) && Contains(this->MovementBounds(**at), x, y))
                return this->OpenModuleMenu(**at, {x, y, 176, 0});
        return false;
    }

    auto hud_runtime::ResetModule(hud_module& module, const bool settings) -> bool
    {
        if (!this->ModuleMenuAvailable(module) || this->default_settings_text_.empty()) return false;
        if (this->NativeWindowIndex(module) < native_window_count)
        {
            if (settings) module.ResetDefaults(this->design_system_.GetTheme());
            else module.OnOptionChanged(4);
            this->SaveSettings();
            return true;
        }
        auto baseline = std::make_unique<hud_runtime>();
        baseline->default_settings_text_ = this->default_settings_text_;
        baseline->LoadSettings(settings_store::FromIniText(this->default_settings_text_),
            {static_cast<int>(this->hud_viewport_.Width), static_cast<int>(this->hud_viewport_.Height)});
        baseline->EnsureLayout(this->hud_viewport_);
        for (const auto* defaults : baseline->modules_.All())
            if (std::strcmp(defaults->Id(), module.Id()) == 0)
            {
                if (settings) module.ResetSettingsFrom(*defaults, ui::theme::CreateDefault());
                else
                {
                    module.State().bounds.x = defaults->State().bounds.x;
                    module.State().bounds.y = defaults->State().bounds.y;
                    module.OnPositionChanged();
                }
                module.State().content_bounds = {};
                this->ResolveHudTransforms(this->hud_viewport_); this->NormalizeConfigSelection(); this->SaveSettings();
                return true;
            }
        return false;
    }

    auto hud_runtime::ConfirmModuleReset(hud_module& module) -> void
    {
        if (!this->ModuleMenuAvailable(module)) return;
        this->FinishConfigInput(true, false); this->CloseProfileControls();
        this->confirmation_module_ = &module;
        this->confirmation_message_ = std::string{"Module: "} + module.DisplayName() + "\nRestore this module's shipped defaults.";
        this->profile_dialog_ = profile_dialog::reset_module;
        this->profile_keyboard_selection_ = 0; this->config_.focus.modal_item = 0;
        this->profile_name_.Open(module.DisplayName());
        this->ui_sounds_.MenuOpened(::GetTickCount64());
    }
}
