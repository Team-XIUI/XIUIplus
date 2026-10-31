#include "config_input_fixture.hpp"

#include <memory>

namespace xiui
{
    auto config_input_test::ModuleMenu(const char* defaultsPath) -> bool
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
        if (!check(!defaults.empty(), "module menu has shipped defaults")) return false;
        auto runtime = std::make_unique<hud_runtime>(), baseline = std::make_unique<hud_runtime>();
        const D3DVIEWPORT8 viewport{0, 0, 2560, 1440, 0, 1};
        for (auto* instance : {runtime.get(), baseline.get()})
        {
            instance->default_settings_text_ = defaults;
            instance->LoadSettings(settings_store::FromIniText(defaults), {2560, 1440});
            instance->EnsureLayout(viewport);
        }
        auto& hud = *runtime; hud.mouse_ = &input; hud.SetEditing(true, false); hud.SetBasicSettings(false, false);
        hud.config_.bounds = {1400, 20, 1100, 600};
        auto& module = hud.magic_burst_; auto& state = module.State();
        state.enabled = true; state.bounds = {600, 400, 160, 180}; state.content_bounds = {}; state.scale = {1.5f, .75f, false};
        module.VisitTextSettings([](ui::text_style& style) { style.font = 1; });
        const auto font = [](hud_module& value) {
            int result{}; value.VisitTextSettings([&](ui::text_style& style) { result = style.font; }); return result;
        };
        hud.ResolveHudTransforms(viewport);
        hud.mouse_x_ = 620; hud.mouse_y_ = 440;
        if (!check(hud.TryOpenModuleMenu(viewport) && hud.module_menu_target_ == &module,
            "right-click hit testing selects the movable module under the pointer")) return false;
        hud.CloseChoiceInput();
        const auto send = [&](uint32_t message, uint32_t buttons, const ui::rect& at) {
            hud.mouse_x_ = static_cast<int>(at.x + 4); hud.mouse_y_ = static_cast<int>(at.y + 4);
            return hud.ProcessPointerMessage(message, buttons, viewport);
        };
        const auto open = [&] { return hud.OpenModuleMenu(module, {850, 400, 176, 0}); };
        const auto click = [&](int index) {
            const auto row = hud.config_.choice_input.Row(index, viewport);
            return send(WM_LBUTTONDOWN, MK_LBUTTON, row) && send(WM_LBUTTONUP, 0, row);
        };
        if (!check(open() && hud.KeyboardFocused() && hud.config_.choice_input.Option().choice_count == 3,
            "movable elements share the existing dropdown and explicit input ownership")) return false;
        const auto row = hud.config_.choice_input.Row(1, viewport), original = state.bounds;
        if (!check(send(WM_LBUTTONDOWN, MK_LBUTTON, row) && input.blocked && state.scale.x == 1.5f,
            "reset waits for release and blocks native input while held")) return false;
        if (!check(send(WM_LBUTTONUP, 0, row) && state.scale.x == 1 && state.scale.y == 1 && state.scale.locked &&
            state.bounds.x == original.x && state.bounds.y == original.y && font(module) == 1,
            "reset scale restores local uniform size while retaining placement and appearance")) return false;
        if (!check(open() && click(0) && state.bounds.x == baseline->magic_burst_.State().bounds.x &&
            state.bounds.y == baseline->magic_burst_.State().bounds.y && state.bounds.width == 160 && font(module) == 1,
            "reset position uses fitted shipped coordinates without resetting size or text")) return false;
        state.bounds.x = 600; state.bounds.y = 400; hud.ResolveHudTransforms(viewport);
        if (!check(open(), "context menu reopens")) return false;
        const ui::rect outside{1200, 1000, 1, 1};
        if (!check(send(WM_LBUTTONDOWN, MK_LBUTTON, outside) && !hud.config_.choice_input.Opened() && input.blocked &&
            send(WM_MOUSEMOVE, MK_LBUTTON, outside) && input.blocked && send(WM_LBUTTONUP, 0, outside) && state.bounds.x == 600,
            "outside dismissal consumes the whole click without a reset or camera drag")) return false;
        if (!check(open() && click(2) && hud.profile_dialog_ == hud_runtime::profile_dialog::reset_module &&
            hud.ConfirmingAction() && hud.profile_name_.Focused() && hud.profile_keyboard_selection_ == 0 && hud.confirmation_module_ == &module,
            "reset settings opens the shared warning with the selected module and Cancel focused")) return false;
        hud.DrawProfileOverlay(viewport);
        hud.HandleControllerConfig(controller_action::confirm, false);
        if (!check(!hud.profile_name_.Focused() && font(module) == 1 && state.bounds.x == 600,
            "default confirmation cancels without changing the module")) return false;
        const auto player = hud.player_.Settings().hp_color;
        if (!check(open() && click(2), "reset confirmation can reopen")) return false;
        hud.HandleControllerConfig(controller_action::right, false);
        hud.HandleControllerConfig(controller_action::confirm, false);
        if (!check(!hud.profile_name_.Focused() && font(module) == font(baseline->magic_burst_) &&
            state.bounds.x == baseline->magic_burst_.State().bounds.x && hud.player_.Settings().hp_color == player,
            "explicit confirmation restores this module while retaining unrelated modules")) return false;
        auto& bar = hud.actionbar_children_[0];
        bar.State().enabled = true; bar.State().bounds.x = 700; bar.State().scale = {2, 2, true};
        hud.actionbars_.BarState(2).bounds.x = 901;
        bar.ResetSettingsFrom(baseline->actionbar_children_[0], ui::theme::CreateDefault());
        if (!check(bar.State().bounds.x == baseline->actionbar_children_[0].State().bounds.x &&
            bar.State().scale.x == baseline->actionbar_children_[0].State().scale.x && hud.actionbars_.BarState(2).bounds.x == 901,
            "independent hotbar reset uses its owner without resetting a sibling")) return false;
        hud.notifications_.Group(1).offset_x = 123; hud.notifications_.Group(0).offset_x = 456;
        hud.notification_groups_[0].ResetSettingsFrom(baseline->notification_groups_[0], ui::theme::CreateDefault());
        if (!check(hud.notifications_.Group(1).offset_x == baseline->notifications_.Group(1).offset_x && hud.notifications_.Group(0).offset_x == 456,
            "notification group reset retains sibling settings")) return false;
        hud.commands_.State().enabled = true;
        if (!check(hud.OpenModuleMenu(hud.commands_, {800, 400, 176, 0}) && hud.config_.choice_input.Option().choice_count == 2 &&
            std::string_view{hud.config_.choice_input.Option().choices[1]} == "Reset settings",
            "native-size Commands omit the unsupported scale action")) return false;
        hud.CloseChoiceInput();
        hud.config_.focus.region = config_controller_region::movement;
        const config_controller_item item{module.Id(), hud.MovementBounds(module)};
        if (!check(hud.ActivateControllerConfigItem(item, controller_action::actions, false) && hud.module_menu_target_ == &module,
            "controller Movement actions open the same module menu")) return false;
        hud.HandleChoiceKey(VK_ESCAPE, 0);
        if (!check(!hud.config_.choice_input.Opened() && hud.confirmation_key_release_ == VK_ESCAPE,
            "keyboard cancellation retains its owned release after the menu closes")) return false;
        hud.SetEditing(false, false);
        return check(!input.blocked, "leaving edit mode restores native mouse blocking");
    }
}
