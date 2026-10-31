#include "config_input_fixture.hpp"

#include <memory>

namespace xiui
{
    auto config_input_test::EditMode(const char* defaultsPath) -> bool
    {
        using namespace config_checks;
        class mouse final : public IMouse
        {
        public:
            bool blocked{};
            void AddCallback(const char*, mousecallback_f) override {}
            void RemoveCallback(const char*) override {}
            bool GetBlockInput(void) const override { return this->blocked; }
            void SetBlockInput(bool value) override { this->blocked = value; }
        } input;
        std::ifstream file{defaultsPath, std::ios::binary};
        const std::string defaults{std::istreambuf_iterator<char>{file}, {}};
        if (!check(!defaults.empty(), "edit mode has shipped defaults")) return false;
        auto runtime = std::make_unique<hud_runtime>();
        auto& hud = *runtime;
        hud.LoadSettings(settings_store::FromIniText(defaults), {2560, 1440});
        const D3DVIEWPORT8 viewport{0, 0, 2560, 1440, 0, 1};
        hud.EnsureLayout(viewport);
        hud.mouse_ = &input;
        hud.satchel_.State().enabled = hud.enemy_list_.State().enabled = true;
        hud.player_.State().enabled = true;
        hud.config_.module = &hud.enemy_list_;
        hud.SetEditing(true, false);
        hud.SetBasicSettings(false, false);
        hud.mouse_x_ = 2500; hud.mouse_y_ = 1400;
        const auto visibleToController = [&](const hud_module& module) {
            const auto modules = hud.ControllerMovableModules();
            return std::find(modules.begin(), modules.end(), &module) != modules.end();
        };
        if (!check(!hud.config_.edit_mode && !hud.ModuleDisplayed(hud.satchel_) && !hud.ModuleDisplayed(hud.enemy_list_) &&
            !hud.ModuleMenuAvailable(hud.enemy_list_) && !visibleToController(hud.enemy_list_) && !hud.MovementToolbar(viewport),
            "opening settings hides inactive modules, controls and remembered-page previews")) return false;
        const auto hiddenGrip = hud.ScaleGrip(hud.enemy_list_);
        if (!check(hud.EditGripAt(hiddenGrip.x + 4, hiddenGrip.y + 4) != &hud.enemy_list_,
            "hidden modules do not retain scale hit targets")) return false;
        if (!check(hud.ModuleDisplayed(hud.player_) && !hud.ConfigPreview(hud.player_) && !hud.ModuleEditControlsVisible(hud.player_),
            "live modules retain live content without always-visible edit controls")) return false;
        hud.config_.bounds = {10, 10, 1100, 600};
        const auto player = hud.MovementBounds(hud.player_);
        hud.mouse_x_ = static_cast<int>(player.x + player.width * .5f);
        hud.mouse_y_ = static_cast<int>(player.y + player.height * .5f);
        if (!check(hud.ModuleEditControlsVisible(hud.player_) && hud.ModuleMenuAvailable(hud.player_),
            "hover exposes quick editing for a live module with Edit Mode off")) return false;
        hud.SelectConfigModule(&hud.enemy_list_);
        if (!check(hud.ModuleDisplayed(hud.enemy_list_) && hud.ConfigPreview(hud.enemy_list_) &&
            visibleToController(hud.enemy_list_) && !hud.ModuleDisplayed(hud.satchel_) && !hud.ConfigPreview(hud.player_),
            "selecting settings previews only that module and exposes its shared editing actions")) return false;
        hud.SelectConfigModule(&hud.satchel_);
        if (!check(hud.ModuleDisplayed(hud.satchel_) && !hud.ModuleDisplayed(hud.enemy_list_),
            "changing settings removes the previous inactive preview")) return false;
        const auto toggle = hud.ConfigLayout().EditModeButton();
        hud.mouse_x_ = static_cast<int>(toggle.x + 20); hud.mouse_y_ = static_cast<int>(toggle.y + 12);
        if (!check(hud.ProcessConfigClick(viewport) && hud.config_.edit_mode && hud.ModuleDisplayed(hud.enemy_list_) &&
            hud.ModuleDisplayed(hud.satchel_), "pointer toggle explicitly previews all enabled modules")) return false;
        hud.config_.focus.active = true;
        hud.config_.focus.viewport = viewport;
        hud.config_.focus.region = config_controller_region::utilities;
        hud.config_.focus.id = "edit-mode";
        hud.controller_surface_ = hud_runtime::controller_surface::config;
        hud.HandleControllerConfig(controller_action::confirm, true);
        if (!check(hud.config_.edit_mode, "held Confirm cannot toggle Edit Mode repeatedly")) return false;
        hud.HandleControllerConfig(controller_action::confirm, false);
        if (!check(!hud.config_.edit_mode && !hud.ModuleDisplayed(hud.enemy_list_) && !hud.ModuleDisplayed(hud.satchel_),
            "controller uses the same toggle and turning it off clears inactive previews")) return false;
        hud.ToggleLayoutEditMode();
        hud.SetEditing(false, false);
        hud.SetEditing(true, false);
        if (!check(!hud.config_.edit_mode && !hud.config_.preview_selected,
            "reopening settings starts with previews off without changing enabled modules")) return false;
        hud.SetEditing(false, false);
        hud.mouse_ = nullptr;
        return true;
    }
}
