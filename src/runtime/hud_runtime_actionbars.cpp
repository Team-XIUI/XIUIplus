#include "hud_runtime.hpp"
#include "ashita_compat.hpp"
#include "input/keyboard_policy.hpp"
#include "native_target.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace xiui
{
    auto hud_runtime::ActionBarsNativePaletteOwned(const bool candidate) const -> bool
    {
        return (candidate || this->actionbars_.ReplaceNativeMacros() || this->actionbars_.NativeMacroKeysOwned()) && this->visibility_context_.macro_available &&
            this->visibility_context_.macro_palette && this->visibility_context_.menu_available && this->visibility_context_.window_focus == native_window_focus::other;
    }

    auto hud_runtime::ActionBarsNativeMenuAllowed(const bool keyboardReplacement) const -> bool
    {
        return (this->visibility_context_.menu_available && this->visibility_context_.window_focus == native_window_focus::commands &&
            !this->visibility_context_.menu_open) || this->ActionBarsNativePaletteOwned(keyboardReplacement);
    }

    auto hud_runtime::PrepareActionBarsExecutor(const bool keyboardReplacement) -> void
    {
        this->action_executor_.SetNativeMenuAllowed(this->ActionBarsNativeMenuAllowed(keyboardReplacement));
        const auto* memory = this->core_ != nullptr ? this->core_->GetMemoryManager() : nullptr;
        this->action_executor_.SetNativeInputBusy(this->visibility_context_.event_active || this->party_source_.InCutscene() ||
            this->visibility_context_.map_open || this->visibility_context_.interface_hidden || NativeTargetSelectionActive(memory) ||
            (this->ControllerNativeMenuOpen() && !this->ActionBarsNativeMenuAllowed(keyboardReplacement)));
        if (this->ActionBarsExecutionAvailable(keyboardReplacement, true)) { this->action_executor_.SetRestricted(false); return; }
        std::string reason = "Macro cancelled because the action input context changed.";
        if (!this->GameWindowFocused()) reason = "Macro cancelled because the game window lost focus.";
        else if (this->editing_) reason = "Macro cancelled because Configuration opened.";
        else if (this->actionbars_.DialogOpen()) reason = "Macro cancelled because an action-bar editor opened.";
        else if (this->ControllerNativeMenuOpen() && !this->ActionBarsNativeMenuAllowed(keyboardReplacement))
            reason = "Macro cancelled because a native menu opened: " + std::string{reinterpret_cast<const char*>(&this->visibility_context_.window_focus_name), 8};
        this->action_executor_.SetRestricted(true, reason);
    }

    auto hud_runtime::NativeHudHiddenMask(void) const -> uint32_t
    {
        auto mask = this->visible_ ? this->game_hud_.HiddenMask() : (uint32_t{1} << native_hud_options.size()) - 1;
        if (this->SetupVisible()) mask = this->SetupNativeHudMask();
        // NPC dialogue uses the native logs even when XIUI replaces normal chat.
        if (this->visible_ && this->visibility_context_.event_active)
            mask &= ~(NativeHudBit(native_hud_element::chat1) | NativeHudBit(native_hud_element::chat2) | NativeHudBit(native_hud_element::chatlog));
        if (this->visible_ && this->visibility_context_.window_focus == native_window_focus::chatlog)
            mask &= ~NativeHudBit(native_hud_element::chatlog);
        return mask | (this->actionbars_.ReplaceNativeMacros() ? native_keyboard_macro_palette :
            this->game_hud_.InstantMacroBar() ? native_instant_macro_palette : 0);
    }

    auto hud_runtime::ActionBarsInputAvailable(const bool keyboardReplacement, const bool pointer) const -> bool
    {
        return this->visible_ && !this->SetupVisible() && this->GameWindowFocused() &&
            !this->ControllerNativeBusy() && (pointer || !this->ControllerNativeMenuOpen() || this->ActionBarsNativeMenuAllowed(keyboardReplacement)) && !this->satchel_.InputFocused() &&
            this->controller_surface_ != controller_surface::picker && !this->controller_pointer_down_ &&
            !this->ChatFocused() && !this->chat_.HistoryExpanded() && !this->chat_second_.HistoryExpanded();
    }

    auto hud_runtime::ActionBarsPointerOwns(const float x, const float y) const -> bool
    {
        if (!this->ActionBarsInputAvailable(false, true)) return false;
        if (this->actionbar_pointer_release_ != 0 || this->actionbars_.PointerDown()) return true;
        if (this->editing_ && (this->mouse_down_ || this->interaction_ != interaction::none || this->profile_name_.Focused() || this->profile_dropdown_ ||
            this->profile_actions_ || this->config_.choice_input.Opened() || this->config_.search_open || this->config_.header_menu_open || this->config_.active_color != nullptr)) return false;
        if (this->actionbars_.ModalDialogOpen()) return this->actionbars_.InputFocused();
        if (!this->actionbars_.FloatingWindowOpen())
            return this->actionbars_.InputFocused() || (!this->editing_ && this->actionbars_.PointerOwns(x, y));
        return this->actionbars_.PointerOwns(x, y) &&
            (!this->editing_ || this->actionbars_.WindowFocused() || !Contains(this->ConfigVisibleBounds(), x, y));
    }

    auto hud_runtime::ActionBarsExecutionAvailable(const bool keyboardReplacement, const bool continuingMacro) const -> bool
    {
        const auto* memory = this->core_ != nullptr ? this->core_->GetMemoryManager() : nullptr;
        const auto player = memory != nullptr ? xiui::ashita::View(memory->GetPlayer()) : nullptr;
        const auto* chat = this->core_ != nullptr ? this->core_->GetChatManager() : nullptr;
        const bool nativeEvent = this->visibility_context_.event_active || this->party_source_.InCutscene();
        return this->visible_ && !this->editing_ && this->GameWindowFocused() && player != nullptr &&
            player->GetLoginStatus() == static_cast<uint8_t>(Ashita::FFXI::Enums::LoginStatus::LoggedIn) && !player->GetIsZoning() &&
            (chat == nullptr || !chat->IsInputOpen()) && !this->ChatFocused() &&
            !this->chat_.HistoryExpanded() && !this->chat_second_.HistoryExpanded() && !this->satchel_.InputFocused() &&
            !this->actionbars_.DialogOpen() && !this->actionbars_.KeyboardFocused() &&
            (this->controller_surface_ == controller_surface::none || this->controller_surface_ == controller_surface::hotbar || this->controller_surface_ == controller_surface::crossbar) &&
            (!this->ControllerNativeMenuOpen() || this->ActionBarsNativeMenuAllowed(keyboardReplacement) || continuingMacro) &&
            (!nativeEvent || continuingMacro) && (!this->visibility_context_.map_open || continuingMacro) &&
            (!this->visibility_context_.interface_hidden || continuingMacro);
    }

    auto hud_runtime::CrossbarSettings(void) const -> controller_crossbar_settings
    {
        auto settings = this->actionbars_.ControllerBindings();
        settings.allowed_menu_slot = 0;
        settings.enabled = settings.enabled && !this->SetupVisible() && this->actionbars_.ReplaceNativeMacros() && this->ActionBarsExecutionAvailable() && !this->actionbars_.InputFocused() && !this->ControllerNativeBusy() &&
            this->ContextVisible(this->actionbar_children_.back()) && this->actionbars_.BarState(action_bar_count).enabled;
        settings.suppress_native_macros = this->actionbars_.ReplaceNativeMacros() && !this->SetupVisible() && this->visible_ && this->GameWindowFocused() &&
            !this->ControllerNativeBusy() && (!this->ControllerNativeMenuOpen() || this->ActionBarsNativeMenuAllowed()) &&
            !this->editing_ && !this->visibility_context_.event_active && !this->visibility_context_.map_open &&
            !this->visibility_context_.interface_hidden && !this->party_source_.InCutscene();
        if ((settings.enabled || settings.suppress_native_macros) && this->ActionBarsNativeMenuAllowed())
            settings.allowed_menu_slot = this->visibility_context_.window_focus_slot;
        return settings;
    }

    auto hud_runtime::SuspendActionBarsInput(const bool closeWindow) -> void
    {
        const bool pointer = this->actionbars_.PointerDown();
        if (pointer) this->actionbar_pointer_release_ |= MK_LBUTTON;
        if (!closeWindow && this->actionbars_.FloatingWindowOpen())
        {
            this->actionbars_.CancelPointerGesture();
            this->actionbars_.SetWindowFocused(false);
        }
        else this->actionbars_.SuspendInput();
        this->actionbars_.SetHover(-1, -1);
        this->action_executor_.Cancel("Macro cancelled because action-bar input was suspended.");
        if (pointer) { this->ReleasePointerCapture(); this->SetChatMouseCapture(false); }
    }

    auto hud_runtime::FinishActionBarsLaunch(void) -> void
    {
        this->FinishConfigInput(true, false);
        this->FinishInteraction();
        this->mouse_down_ = false; this->mouse_buttons_ = 0;
        this->ReleasePointerCapture(); this->mouse_captured_ = false;
        this->actionbars_.SetWindowFocused(true);
    }

    auto hud_runtime::UpdateActionBars(const uint64_t now) -> void
    {
        this->action_executor_.SetUtc(this->experience_.Utc());
        const auto* memory = this->core_ != nullptr ? this->core_->GetMemoryManager() : nullptr;
        const auto player = memory != nullptr ? xiui::ashita::View(memory->GetPlayer()) : nullptr;
        const auto& pet = this->pet_source_.State();
        std::string petName = pet.ready ? pet.pet.name : std::string{};
        if (!petName.empty())
        {
            if (pet.kind == pet_kind::wyvern) petName = "Wyvern";
            else if (pet.kind == pet_kind::automaton) petName = "Automaton";
            else if (pet.kind == pet_kind::charm) petName = "Charmed";
        }
        this->action_store_.SetContext({player != nullptr ? player->GetMainJob() : uint8_t{},
            player != nullptr ? player->GetSubJob() : uint8_t{}, std::move(petName)});
        std::array<bool, action_bar_count + 1> available{};
        available[0] = this->ContextVisible(this->actionbars_);
        for (size_t i = 0; i < this->actionbar_children_.size(); ++i)
            available[i + 1] = this->ContextVisible(this->actionbar_children_[i]);
        this->actionbars_.SetSurfaceAvailability(available);
        this->actionbars_.SetPreview(this->ConfigPreview(this->actionbar_children_.back()));
        this->actionbars_.SetFocusStyle(this->general_.FocusStyle());
        this->actionbars_.SetConfigText(&this->general_.ConfigText());
        if (this->actionbars_.PointerDown())
        {
            const auto* properties = this->core_ != nullptr ? this->core_->GetProperties() : nullptr;
            const auto window = properties != nullptr ? properties->GetFinalFantasyHwnd() : nullptr;
            if (window != nullptr && ::GetCapture() != window)
                this->SuspendActionBarsInput(false);
        }
        // A macro's own subtarget is drained by the executor before its next line.
        if (!this->ActionBarsInputAvailable(false, true))
        {
            const bool pointer = this->actionbars_.PointerDown();
            if (pointer) this->actionbar_pointer_release_ |= MK_LBUTTON;
            if (this->actionbars_.FloatingWindowOpen())
            {
                this->actionbars_.CancelPointerGesture();
                this->actionbars_.SetWindowFocused(false);
            }
            else this->actionbars_.SuspendInput();
            this->actionbars_.SetHover(-1, -1);
            if (pointer) { this->ReleasePointerCapture(); this->SetChatMouseCapture(false); }
        }
        this->PrepareActionBarsExecutor();
        this->action_executor_.Update(this->core_, now);
        this->actionbars_.Update(now);
        if (const auto requested = this->actionbars_.TakeConfigurationRequest())
        {
            this->SelectConfigModule(&this->actionbars_);
            this->actionbars_.SelectConfigView(*requested);
            this->SetEditing(true);
        }
        if (this->actionbars_.TakeMagicBurstSettingsRequest())
        {
            this->SelectConfigModule(&this->magic_burst_);
            this->config_.section = 0;
            this->SetEditing(true);
        }
        const bool presentation = this->actionbars_.TakeSettingsChanged();
        const bool actions = this->action_store_.Changed();
        if (presentation || actions) this->SaveSettings();
    }

    auto hud_runtime::HandleActionBarsCommand(std::string_view command) -> bool
    {
        std::string normalized{command};
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
        command = normalized;
        const auto space = command.find(' ');
        const auto name = command.substr(0, space);
        if (name != "hotbar" && name != "crossbar" && name != "macros" && name != "palettes" && name != "actionbars" && name != "keybinds") return false;
        auto argument = space == std::string_view::npos ? std::string_view{} : command.substr(space + 1);
        while (!argument.empty() && argument.front() == ' ') argument.remove_prefix(1);
        const bool crossbar = name == "crossbar" || (name == "keybinds" && argument == "crossbar");
        if (argument == "cancel" || (name == "macros" && argument == "stop"))
        { this->action_executor_.Cancel(); this->SuspendActionBarsInput(); return true; }
        if (name == "actionbars" && (argument == "run" || argument.starts_with("run ")))
        {
            size_t bar{}, slot{}; char trailing{};
            if (std::sscanf(std::string{argument}.c_str(), "run %zu %zu %c", &bar, &slot, &trailing) != 2 ||
                bar == 0 || bar > action_bar_count || slot == 0 || slot > action_bar_slots)
            {
                if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
                    this->core_->GetChatManager()->Write(1, false, i18n::Text("Usage: /xiuiplus actionbars run <bar 1-10> <slot 1-12>"));
                return true;
            }
            this->PrepareActionBarsExecutor();
            if (!this->action_executor_.Activate(this->core_, this->action_store_.Resolve(action_surface::hotbar, bar - 1, slot - 1), ::GetTickCount64()) &&
                this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
                this->core_->GetChatManager()->Write(1, false, i18n::Text(this->action_executor_.Status().c_str()));
            return true;
        }
        if (argument == "status")
        {
            const auto& bindings = this->actionbars_.ControllerBindings();
            const char* crossbarState = "ready";
            if (!bindings.enabled || !bindings.replace_native) crossbarState = "disabled in Setup";
            else if (!this->actionbars_.BarState(action_bar_count).enabled) crossbarState = "hidden";
            else if (!this->actionbars_.ReplaceNativeMacros()) crossbarState = "native replacement is off";
            else if (this->editing_) crossbarState = "paused: Configuration is open";
            else if (this->actionbars_.DialogOpen()) crossbarState = "paused: close the action-bar workspace";
            else if (!this->GameWindowFocused()) crossbarState = "paused: game is unfocused";
            else if (!this->controller_status_.crossbar_mapping_known) crossbarState = "controller mappings are not ready";
            else if (!this->CrossbarSettings().enabled) crossbarState = "paused: another window owns input";
            if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
            {
                const auto* memory = this->core_->GetMemoryManager();
                const auto party = memory != nullptr ? xiui::ashita::View(memory->GetParty()) : nullptr;
                const auto target = memory != nullptr ? xiui::ashita::View(memory->GetTarget()) : nullptr;
                this->core_->GetChatManager()->Writef(1, false, "[XIUI+] Action Bars: %zu palettes, %zu shared actions, 7 stored crossbar banks; crossbar %s; native macro replacement %s. %s",
                    this->action_store_.AllPalettes().size(), this->action_store_.Macros().size(),
                    crossbarState, this->actionbars_.ReplaceNativeMacros() ? "on" : "off", this->action_executor_.Status().c_str());
                this->core_->GetChatManager()->Writef(1, false, "[XIUI+] Action input: native window %.8s; native menu permission %s; target selection %s; runtime execution %s.",
                    reinterpret_cast<const char*>(&this->visibility_context_.window_focus_name), this->ActionBarsNativeMenuAllowed() ? "allowed" : "blocked",
                    NativeTargetSelectionActive(memory) ? "active" : "idle", this->ActionBarsExecutionAvailable() ? "ready" : "paused");
                if (target != nullptr)
                    this->core_->GetChatManager()->Writef(1, false, "[XIUI+] Native targeting: action %u; subtarget %u; type %u; id %u; action callback %s; cancel callback %s. Pointer held %s; drag %s.",
                        static_cast<unsigned>(target->GetActionTargetActive()), static_cast<unsigned>(target->GetIsSubTargetActive()),
                        static_cast<unsigned>(target->GetActionType()), target->GetActionId(), target->GetActionCallback() != 0 ? "pending" : "none",
                        target->GetCancelCallback() != 0 ? "pending" : "none", this->actionbars_.PointerDown() ? "yes" : "no",
                        this->actionbars_.AssignmentDragging() ? "yes" : "no");
                this->core_->GetChatManager()->Writef(1, false, "[XIUI+] Current TP: %u. %s", party != nullptr ? party->GetMemberTP(0) : 0,
                    this->action_executor_.Diagnostics(::GetTickCount64()).c_str());
            }
            if (const auto library = this->actionbars_.LibraryStatus(); !library.empty() && this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
                this->core_->GetChatManager()->Writef(1, false, "[XIUI+] %s", library.c_str());
            return true;
        }
        if (argument == "next" || argument == "previous")
        {
            this->action_executor_.Cancel();
            this->action_store_.Cycle(crossbar ? action_surface::crossbar : action_surface::hotbar, argument == "next" ? 1 : -1);
            this->SaveSettings(); return true;
        }
        this->SetVisible(true);
        this->ReleaseControllerFocus();
        this->ReleaseChatInput();
        this->SuspendSatchelInput();
        if (name == "keybinds" || argument == "keybinds" || argument == "binds") this->actionbars_.OpenKeybinds(crossbar);
        else if (argument == "setup") this->actionbars_.OpenSetup();
        else if (name == "palettes" || argument == "palettes") this->actionbars_.OpenPalettes(crossbar);
        else if (argument == "import") this->actionbars_.OpenImport();
        else if (name == "macros" || argument == "edit" || argument == "actions") this->actionbars_.OpenLibrary(crossbar);
        else
        {
            this->SelectConfigModule(&this->actionbars_);
            this->actionbars_.SelectConfigView(crossbar ? action_bar_count : 0);
            this->SetEditing(true);
        }
        if (this->actionbars_.FloatingWindowOpen()) this->FinishActionBarsLaunch();
        return true;
    }

    auto hud_runtime::HandleActionBarsKey(const WPARAM key, const LPARAM flags, const bool blocked) -> bool
    {
        this->actionbars_.ObserveNativeMacroKey(key, flags);
        const bool released = (static_cast<uint32_t>(flags) & 0x80000000u) != 0;
        if (released && key < this->actionbar_key_release_.size() && this->actionbar_key_release_[key])
        {
            this->actionbar_key_release_[key] = false;
            this->actionbars_.HandleKey(key, flags);
            return true;
        }
        // A fresh press replaces a release missed during zoning or foreground loss.
        if (!released && (static_cast<uint32_t>(flags) & 0x40000000u) == 0 &&
            key < this->actionbar_key_release_.size() && this->actionbar_key_release_[key])
        {
            this->actionbar_key_release_[key] = false;
            this->actionbars_.HandleKey(key, static_cast<LPARAM>(0x80000000u));
        }
        const bool replacement = !this->editing_ && !this->actionbars_.InputFocused() && this->actionbars_.ReplacesNativeMacroKey(key);
        if (released || blocked || !this->ActionBarsInputAvailable(replacement, this->actionbars_.InputFocused())) return false;
        if (this->editing_ && !this->actionbars_.InputFocused()) return false;
        if (!this->actionbars_.InputFocused() && this->controller_surface_ != controller_surface::none &&
            this->controller_surface_ != controller_surface::hotbar && this->controller_surface_ != controller_surface::crossbar) return false;
        const bool owned = key < this->actionbar_key_release_.size() && this->actionbar_key_release_[key];
        if (!owned && !this->actionbars_.InputFocused() && !KeyboardAllowsGlobalBinding(this->window_config_.FullKeyboard(), key, flags)) return false;
        this->PrepareActionBarsExecutor(replacement);
        const bool consumed = this->actionbars_.HandleKey(key, flags);
        if (consumed && replacement) this->actionbars_.ObserveNativeMacroKey(key, flags, true);
        if (consumed && key < this->actionbar_key_release_.size()) this->actionbar_key_release_[key] = true;
        return consumed;
    }

    auto hud_runtime::HandleActionBarsMouse(const uint32_t message, const WPARAM buttons, const float x, const float y, const bool inputBlocked) -> bool
    {
        const bool blocked = this->PointerInputBlocked(inputBlocked);
        const auto release = message == WM_LBUTTONUP ? MK_LBUTTON : message == WM_RBUTTONUP ? MK_RBUTTON : 0;
        if (release != 0 && (this->actionbar_pointer_release_ & release) != 0 && (!this->ActionBarsInputAvailable(false, true) || !this->actionbars_.PointerDown()))
        {
            if (release == MK_LBUTTON)
            {
                if (this->actionbars_.PointerDown()) this->actionbars_.CancelPointerGesture();
                this->actionbars_.HandleMouse(message, buttons, x, y, false, this->editing_);
            }
            this->actionbar_pointer_release_ &= ~release;
            this->ReleasePointerCapture(); this->SetChatMouseCapture(false); return true;
        }
        if (message == WM_MOUSEMOVE && this->actionbar_pointer_release_ != 0 && !this->actionbars_.PointerDown())
        {
            if (this->editing_)
            {
                D3DVIEWPORT8 viewport{};
                if (this->device_ != nullptr) this->device_->GetViewport(&viewport);
                this->UpdateMouseCapture(viewport, this->mouse_buttons_);
            }
            else this->SetChatMouseCapture(true);
            return true;
        }
        if (!this->ActionBarsInputAvailable(false, true)) return false;
        const bool press = message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK || message == WM_RBUTTONDOWN || message == WM_RBUTTONDBLCLK;
        const bool floating = this->actionbars_.FloatingWindowOpen() && !this->actionbars_.ModalDialogOpen();
        if (!this->actionbars_.PointerDown() && this->editing_ &&
            (this->mouse_down_ || this->interaction_ != interaction::none || this->profile_name_.Focused() || this->profile_dropdown_ ||
                this->profile_actions_ || this->config_.choice_input.Opened() || this->config_.search_open || this->config_.header_menu_open || this->config_.active_color != nullptr))
        {
            if (press && floating) this->actionbars_.SetWindowFocused(false);
            this->actionbars_.SetHover(-1, -1);
            return false;
        }
        if (floating && !this->actionbars_.PointerDown())
        {
            const bool inside = this->ActionBarsPointerOwns(x, y);
            if (press) this->actionbars_.SetWindowFocused(inside && this->actionbars_.WindowPointerOwns(x, y));
            if (!inside) { this->actionbars_.SetHover(-1, -1); return false; }
            if (press) this->FinishConfigInput(true, false);
        }
        if (this->editing_ && !this->actionbars_.InputFocused() && !this->actionbars_.PointerDown() &&
            (this->mouse_down_ || this->interaction_ != interaction::none || this->profile_name_.Focused() || this->profile_dropdown_ ||
                this->profile_actions_ || this->config_.choice_input.Opened() || this->config_.search_open || this->config_.header_menu_open ||
                this->config_.active_color != nullptr || Contains(this->ConfigVisibleBounds(), x, y)))
        { this->actionbars_.SetHover(-1, -1); return false; }
        if (release == MK_LBUTTON && this->actionbars_.AssignmentDragging() && this->editing_ &&
            (Contains(this->ConfigVisibleBounds(), x, y) || this->profile_name_.Focused() || this->profile_dropdown_ ||
                this->profile_actions_ || this->config_.choice_input.Opened() || this->config_.search_open || this->config_.active_color != nullptr))
        {
            this->actionbars_.CancelPointerGesture();
            this->actionbars_.HandleMouse(message, buttons, x, y, false, this->editing_);
            this->actionbar_pointer_release_ &= ~release;
            this->ReleasePointerCapture();
            return true;
        }
        this->actionbars_.SetHover(x, y);
        this->PrepareActionBarsExecutor();
        if (!this->actionbars_.HandleMouse(message, buttons, x, y, blocked, this->editing_))
        {
            if (this->editing_ && !this->actionbars_.InputFocused()) this->actionbars_.SetHover(-1, -1);
            return false;
        }
        if (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK || message == WM_RBUTTONDOWN)
        {
            this->ReleaseControllerFocus();
            this->chat_.Focus(false); this->chat_second_.Focus(false);
            this->actionbar_pointer_release_ |= message == WM_RBUTTONDOWN ? MK_RBUTTON : MK_LBUTTON;
            const auto* properties = this->core_ != nullptr ? this->core_->GetProperties() : nullptr;
            const auto window = properties != nullptr ? properties->GetFinalFantasyHwnd() : nullptr;
            if (window != nullptr) { ::SetCapture(window); if (::GetCapture() == window) this->mouse_capture_window_ = window; }
        }
        if (this->editing_)
        {
            D3DVIEWPORT8 viewport{};
            if (this->device_ != nullptr) this->device_->GetViewport(&viewport);
            this->UpdateMouseCapture(viewport, this->mouse_buttons_);
        }
        else this->SetChatMouseCapture(true);
        if (release != 0) { this->actionbar_pointer_release_ &= ~release; this->ReleasePointerCapture(); }
        return true;
    }
}
