#include "config_input_fixture.hpp"

#include <memory>

namespace xiui
{
    auto config_input_test::SetupWizard(const char* defaultsPath) -> bool
    {
        using namespace config_checks;
        using control = ui::setup_control;
        auto runtime = std::make_unique<hud_runtime>();
        auto& hud = *runtime;
        std::array<std::array<uint32_t, 11>, 189> nativeSettings{};
        std::array<uintptr_t, 2> nativeManager{0, reinterpret_cast<uintptr_t>(nativeSettings.data())};
        const auto nativeConfig = reinterpret_cast<uintptr_t>(nativeManager.data());
        hud.window_config_.config_slot_ = reinterpret_cast<uintptr_t>(&nativeConfig); hud.window_config_.setter_ = 1;
        nativeSettings[180][1] = 180; nativeSettings[180][2] = 1; nativeSettings[180][3] = 4; nativeSettings[180][7] = 2;
        std::ifstream file{defaultsPath, std::ios::binary};
        const std::string defaults{std::istreambuf_iterator<char>{file}, {}};
        if (!check(!defaults.empty(), "setup has shipped defaults")) return false;
        hud.default_settings_text_ = defaults;
        hud.LoadSettings(settings_store::FromIniText(defaults), {1280, 720});
        const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
        hud.EnsureLayout(viewport);
        hud.LoadSetup(settings_store::FromIniText(""), false);
        hud.UpdateSetup();
        if (!check(!hud.setup_.open, "existing installs are not interrupted by setup")) return false;
        hud.LoadSetup(settings_store::FromIniText(""), true); hud.UpdateSetup();
        if (!check(hud.SetupVisible() && hud.KeyboardFocused() && hud.ControllerCapture() == controller_capture_mode::exclusive,
            "first load deliberately owns keyboard and controller input")) return false;
        const uint32_t nativeMask = (1u << native_hud_options.size()) - 1;
        if (!check(std::none_of(hud.modules_.All().begin(), hud.modules_.All().end(), [&](const auto* module) { return hud.ContextVisible(*module); }) &&
            (hud.NativeHudHiddenMask() & nativeMask) == nativeMask && hud.player_.State().enabled,
            "intro hides all XIUI+ and native HUD without changing enabled preferences")) return false;
        const auto key = [&](WPARAM value, uint32_t flags = 0) { return hud.HandleSetupKey(value, flags, false); };
        key(VK_RETURN); key(VK_RETURN, 0x40000000);
        if (!check(hud.setup_.step == 1, "held confirmation advances only once")) return false;
        key(VK_RETURN, 0x80000000);
        hud.HandleController({controller_action::confirm, controller_backend::none, 0, true});
        if (!check(hud.setup_.step == 2 && hud.SetupModule() == &hud.player_, "controller and keyboard share the walkthrough")) return false;
        if (!check(hud.ContextVisible(hud.player_) && !hud.ContextVisible(hud.chat_) && !hud.ContextVisible(hud.equipment_),
            "each module step reveals only its own preview before earlier choices")) return false;
        hud.game_hud_.SetAvailable((1u << native_hud_options.size()) - 1);
        const auto playerBounds = hud.player_.State().bounds;
        hud.ActivateSetup(control::disable);
        const uint32_t playerHud = NativeHudBit(native_hud_element::player) | NativeHudBit(native_hud_element::party);
        if (!check(!hud.player_.State().enabled && (hud.game_hud_.HiddenMask() & playerHud) == 0,
            "native Player Bar restores both player gauges and the bottom-right party/status panel")) return false;
        if (!check(!hud.ContextVisible(hud.player_) && (hud.NativeHudHiddenMask() & playerHud) == 0 &&
            (hud.NativeHudHiddenMask() & NativeHudBit(native_hud_element::target)) != 0,
            "native fallback appears while unvisited native HUD stays hidden")) return false;
        hud.setup_.step = 5; hud.ActivateSetup(control::enable);
        if (!check((hud.NativeHudHiddenMask() & NativeHudBit(native_hud_element::party)) == 0 &&
            (hud.game_hud_.HiddenMask() & NativeHudBit(native_hud_element::party)) == 0,
            "choosing XIUI+ Party retains the native player status fallback")) return false;
        hud.setup_.step = 2;
        hud.ActivateSetup(control::enable);
        if (!check(hud.player_.State().enabled && hud.player_.State().bounds.x == playerBounds.x && hud.player_.State().bounds.y == playerBounds.y &&
            (hud.game_hud_.HiddenMask() & playerHud) == playerHud, "enabling both XIUI+ replacements keeps layout and hides native duplicates")) return false;
        hud.ActivateSetup(control::next); hud.ActivateSetup(control::disable);
        if (!check(hud.ContextVisible(hud.player_) && !hud.ContextVisible(hud.target_), "accepted modules stay visible while future modules stay hidden")) return false;
        const uint32_t chatMask = NativeHudBit(native_hud_element::chat1) | NativeHudBit(native_hud_element::chat2) | NativeHudBit(native_hud_element::chatlog);
        if (!check(!hud.chat_.WindowVisible() && !hud.chat_second_.WindowVisible() && (hud.game_hud_.HiddenMask() & chatMask) == 0,
            "Chat off restores both native logs and native expanded history")) return false;
        hud.ActivateSetup(control::enable);
        const auto nativeBefore = nativeSettings;
        const auto chatBounds = hud.chat_.State().bounds, secondBounds = hud.chat_second_.State().bounds;
        hud.chat_.Clear();
        chat_event chat{}; chat.sequence = 100; chat.mode = 14; chat.windows = 2; strcpy_s(chat.text, "Second-window chat");
        hud.chat_.Receive(nullptr, chat, false);
        chat.sequence = 101; chat.mode = 20; chat.windows = 1; strcpy_s(chat.text, "First-window combat");
        hud.chat_.Receive(nullptr, chat, false);
        hud.chat_.RebuildRows(hud.design_system_, 1000); hud.chat_second_.RebuildRows(hud.design_system_, 1000);
        if (!check(hud.chat_.WindowLayout() == chat_window_layout::follow_game && hud.chat_second_.State().enabled &&
            hud.chat_.rows_.size() == 1 && hud.chat_.rows_[0].sequence == 101 &&
            hud.chat_second_.rows_.size() == 1 && hud.chat_second_.rows_[0].sequence == 100,
            "setup's default follows native two-window admission without duplicating chat into combat")) return false;
        hud.setup_.focus = control::chat_combined;
        key(VK_RETURN);
        hud.chat_.RebuildRows(hud.design_system_, 1000);
        if (!check(hud.chat_.WindowLayout() == chat_window_layout::combined && !hud.chat_second_.State().enabled &&
            hud.chat_.rows_.size() == 2 && !hud.chat_.InitializeNativeWindows(2),
            "keyboard selection combines both native logs once and prevents deferred window-count initialization from undoing it")) return false;
        hud.setup_.focus = control::chat_native;
        key(VK_RETURN, 0x40000000);
        if (!check(hud.chat_.WindowLayout() == chat_window_layout::combined, "held Enter cannot select a different chat layout")) return false;
        key(VK_RETURN, 0x80000000);
        auto combined = settings_store::FromIniText(""); hud.chat_.Save(combined); hud.chat_second_.Save(combined);
        chat_module primary, secondary{true}; primary.AttachSecondWindow(secondary); primary.Load(combined); secondary.Load(combined);
        if (!check(primary.WindowLayout() == chat_window_layout::combined && !primary.InitializeNativeWindows(2),
            "the combined layout persists through reload without changing native settings")) return false;
        hud.HandleSetupAction(controller_action::confirm, false);
        if (!check(hud.chat_.WindowLayout() == chat_window_layout::follow_game && hud.chat_second_.State().enabled && nativeSettings == nativeBefore &&
            hud.chat_.State().bounds.x == chatBounds.x && hud.chat_.State().bounds.y == chatBounds.y &&
            hud.chat_second_.State().bounds.x == secondBounds.x && hud.chat_second_.State().bounds.y == secondBounds.y,
            "controller selection restores native routing while retaining positions and every native filter setting")) return false;
        nativeSettings[180][1] = 0;
        hud.ActivateSetup(control::chat_native);
        if (!check(!hud.chat_second_.State().enabled && hud.chat_.InitializeNativeWindows(2) && hud.chat_second_.State().enabled,
            "Follow game waits for unavailable native settings, then adopts the real window count")) return false;
        nativeSettings[180][1] = 180; nativeSettings[180][2] = 0;
        hud.ActivateSetup(control::chat_native);
        if (!check(!hud.chat_second_.State().enabled && hud.chat_.WindowLayout() == chat_window_layout::follow_game,
            "a native one-window layout stays one window")) return false;
        nativeSettings[180][2] = 1;
        hud.ActivateSetup(control::chat_native);
        const auto saved = hud.CaptureSettings();
        auto resumed = std::make_unique<hud_runtime>(); resumed->LoadSetup(saved, false); resumed->UpdateSetup();
        if (!check(resumed->setup_.open && resumed->setup_.step == 3 && saved.Preferences("").ReadString("session.setup", "pending").empty(),
            "unfinished setup resumes locally and stays out of shared profiles")) return false;
        if (!check(resumed->setup_.revealed == hud.setup_.revealed, "resuming setup retains its progressive reveal choices")) return false;
        const auto next = hud.SetupItems(viewport);
        const auto button = std::find_if(next.begin(), next.end(), [](const auto& item) { return item.id == control::next; })->bounds;
        hud.mouse_x_ = static_cast<int>(button.x + 4); hud.mouse_y_ = static_cast<int>(button.y + 4);
        hud.HandleSetupMouse(WM_LBUTTONDOWN, MK_LBUTTON, false, viewport);
        if (!check(hud.setup_.step == 3, "mouse activation waits for matching release")) return false;
        hud.mouse_x_ = hud.mouse_y_ = 0;
        hud.HandleSetupMouse(WM_LBUTTONUP, 0, false, viewport);
        if (!check(hud.setup_.step == 3, "releasing outside cancels a button")) return false;
        hud.setup_.step = 8; hud.setup_.focus = control::next;
        key(VK_RETURN);
        if (!check(!hud.setup_.open && hud.KeyboardFocused() && key(VK_RETURN, 0x80000000) && !hud.KeyboardFocused(),
            "Finish owns Enter until release so it cannot open native chat")) return false;
        if (!check(hud.ContextVisible(hud.experience_) && hud.NativeHudHiddenMask() ==
            (hud.game_hud_.HiddenMask() | (hud.actionbars_.ReplaceNativeMacros() ? native_keyboard_macro_palette : 0)),
            "finishing releases temporary visibility and restores normal native preferences")) return false;
        resumed->LoadSetup(hud.CaptureSettings(), false); resumed->setup_.open = false; resumed->UpdateSetup();
        if (!check(!resumed->setup_.open, "finished setup stays finished after reload")) return false;
        resumed->LoadSettings(hud.CaptureSettings());
        resumed->game_hud_.SetAvailable(0); resumed->game_hud_.ShowAll();
        *resumed->game_hud_.Option(0).toggle = false;
        resumed->player_.State().enabled = resumed->chat_.State().enabled = resumed->party_.State().enabled = true;
        resumed->general_.SetPartyEnabled(false);
        resumed->setup_.pending = resumed->setup_.open = true; resumed->setup_.revealed = 0;
        const auto acceptedBounds = resumed->player_.State().bounds;
        for (const size_t step : {size_t{2}, size_t{3}, size_t{5}})
        {
            resumed->setup_.step = step;
            resumed->ActivateSetup(control::next);
        }
        const auto replacements = playerHud | chatMask;
        if (!check(resumed->general_.PartyEnabled() && (resumed->game_hud_.HiddenMask() & replacements) == replacements &&
            !resumed->game_hud_.Option(static_cast<size_t>(native_hud_element::party) + 1).available,
            "Next accepts preselected XIUI+ replacements and saves native hides even before native controls are ready")) return false;
        resumed->game_hud_.ShowAll(); resumed->setup_.step = 8; resumed->ActivateSetup(control::next);
        const auto accepted = resumed->CaptureSettings(); resumed->LoadSettings(accepted);
        if (!check((resumed->NativeHudHiddenMask() & replacements) == replacements &&
            (resumed->NativeHudHiddenMask() & NativeHudBit(native_hud_element::target)) == 0 &&
            resumed->player_.State().bounds.x == acceptedBounds.x && resumed->player_.State().bounds.y == acceptedBounds.y,
            "Finish persists accepted native hides across reload without changing unvisited visibility or HUD positions")) return false;
        resumed->setup_.pending = resumed->setup_.open = true; resumed->setup_.revealed = 0;
        resumed->setup_.step = 3; resumed->chat_.State().enabled = false; resumed->ActivateSetup(control::next);
        resumed->FinishSetup();
        if (!check((resumed->game_hud_.HiddenMask() & chatMask) == 0 && (resumed->game_hud_.HiddenMask() & playerHud) == playerHud,
            "accepting the game chat restores all native logs while keeping earlier unvisited player and party choices")) return false;
        hud.BeginSetup();
        for (size_t step = 0; step < ui::setup_pages.size(); ++step)
        {
            hud.setup_.step = step;
            const auto panel = hud.SetupBounds(viewport);
            for (const auto& item : hud.SetupItems(viewport))
                if (!check(item.bounds.x >= panel.x && item.bounds.y >= panel.y && item.bounds.x + item.bounds.width <= panel.x + panel.width &&
                    item.bounds.y + item.bounds.height <= panel.y + panel.height, "wizard controls remain inside their panel")) return false;
        }
        hud.cast_bar_.State().enabled = true;
        if (!check(!hud.ContextVisible(hud.cast_bar_), "previously enabled optional modules stay hidden until chosen")) return false;
        hud.ActivateSetup(control::castbar);
        if (!check(hud.cast_bar_.State().enabled && hud.ContextVisible(hud.cast_bar_) && !hud.ContextVisible(hud.experience_),
            "choosing an optional module reveals it immediately without enabling others")) return false;
        hud.ActivateSetup(control::skip);
        if (!check(hud.ContextVisible(hud.experience_), "Skip releases temporary hiding without changing untouched settings")) return false;
        const auto shipped = settings_store::FromIniText(defaults);
        if (!check(shipped.ReadInt("chat", "source", -1) == 0 && shipped.ReadInt("chat2", "source", -1) == 1,
            "shipped chat sources follow the corresponding native windows")) return false;
        auto freshChat = shipped; freshChat.WriteBool("chat", "native_windows_initialized", false);
        hud.chat_.Load(freshChat); hud.chat_second_.Load(freshChat);
        if (!check(hud.chat_.InitializeNativeWindows(2) && hud.chat_second_.State().enabled && !hud.chat_.InitializeNativeWindows(1),
            "fresh chat adopts two native windows once and preserves later choices")) return false;
        hud.chat_.Load(freshChat); hud.chat_second_.Load(freshChat);
        if (!check(!hud.chat_.InitializeNativeWindows(std::nullopt) && hud.chat_.InitializeNativeWindows(1) && !hud.chat_second_.State().enabled,
            "unavailable native settings defer initialization and one native window stays one")) return false;
        hud.chat_.Load(freshChat); hud.chat_second_.State().enabled = true; hud.chat_.OnOptionChanged(11);
        if (!check(!hud.chat_.InitializeNativeWindows(1) && hud.chat_second_.State().enabled, "manual window choices cancel deferred native initialization")) return false;
        hud.chat_.Load(settings_store::FromIniText("[chat]\nenabled=1\n"));
        hud.chat_second_.State().enabled = true;
        if (!check(!hud.chat_.InitializeNativeWindows(1) && hud.chat_second_.State().enabled, "existing custom chat is never reseeded")) return false;
        std::array<std::array<uint32_t, 11>, 181> records{};
        auto& record = records[180]; record[1] = 180; record[3] = 4; record[7] = 2;
        const std::array<uintptr_t, 2> manager{0, reinterpret_cast<uintptr_t>(records.data())};
        const uintptr_t address = reinterpret_cast<uintptr_t>(manager.data());
        native_window_config native; native.config_slot_ = reinterpret_cast<uintptr_t>(&address); native.setter_ = 1;
        if (!check(native.ChatWindowCount() == 1u, "native multi-window Off maps to one window")) return false;
        for (uint32_t mode : {1u, 2u})
        {
            record[2] = mode;
            if (!check(native.ChatWindowCount() == 2u, "both native multi-window layouts map to two windows")) return false;
        }
        record[7] = 99;
        return check(!native.ChatWindowCount(), "unknown native config records remain unavailable");
    }
}
