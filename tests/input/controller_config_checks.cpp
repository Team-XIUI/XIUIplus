#include "hud_runtime.hpp"

#include <cstdio>
#include <cmath>

namespace
{
    auto Check(const bool value, const char* message) -> bool
    { if (!value) std::fprintf(stderr, "controller config check failed: %s\n", message); return value; }
}

namespace xiui
{
    struct controller_config_test
    {
        static auto Run(void) -> bool
        {
            using action = controller_action;
            using region = config_controller_region;
            config_controller_focus stable;
            std::vector<config_controller_item> items{{"first", {}, 0, ""}, {"selected", {}, 1, ""}, {"last", {}, 2, ""}};
            stable.item = 1; stable.Resolve(items);
            std::swap(items[0], items[1]); stable.Resolve(items);
            if (!Check(stable.item == 0 && stable.id == "selected", "focus follows identity when rows reorder")) return false;
            items.erase(items.begin()); stable.Resolve(items);
            if (!Check(stable.item == 0 && stable.id == "first", "removed focus recovers to an available row")) return false;

            hud_runtime runtime;
            runtime.editing_ = true;
            runtime.config_.bounds = {30, 30, 1100, 600};
            runtime.config_.module = &runtime.general_;
            runtime.config_.section = general_module::input_section;
            runtime.general_.SetInputMode(2);
            runtime.config_.focus.region = region::options;
            runtime.RefreshControllerConfigFocus();
            auto controls = runtime.ControllerConfigItems(region::options);
            if (!Check(controls.size() == 1, "read-only device diagnostics do not become focus stops")) return false;
            runtime.HandleControllerConfig(action::confirm, true);
            if (!Check(!runtime.config_.choice_input.Opened(), "held confirmation cannot open or activate a control")) return false;
            runtime.HandleControllerConfig(action::confirm, false);
            runtime.HandleControllerConfig(action::up, false);
            runtime.HandleControllerConfig(action::confirm, false);
            if (!Check(runtime.general_.InputMode() == 1 && !runtime.config_.choice_input.Opened(), "mode uses the shared dropdown and applies once")) return false;
            const auto originalBounds = runtime.player_.State().bounds;
            auto preferences = settings_store::FromIniText(""); runtime.general_.Save(preferences);
            general_module restored; restored.Load(preferences);
            if (!Check(restored.InputMode() == 1 && runtime.player_.State().bounds.x == originalBounds.x && runtime.player_.State().bounds.y == originalBounds.y,
                "presentation persists without moving the HUD")) return false;

            runtime.config_.section = general_module::grid_section;
            runtime.config_.focus.id.clear(); runtime.config_.focus.item = 0;
            runtime.RefreshControllerConfigFocus();
            const bool initialGrid = runtime.general_.Grid().visible;
            runtime.HandleControllerConfig(action::confirm, false);
            runtime.HandleControllerConfig(action::confirm, true);
            if (!Check(runtime.general_.Grid().visible != initialGrid, "toggle changes once per fresh confirmation")) return false;
            controls = runtime.ControllerConfigItems(region::options);
            const auto snap = std::find_if(controls.begin(), controls.end(), [&](const auto& item) { return item.index == runtime.general_.GridStart() + 3; });
            if (!Check(snap != controls.end(), "grid snap slider is reachable")) return false;
            runtime.config_.focus.id = snap->id;
            runtime.RefreshControllerConfigFocus();
            const float before = runtime.general_.Grid().amount;
            runtime.HandleControllerConfig(action::confirm, false);
            runtime.HandleControllerConfig(action::right, false);
            runtime.HandleControllerConfig(action::cancel, false);
            if (!Check(runtime.editing_ && !runtime.config_.focus.adjusting && runtime.general_.Grid().amount == before + 1,
                "adjustment changes one step and Cancel returns to navigation")) return false;

            runtime.config_.module = &runtime.experience_;
            runtime.config_.section = 1;
            runtime.experience_display_.SelectView(experience_display_view::readouts);
            runtime.config_.focus.id.clear(); runtime.config_.focus.item = 0;
            controls = runtime.ControllerConfigItems(region::options);
            const auto add = std::find_if(controls.begin(), controls.end(), [](const auto& item) { return item.index >= 0x200; });
            if (!Check(add != controls.end(), "available readouts are controller focus stops")) return false;
            const auto readout = add->index - 0x200;
            runtime.config_.focus.id = add->id;
            runtime.HandleControllerConfig(action::confirm, false);
            if (!Check(runtime.experience_.Settings().readouts[readout], "controller adds through the same readout action as pointer input")) return false;
            runtime.RefreshControllerConfigFocus();
            controls = runtime.ControllerConfigItems(region::options);
            const auto focused = controls[runtime.config_.focus.item];
            if (!Check(focused.index == 0x100 + readout, "readout focus survives moving from available to shown")) return false;
            runtime.HandleControllerConfig(action::confirm, false);
            controls = runtime.ControllerConfigItems(region::options);
            const auto remove = std::find_if(controls.begin(), controls.end(), [](const auto& item) { return item.index == 22; });
            if (!Check(remove != controls.end(), "readout remove action is reachable")) return false;
            runtime.config_.focus.id = remove->id;
            runtime.HandleControllerConfig(action::confirm, false);
            if (!Check(!runtime.experience_.Settings().readouts[readout], "readout removal uses its selected identity")) return false;

            runtime.config_.module = &runtime.general_;
            runtime.config_.section = general_module::appearance_section;
            runtime.general_.SelectAppearanceTab(1);
            runtime.config_.bounds.height = ui::config_layout::minimum_height;
            controls = runtime.ControllerConfigItems(region::options);
            if (!Check(controls.size() > static_cast<size_t>(runtime.ConfigLayout().VisibleRows() * 2), "long option list exercises scrolling")) return false;
            runtime.config_.focus.id = controls.back().id;
            runtime.RefreshControllerConfigFocus();
            controls = runtime.ControllerConfigItems(region::options);
            const auto at = controls[runtime.config_.focus.item].bounds;
            const auto area = runtime.ConfigLayout().ContentArea();
            if (!Check(runtime.config_.scroll > 0 && at.y >= area.y && at.y + at.height <= area.y + area.height,
                "focused long-list option scrolls into view")) return false;

            runtime.config_.bounds.height = 600;
            runtime.general_.SelectAppearanceTab(3);
            controls = runtime.ControllerConfigItems(region::options);
            if (!Check(controls.size() == ui::controller_focus_settings::option_count && runtime.general_.FocusStyle().thickness == 2,
                "controller focus appearance has its own controls and defaults to two pixels")) return false;
            const auto borderWidth = std::find_if(controls.begin(), controls.end(), [&](const auto& item) { return item.index == runtime.general_.FocusStart() + 1; });
            if (!Check(borderWidth != controls.end(), "controller can reach the border thickness slider")) return false;
            runtime.config_.focus.id = borderWidth->id;
            runtime.RefreshControllerConfigFocus();
            runtime.HandleControllerConfig(action::confirm, false);
            runtime.HandleControllerConfig(action::right, false);
            runtime.HandleControllerConfig(action::cancel, false);
            *runtime.general_.Option(runtime.general_.FocusStart()).color = 0x8F80B0FF;
            auto focusSettings = settings_store::FromIniText("");
            runtime.general_.Save(focusSettings);
            restored.Load(focusSettings.Preferences(""));
            if (!Check(restored.FocusStyle().thickness == 2.5f && restored.FocusStyle().color == 0x8F80B0FF &&
                restored.FocusStyle().glow && restored.FocusStyle().shadow && restored.FocusStyle().shadow_y == 1,
                "focus styling, effects and color alpha survive portable profile settings")) return false;

            D3DCOLOR color = 0xFF406080;
            runtime.config_.active_color = &color; runtime.config_.color_picker.Open(color);
            runtime.HandleControllerConfig(action::right, false);
            runtime.HandleControllerConfig(action::actions, false);
            if (!Check(color == 0xFF406080, "color restore retains the original value through controller edits")) return false;
            runtime.HandleControllerConfig(action::cancel, false);
            runtime.profile_name_.Open("Delete me"); runtime.profile_dialog_ = hud_runtime::profile_dialog::delete_profile;
            runtime.config_.focus.modal_item = 1; runtime.config_.focus.modal_id.clear();
            runtime.HandleControllerConfig(action::confirm, false);
            if (!Check(!runtime.profile_name_.Focused() && runtime.editing_, "new destructive dialog defaults to Cancel before fresh activation")) return false;

            const auto flag = runtime.general_.Option(runtime.general_.GridStart() + 2);
            *flag.toggle = false;
            auto& playerBounds = runtime.player_.State().bounds;
            playerBounds = {240, 160, playerBounds.width, playerBounds.height};
            const auto beforeMove = playerBounds;
            runtime.cast_cost_.State().enabled = false;
            const auto hiddenBounds = runtime.cast_cost_.State().bounds;
            runtime.config_.focus.region = region::movement;
            runtime.config_.focus.id = runtime.player_.Id();
            runtime.config_.focus.viewport = {0, 0, 1920, 1080, 0, 1};
            runtime.RefreshControllerConfigFocus();
            const auto movable = runtime.ControllerMovableModules();
            if (!Check(!movable.empty() && std::all_of(movable.begin(), movable.end(), [](const auto* module) {
                return module->State().enabled && module->HasEditableBounds() && module->PreviewVisible() && module->PositionBoundsReady();
            }), "movement list skips disabled and unavailable previews")) return false;
            runtime.HandleControllerConfig(action::confirm, false);
            runtime.HandleControllerConfig(action::right, false);
            if (!Check(runtime.config_.focus.moving && runtime.config_.collapsed && playerBounds.x == beforeMove.x + runtime.general_.Grid().amount,
                "Move starts deliberately and nudges by the configured snap amount")) return false;
            if (!Check(runtime.cast_cost_.State().bounds.x == hiddenBounds.x && runtime.cast_cost_.State().bounds.y == hiddenBounds.y,
                "nudging one module leaves disabled module positions unchanged")) return false;
            const auto previewSettings = runtime.CaptureSettings();
            if (!Check(previewSettings.ReadFloat("layout.player", "x", -1) == beforeMove.x, "automatic saves retain the committed position during a move preview")) return false;
            runtime.HandleControllerConfig(action::cancel, false);
            if (!Check(!runtime.config_.focus.moving && !runtime.config_.collapsed && playerBounds.x == beforeMove.x && playerBounds.y == beforeMove.y,
                "Cancel restores pre-move bounds and the expanded config")) return false;
            runtime.HandleControllerConfig(action::confirm, false);
            runtime.HandleControllerConfig(action::actions, false);
            runtime.HandleControllerConfig(action::down, false);
            runtime.HandleControllerConfig(action::confirm, true);
            if (!Check(runtime.config_.focus.moving && playerBounds.y == beforeMove.y + 1, "fine movement uses one pixel and held confirmation cannot commit")) return false;
            runtime.HandleControllerConfig(action::confirm, false);
            if (!Check(!runtime.config_.focus.moving && playerBounds.y == beforeMove.y + 1 && runtime.CaptureSettings().ReadFloat("layout.player", "y", -1) == beforeMove.y + 1,
                "fresh Confirm commits the normal saved module position")) return false;
            runtime.HandleControllerConfig(action::confirm, false);
            runtime.HandleControllerConfig(action::next_tab, false);
            const auto centeredX = runtime.MovementBounds(runtime.player_);
            runtime.HandleControllerConfig(action::previous_tab, false);
            const auto centeredY = runtime.MovementBounds(runtime.player_);
            if (!Check(centeredX.x == std::round((1920 - centeredX.width) * 0.5f) && centeredY.y == std::round((1080 - centeredY.height) * 0.5f),
                "controller centering reuses visual bounds for both axes")) return false;
            runtime.HandleControllerConfig(action::cancel, false);
            const auto itemsBeforeDisable = runtime.ControllerConfigItems(region::movement);
            runtime.player_.State().enabled = false;
            runtime.RefreshControllerConfigFocus();
            const auto afterDisable = runtime.ControllerConfigItems(region::movement);
            if (!Check(afterDisable.size() + 1 == itemsBeforeDisable.size() && runtime.config_.focus.id != runtime.player_.Id(),
                "disabled previews leave the movement list and focus recovers")) return false;
            runtime.editing_ = false;
            return true;
        }
    };

    auto RunControllerConfigChecks(void) -> bool { return controller_config_test::Run(); }
}
