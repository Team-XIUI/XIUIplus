#include "hud/hud_runtime.hpp"

#include <cstdio>
#include <cstring>

namespace xiui
{
    auto RunControllerModuleChecks(void) -> bool;
    auto RunControllerConfigChecks(void) -> bool;
    auto RunPartyTimingChecks(void) -> bool;

    struct controller_runtime_test
    {
        static auto Check(const bool value, const char* message) -> bool
        {
            if (!value) std::fprintf(stderr, "controller runtime check failed: %s\n", message);
            return value;
        }

        static auto PartyInput(void) -> bool
        {
            using action = controller_action;
            hud_runtime runtime;
            auto& source = runtime.party_source_;
            source.player_id_ = 100;
            for (uint32_t slot = 0; slot < 3; ++slot)
            {
                auto& member = source.members_[slot];
                member.active = true; member.id = 100 + slot; member.slot = slot;
            }
            const auto send = [&](const action value, const bool repeated = false) {
                return runtime.HandleController({value, controller_backend::xinput, 0, true, repeated, false, 100});
            };
            for (const auto direction : {action::up, action::down, action::left, action::right})
            {
                if (!Check((runtime.ControllerInputState() & ControllerEntryBit(direction)) == 0,
                    "gameplay directions do not request host entry capture")) return false;
                for (const auto backend : {controller_backend::xinput, controller_backend::direct_input})
                {
                    if (!Check(!runtime.HandleController({direction, backend, 0, true, false, false, 100}) &&
                        !runtime.HandleController({direction, backend, 0, true, true, false, 200}) &&
                        !runtime.HandleController({direction, backend, 0, false, false, false, 300}) &&
                        !runtime.ControllerCaptured() && !runtime.party_.ControllerFocused(),
                        "gameplay direction press, hold and release remain native")) return false;
                }
            }
            if (!Check(runtime.BeginControllerFocus(hud_runtime::controller_surface::party) &&
                runtime.party_.ControllerMember()->id == 100, "explicit party focus starts at the first member")) return false;
            if (!Check(send(action::down, true) && runtime.party_.ControllerMember()->id == 101, "owned held direction advances party cursor")) return false;
            send(action::down); send(action::down);
            if (!Check(runtime.party_.ControllerMember()->id == 100, "Down wraps to the first party member")) return false;
            send(action::up);
            if (!Check(runtime.party_.ControllerMember()->id == 102 && !source.members_[0].selected && !source.members_[2].selected,
                "Up wraps to the last member without changing native selection")) return false;
            runtime.native_ui_.menu_visible = true;
            if (!Check(!runtime.BeginControllerFocus(hud_runtime::controller_surface::party), "native menus also block explicit party entry")) return false;
            if (!Check(!runtime.ControllerCaptured() && !send(action::down) && !runtime.party_.ControllerFocused(),
                "native menus immediately retain D-pad ownership")) return false;
            runtime.native_ui_.menu_visible = false;
            runtime.editing_ = true;
            send(action::down);
            if (!Check(!runtime.party_.ControllerFocused() && runtime.editing_, "configuration takes priority over party entry")) return false;
            runtime.editing_ = false;
            runtime.ReleaseControllerFocus();
            runtime.party_.State().enabled = false;
            if (!Check(!send(action::down) && !runtime.ControllerCaptured(), "disabled party leaves D-pad with the game")) return false;
            runtime.party_.State().enabled = true;
            source.members_[1].selected = true;
            if (!Check(runtime.BeginControllerFocus(hud_runtime::controller_surface::party) &&
                runtime.party_.ControllerMember()->id == 101, "explicit entry starts from the current native party target")) return false;
            if (!Check(send(action::down) && runtime.party_.ControllerMember()->id == 102, "explicit browsing advances from the current native party target")) return false;
            send(action::cancel);
            if (!Check(!runtime.ControllerCaptured() && !send(action::up) && !send(action::down),
                "Cancel returns party directions to native targeting")) return false;
            source.members_[1].selected = false;
            if (!Check(runtime.BeginControllerFocus(hud_runtime::controller_surface::party) && send(action::up) &&
                runtime.party_.ControllerMember()->id == 102, "explicit browsing wraps Up to the last member")) return false;
            if (!Check(send(action::cycle_window) && runtime.chat_.HistoryFocused() && !runtime.party_.ControllerFocused(),
                "cycle switches from party browsing to chat")) return false;
            send(action::down);
            return Check(runtime.chat_.HistoryFocused() && !runtime.party_.ControllerFocused(), "chat keeps its own D-pad scrolling");
        }

        static auto Run(void) -> bool
        {
            using action = controller_action;
            using surface = hud_runtime::controller_surface;
            hud_runtime runtime;
            const auto send = [&](const action value, const bool repeated = false) {
                return runtime.HandleController({value, controller_backend::xinput, 0, true, repeated, false, 100});
            };
            if (!Check(!send(action::confirm) && !runtime.ControllerCaptured(), "unfocused input remains native")) return false;
            if (!Check(!send(action::cycle_window, true) && !runtime.ControllerCaptured(), "held cycle cannot acquire chat")) return false;
            if (!Check(send(action::cycle_window) && runtime.chat_.HistoryFocused() && !runtime.chat_.HistoryExpanded(),
                "cycle first focuses compact chat without selecting it")) return false;
            if (!Check(runtime.ControllerCapture() == controller_capture_mode::navigation && !runtime.KeyboardFocused(),
                "chat history captures navigation without taking gameplay keyboard input")) return false;
            if (!Check(!runtime.HandleChatKey('W', 0, false, true, false, 0) && runtime.chat_.HistoryFocused(),
                "keyboard movement leaves chat focused and passes to the game")) return false;
            if (!Check(send(action::confirm) && runtime.chat_.HistoryExpanded(), "Confirm selects the focused chat window")) return false;
            if (!Check(runtime.ControllerCapture() == controller_capture_mode::navigation && !runtime.KeyboardFocused(),
                "expanded chat keeps gameplay input available")) return false;
            runtime.editing_ = true;
            if (!Check(runtime.ControllerCapture() == controller_capture_mode::exclusive, "configuration retains exclusive controller capture")) return false;
            runtime.editing_ = false;
            if (!Check(send(action::cycle_window) && runtime.chat_.HistoryExpanded(), "cycle stays inside selected chat")) return false;
            send(action::cancel); send(action::cancel);
            if (!Check(!runtime.ControllerCaptured(), "layered Cancel returns chat control to the game")) return false;
            runtime.chat_.State().enabled = false;
            if (!Check(!send(action::cycle_window) && !runtime.ControllerCaptured(), "disabled chat leaves cycle with the game")) return false;
            runtime.chat_.State().enabled = true;
            if (!Check(runtime.BeginControllerFocus(surface::picker) && runtime.ControllerCaptured(), "explicit focus owns input")) return false;
            if (!Check(send(action::confirm, true) && runtime.controller_surface_ == surface::picker, "held Confirm cannot activate picker")) return false;
            send(action::cancel);
            if (!Check(!runtime.ControllerCaptured(), "Cancel releases picker")) return false;

            if (!Check(runtime.BeginControllerFocus(surface::chat), "chat can acquire history focus")) return false;
            send(action::actions);
            if (!Check(runtime.chat_.ControllerComposing(), "compose opens without sending")) return false;
            if (!Check(runtime.KeyboardFocused() && runtime.ControllerCapture() == controller_capture_mode::navigation,
                "typing owns the keyboard while controller movement remains available")) return false;
            send(action::cancel);
            runtime.UpdateControllerFocus();
            if (!Check(runtime.chat_.HistoryFocused(), "compose Cancel returns to history")) return false;
            controller_status suspended{}; suspended.suspended = true;
            runtime.UpdateControllerStatus(suspended);
            if (!Check(!runtime.ControllerCaptured() && !runtime.chat_.HistoryFocused(), "suspension releases owned chat")) return false;
            runtime.UpdateControllerStatus({});

            runtime.BeginControllerFocus(surface::picker);
            runtime.SetVisible(false);
            if (!Check(!runtime.ControllerCaptured(), "hidden UI releases ownership")) return false;
            runtime.SetVisible(true);
            runtime.BeginControllerFocus(surface::picker);
            runtime.HandlePacket(0x000B, nullptr, 0);
            if (!Check(!runtime.ControllerCaptured(), "zoning releases ownership")) return false;
            runtime.BeginControllerFocus(surface::picker);
            runtime.LoadSettings(settings_store::FromIniText(""));
            if (!Check(!runtime.ControllerCaptured(), "profile changes release ownership")) return false;

            runtime.BeginControllerFocus(surface::picker);
            const D3DVIEWPORT8 viewport{0, 0, 1920, 1080, 0, 1};
            runtime.HandleControllerPointer(WM_LBUTTONDOWN, 0, 0, viewport);
            if (!Check(runtime.controller_pointer_down_ && runtime.controller_surface_ == surface::none, "outside picker press dismisses with retained gesture")) return false;
            if (!Check(runtime.HandleControllerPointer(WM_LBUTTONUP, 0, 0, viewport) && !runtime.controller_pointer_down_, "picker release cannot click through")) return false;
            if (!Check(!runtime.SelectHudTarget(1, 1), "target activation revalidates live identity")) return false;
            return true;
        }
    };
}

int main(const int argc, char** argv)
{
    if (argc == 2 && std::strcmp(argv[1], "--config-only") == 0)
    {
        if (!xiui::RunControllerConfigChecks()) return 1;
        std::puts("Controller configuration focus, shared actions, editors and input ownership passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--party-input-only") == 0)
    {
        if (!xiui::controller_runtime_test::PartyInput()) return 1;
        std::puts("Party input: native gameplay targeting and explicit browsing ownership checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--party-timing-only") == 0)
    {
        if (!xiui::RunPartyTimingChecks()) return 1;
        std::puts("Party tickers: observations, holds, lifecycle, layout and settings checks passed.");
        return 0;
    }
    if (!xiui::RunControllerModuleChecks() || !xiui::RunControllerConfigChecks() || !xiui::controller_runtime_test::Run() || !xiui::controller_runtime_test::PartyInput()) return 1;
    std::puts("XIUI+ controller module, configuration and ownership checks passed.");
    return 0;
}
