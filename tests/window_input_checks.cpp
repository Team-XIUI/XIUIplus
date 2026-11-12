#include "config_input_fixture.hpp"

#include <memory>

namespace xiui
{
    auto config_input_test::WindowInput(void) -> bool
    {
        using namespace config_checks;
        if (!check(ui::color_picker::ParseHex("#12ab34") == 0xFF12AB34 &&
            ui::color_picker::ParseHex("12AB3480") == 0x8012AB34 &&
            !ui::color_picker::ParseHex("#xyzxyz") && !ui::color_picker::ParseHex("#12345"),
            "hex colors accept RGB/RGBA with optional # and reject invalid input")) return false;
        auto runtime = std::make_unique<hud_runtime>(); auto& hud = *runtime;
        hud.editing_ = true; hud.config_.bounds = {40, 40, 1000, 600};
        hud.config_.module = &hud.player_;
        auto& color = hud.player_.Settings().hp_color;
        hud.config_.active_color = &color; hud.config_.color_picker.Open(color);
        const D3DVIEWPORT8 viewport{0, 0, 1600, 900, 0, 1};
        const ui::color_picker_layout layout{hud.ConfigLayout().ColorEditor(viewport)};
        const auto hex = layout.Hex();
        hud.mouse_x_ = static_cast<int>(hex.x + 3); hud.mouse_y_ = static_cast<int>(hex.y + 3);
        hud.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
        hud.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
        if (!check(hud.config_.color_picker.EditingHex() && hud.KeyboardFocused(), "clicking hex opens the shared keyboard editor")) return false;
        hud.config_.color_picker.HexInput().Insert("#1256AB80");
        hud.HandleControllerConfig(controller_action::confirm, false);
        if (!check(color == 0x801256AB && !hud.config_.color_picker.EditingHex(), "Confirm applies typed RGBA through the shared color action")) return false;
        hud.config_.color_picker.BeginHex(); hud.config_.color_picker.HexInput().Insert("invalid");
        if (!check(!hud.config_.color_picker.CommitHex() && color == 0x801256AB && hud.config_.color_picker.EditingHex(),
            "invalid hex retains the previous color and keeps the editor open")) return false;
        hud.HandleControllerConfig(controller_action::cancel, false);
        if (!check(!hud.config_.color_picker.EditingHex() && hud.config_.active_color == &color &&
            ui::color_picker::HitTest(hud.ConfigLayout().ColorEditor(viewport), layout.Copy().x + 2, layout.Copy().y + 2) == ui::color_picker::part::copy,
            "Cancel leaves the picker open and copying has its own control")) return false;
        hud.config_.active_color = nullptr; hud.editing_ = false;
        hud.chat_.AdoptDraft({"/t Friend keep this message", 13});
        hud.controller_surface_ = hud_runtime::controller_surface::chat;
        runtime_state before{}; hud.CaptureChat(before);
        hud.SuspendWindowInput();
        controller_status suspended{}; suspended.suspended = true; hud.UpdateControllerStatus(suspended);
        hud.UpdateControllerFocus();
        runtime_state after{}; hud.CaptureChat(after);
        if (!check(hud.chat_.Focused() && !hud.KeyboardFocused() && hud.ControllerCapture() == controller_capture_mode::none &&
            std::strcmp(before.chat_draft, after.chat_draft) == 0 && before.chat_caret == after.chat_caret,
            "foreground suspension releases input while retaining composition, draft and caret")) return false;
        hud.UpdateVisibilityContext(); hud.UpdateControllerStatus({});
        if (!check(hud.chat_.Focused() && hud.KeyboardFocused(), "returning to the game resumes the existing chat focus")) return false;
        hud.chat_.BeginHistory(true);
        hud.chat_.HandleControllerAction(controller_action::confirm, false, hud.design_system_);
        hud.chat_.history_view_.scroll.fill(45);
        hud.chat_.HandleControllerAction(controller_action::cancel, false, hud.design_system_);
        hud.chat_.HandleControllerAction(controller_action::confirm, false, hud.design_system_);
        if (!check(hud.chat_.HistoryExpanded() && std::all_of(hud.chat_.history_view_.scroll.begin(), hud.chat_.history_view_.scroll.end(), [](size_t value) { return value == 0; }),
            "each expanded-history opening starts at newest messages for every history tab")) return false;
        hud.chat_.HandleControllerAction(controller_action::cancel, false, hud.design_system_);
        hud.SuspendWindowInput(); hud.UpdateControllerFocus();
        if (!check(hud.chat_.HistoryFocused(), "window selector survives foreground loss")) return false;
        hud.UpdateVisibilityContext();
        std::array<uint8_t, 0x100> primitive{}, resource{};
        auto primitivePointer = reinterpret_cast<uintptr_t>(primitive.data());
        const auto resourcePointer = reinterpret_cast<uintptr_t>(resource.data());
        std::memcpy(primitive.data() + 4, &resourcePointer, sizeof(resourcePointer));
        hud.visibility_source_.menu_slot_ = reinterpret_cast<uintptr_t>(&primitivePointer);
        hud.native_ui_.menu_visible = true;
        hud.game_hud_.Load(settings_store::FromIniText("[game_hud]\nenabled=1\nshow_chat1=0\nshow_chatlog=0\nshow_status=0\n"));
        const auto focus = [&](std::string_view name) {
            std::memset(resource.data() + 0x46, ' ', 16);
            std::memcpy(resource.data() + 0x46, "menu    ", 8);
            std::memcpy(resource.data() + 0x4E, name.data(), std::min<size_t>(8, name.size()));
            hud.UpdateVisibilityContext();
        };
        for (const auto name : {"equip", "itemctrl"})
        {
            focus(name);
            if (!check(hud.NativeWindowCycle() && !hud.ControllerCaptured() && (hud.ControllerInputState() & controller_native_cycle) != 0,
                "native Equipment/storage and inventory/sort retain their original cycle control")) return false;
        }
        std::array<std::array<uint32_t, 11>, 67> settings{};
        std::array<uintptr_t, 2> manager{0, reinterpret_cast<uintptr_t>(settings.data())};
        const auto config = reinterpret_cast<uintptr_t>(manager.data());
        hud.window_config_.config_slot_ = reinterpret_cast<uintptr_t>(&config); hud.window_config_.setter_ = 1;
        auto& keyboard = settings[66]; keyboard[1] = 66; keyboard[2] = 2; keyboard[3] = 4; keyboard[7] = 2;
        hud.ReleaseChatInput();
        focus("playermo");
        hud.chat_capture_available_ = true;
        if (!check(hud.visibility_context_.window_focus == native_window_focus::commands && !hud.NativeWindowCycle() &&
            (hud.ControllerInputState() & controller_native_cycle) == 0 && hud.KeyboardInputState() == (keyboard_capture_cycle | keyboard_capture_numpad_cycle),
            "the combat command menu leaves XIUI+'s window cycle available")) return false;
        if (!check(hud.HandleChatKey('F', 0, false, true, false, 0) && hud.chat_.HistoryFocused() && !hud.chat_.Focused(),
            "F selects chat history over the combat command menu without opening composition")) return false;
        if (!check(hud.KeyboardInputState() == keyboard_capture_history && !hud.KeyboardFocused(),
            "chat reserves navigation for DirectInput without capturing unrelated movement keys")) return false;
        const auto channelDraft = hud.chat_.editor_.Text();
        const auto channelCaret = hud.chat_.editor_.Caret();
        const auto channel = hud.chat_.channel_;
        hud.chat_.tabs_[hud.chat_.active_tab_].scroll = 3;
        for (unsigned poll = 0; poll < 2; ++poll)
        {
            focus("chatctrl");
            if (!check(hud.visibility_context_.window_focus == native_window_focus::chatmode && !hud.ChatFocused() &&
                hud.KeyboardInputState() == 0 && hud.ControllerCapture() == controller_capture_mode::none,
                "the native chat-mode selector releases compact-log navigation on every poll")) return false;
            for (const WPARAM key : {VK_TAB, VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT, VK_PRIOR, VK_NEXT, VK_HOME, VK_END, VK_RETURN, VK_ESCAPE})
                if (!check(!hud.HandleChatKey(key, 0, false, true, false, 0),
                    "chat-mode selection keeps its navigation and confirmation native")) return false;
        }
        if (!check(hud.chat_.editor_.Text() == channelDraft && hud.chat_.editor_.Caret() == channelCaret &&
            hud.chat_.channel_ == channel && hud.chat_.tabs_[hud.chat_.active_tab_].scroll == 3,
            "native channel selection preserves the draft, caret, send channel and compact scroll")) return false;
        focus("playermo");
        if (!check(hud.HandleChatKey('F', 0, false, true, false, 0) && hud.KeyboardInputState() == keyboard_capture_history,
            "closing the chat-mode selector permits deliberate log browsing again")) return false;
        focus("playermo"); hud.SyncNativeChatInput(nullptr, true); hud.UpdateControllerFocus();
        if (!check(hud.chat_.HistoryFocused() && !hud.HandleChatKey(VK_RETURN, 0, true, true, false, 0) && !hud.chat_.HistoryExpanded(),
            "combat menu polling retains chat focus while prior input owners still block Enter")) return false;
        if (!check(hud.HandleChatKey(VK_RETURN, 0, false, true, false, 0) && hud.chat_.HistoryExpanded() &&
            hud.confirmation_key_release_ == VK_RETURN && hud.HandleKey(VK_RETURN, 0x40000000, false) && hud.chat_.HistoryExpanded() &&
            hud.HandleKey(VK_RETURN, static_cast<LPARAM>(0xC0000000u), false) && hud.confirmation_key_release_ == 0,
            "F then Enter expands the chat log once and owns Enter through its release")) return false;
        hud.chat_second_.State().enabled = true;
        for (size_t tab = 1; tab < std::size(chat_history_tabs); ++tab)
        {
            if (!check(hud.HandleChatKey('F', 0, false, true, false, 0) && hud.chat_.history_view_.tab == tab,
                "F selects every expanded-log tab including Window 2, Shout and Emotes")) return false;
            focus(tab % 2 == 0 ? "equip" : "itemctrl");
            if (!check(hud.chat_.HistoryExpanded() && hud.chat_.history_view_.tab == tab,
                "native focus polling cannot steal the expanded log or reset its selected channel")) return false;
        }
        if (!check(hud.HandleChatKey('F', 0, false, true, false, 0) && hud.chat_.history_view_.tab == 0,
            "expanded-log F wraps from Emotes back to Window 1")) return false;
        const auto savedMask = hud.game_hud_.HiddenMask();
        const auto savedEffectiveMask = hud.NativeHudHiddenMask();
        focus("fulllog");
        if (!check(!hud.ChatFocused() && hud.KeyboardInputState() == 0 &&
            (hud.NativeHudHiddenMask() & NativeHudBit(native_hud_element::chatlog)) == 0 && hud.game_hud_.HiddenMask() == savedMask,
            "an active native full log releases XIUI+ input and remains visible without changing saved HUD preferences")) return false;
        focus("playermo");
        if (!check(hud.KeyboardInputState() == (keyboard_capture_cycle | keyboard_capture_numpad_cycle) && hud.NativeHudHiddenMask() == savedEffectiveMask,
            "closing the native log restores ordinary keyboard entry and the configured HUD mask")) return false;
        hud.ReleaseChatInput();
        focus("playermo");
        if (!check(hud.HandleController({controller_action::cycle_window, controller_backend::xinput, 0, true, false, false, 1}) &&
            hud.chat_.HistoryFocused() && hud.HandleController({controller_action::confirm, controller_backend::xinput, 0, true, false, false, 2}) &&
            hud.chat_.HistoryExpanded(), "controller cycle and Confirm use the same combat-menu chat entry")) return false;
        hud.ReleaseChatInput();
        hud.keyboard_window_focus_ = true;
        focus("logwindo");
        controller_status keyboardOnly{}; keyboardOnly.enumeration_disabled = true;
        hud.UpdateControllerStatus(keyboardOnly);
        if (!check(hud.chat_.HistoryFocused() && hud.keyboard_window_focus_, "native F cycling retains keyboard focus with controller enumeration disabled")) return false;
        hud.UpdateControllerStatus({});
        if (!check(hud.chat_.HistoryFocused() && hud.ControllerCapture() == controller_capture_mode::navigation &&
            !hud.HandleController({controller_action::cycle_window, controller_backend::xinput, 0, true, false, false, 1}),
            "native focus on hidden chat mirrors XIUI+ browsing and leaves native cycling available")) return false;
        hud.HandleController({controller_action::confirm, controller_backend::xinput, 0, true, false, false, 2});
        if (!check(hud.chat_.HistoryExpanded(), "chat can expand while the native combat/menu UI remains open")) return false;
        hud.HandleController({controller_action::cancel, controller_backend::xinput, 0, true, false, false, 3});
        hud.SuspendWindowInput(); focus("equip");
        if (!check(hud.chat_.HistoryFocused(), "foreground resume preserves the selector if native focus changed while inactive")) return false;
        focus("itemctrl");
        return check(!hud.chat_.HistoryFocused() && !hud.ControllerCaptured() && hud.NativeWindowCycle(),
            "returning to native Storage/Sort releases chat confirmation and direction ownership");
    }
}
