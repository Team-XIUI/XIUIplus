#include "hud_runtime.hpp"

#include <algorithm>
#include <array>
#include <cstdio>

namespace xiui
{
    auto hud_runtime::ControllerPresented(void) const -> bool
    {
        return this->general_.InputMode() == 2 || (this->general_.InputMode() == 0 && this->controller_presented_);
    }

    auto hud_runtime::GameWindowFocused(void) const -> bool
    {
        const auto* properties = this->core_ != nullptr ? this->core_->GetProperties() : nullptr;
        return properties == nullptr || ::GetForegroundWindow() == properties->GetFinalFantasyHwnd();
    }

    auto hud_runtime::ControllerNativeBusy(void) const -> bool
    {
        if (this->core_ == nullptr) return false;
        if (!this->GameWindowFocused()) return true;
        const auto* chat = this->core_->GetChatManager();
        if (chat != nullptr && chat->IsInputOpen() != 0) return true;
        const auto* memory = this->core_->GetMemoryManager();
        const auto* player = memory != nullptr ? memory->GetPlayer() : nullptr;
        if (player != nullptr && player->GetLoginStatus() != static_cast<uint8_t>(Ashita::FFXI::Enums::LoginStatus::LoggedIn)) return true;
        const auto* target = memory != nullptr ? memory->GetTarget() : nullptr;
        return this->visibility_context_.event_active || this->visibility_context_.map_open || this->visibility_context_.interface_hidden || this->party_source_.InCutscene() || (target != nullptr &&
            (target->GetActionTargetActive() != 0 || target->GetIsSubTargetActive() != 0));
    }

    auto hud_runtime::ControllerNativeMenuOpen(void) const -> bool
    {
        const auto* memory = this->core_ != nullptr ? this->core_->GetMemoryManager() : nullptr;
        const auto* target = memory != nullptr ? memory->GetTarget() : nullptr;
        return this->native_ui_.menu_visible || (target != nullptr && target->GetIsMenuOpen() != 0);
    }

    auto hud_runtime::ControllerCapture(void) const -> controller_capture_mode
    {
        if (!this->visible_ || !this->ControllerSurfaceVisible() || this->ControllerNativeBusy()) return controller_capture_mode::none;
        if (this->editing_) return controller_capture_mode::exclusive;
        if (this->ControllerNativeMenuOpen())
        {
            const bool statusBrowse = !this->keyboard_window_focus_ &&
                (this->controller_surface_ == controller_surface::buffs || this->controller_surface_ == controller_surface::debuffs);
            return statusBrowse ? controller_capture_mode::navigation : controller_capture_mode::none;
        }
        if (this->ActionBarsInputAvailable() && this->actionbars_.InputFocused()) return controller_capture_mode::exclusive;
        if (this->ChatFocused()) return controller_capture_mode::navigation;
        return this->controller_surface_ != controller_surface::none ? controller_capture_mode::exclusive : controller_capture_mode::none;
    }

    auto hud_runtime::ControllerInputState(void) const -> uint32_t
    {
        const uint32_t revision = (this->controller_input_revision_ << 16) & controller_revision_mask;
        if (!this->visible_ || this->ControllerNativeBusy()) return revision;
        uint32_t state = revision | static_cast<uint32_t>(this->ControllerCapture());
        if ((this->controller_surface_ == controller_surface::party || this->controller_surface_ == controller_surface::satchel || this->controller_surface_ == controller_surface::treasure) && this->ControllerSurfaceVisible()) state |= controller_party_input;
        state |= ControllerEntryBit(controller_action::focus);
        if ((this->ContextVisible(this->chat_) && this->chat_.WindowVisible()) ||
            (this->ContextVisible(this->chat_second_) && this->chat_second_.WindowVisible()) ||
            (this->ContextVisible(this->buffs_) && this->buffs_.ShouldRender({})) ||
            (this->ContextVisible(this->debuffs_) && this->debuffs_.ShouldRender({}))) state |= ControllerEntryBit(controller_action::cycle_window);
        return state;
    }

    auto hud_runtime::UpdateControllerStatus(const controller_status& status) -> void
    {
        this->controller_status_ = status;
        this->general_.SetControllerStatus(status);
        this->actionbars_.UpdateControllerStatus(status);
        if ((status.suspended || (status.connection_known && !status.connected)) && this->actionbars_.SuspendCrossbarInput())
            this->action_executor_.Cancel();
        if (this->config_.focus.resizing && this->config_.focus.resize_keyboard)
        {
            if (!this->visible_ || !this->editing_ || this->ControllerNativeBusy()) this->ReleaseControllerFocus(!this->GameWindowFocused());
            return;
        }
        if (!this->keyboard_window_focus_ && (status.suspended || (status.connection_known && !status.connected) || status.enumeration_disabled ||
            (status.configuration_known && !status.gamepad_enabled)))
            if (this->controller_surface_ != controller_surface::none || this->config_.focus.active) this->ReleaseControllerFocus(!this->GameWindowFocused());
    }

    auto hud_runtime::ReleaseControllerFocus(const bool preserveExpandedHistory) -> void
    {
        if (this->controller_surface_ == controller_surface::chat || this->controller_surface_ == controller_surface::chat2)
        { this->chat_.Focus(false, preserveExpandedHistory); this->chat_second_.Focus(false, preserveExpandedHistory); }
        for (auto* group : {&this->party_, &this->alliance_first_, &this->alliance_second_}) group->EndControllerBrowse();
        this->enemy_list_.EndControllerBrowse();
        this->satchel_.EndControllerBrowse();
        this->treasure_.EndInput();
        this->actionbars_.EndControllerBrowse();
        this->target_.EndControllerStatus();
        this->buffs_.EndControllerBrowse();
        this->debuffs_.EndControllerBrowse();
        this->pet_.EndControllerStatus();
        this->controller_status_details_ = false;
        this->controller_surface_ = controller_surface::none;
        this->keyboard_window_focus_ = false;
        this->controller_notification_id_ = 0;
        this->controller_hint_.clear();
        this->ResetControllerConfigFocus();
        this->config_.input_mode_menu = false;
    }

    auto hud_runtime::ControllerSurfaces(void) const -> std::vector<controller_surface>
    {
        std::vector<controller_surface> result;
        if (this->ContextVisible(this->notifications_) && this->notifications_.FirstDetail() != 0) result.push_back(controller_surface::notifications);
        if (this->ContextVisible(this->chat_) && this->chat_.WindowVisible()) result.push_back(controller_surface::chat);
        if (this->ContextVisible(this->chat_second_) && this->chat_second_.WindowVisible()) result.push_back(controller_surface::chat2);
        if (this->ControllerPartyVisible(0) || this->ControllerPartyVisible(1) || this->ControllerPartyVisible(2)) result.push_back(controller_surface::party);
        if (this->ContextVisible(this->enemy_list_) && this->enemy_list_.ShouldRender({})) result.push_back(controller_surface::enemies);
        if (this->ContextVisible(this->ready_check_) && this->ready_check_.ControllerButtonCount() != 0) result.push_back(controller_surface::ready_check);
        if (this->ContextVisible(this->equipment_) && this->equipment_.ShouldRender({})) result.push_back(controller_surface::equipment);
        if (this->ContextVisible(this->satchel_) && this->satchel_.State().enabled) result.push_back(controller_surface::satchel);
        if (this->ContextVisible(this->treasure_) && this->treasure_.ShouldRender({}) && this->treasure_source_.Ready() && this->treasure_source_.Count() != 0) result.push_back(controller_surface::treasure);
        if (this->ContextVisible(this->actionbars_) && this->actionbars_.BarState(0).enabled) result.push_back(controller_surface::hotbar);
        if (this->ContextVisible(this->actionbar_children_.back()) && this->actionbars_.BarState(action_bar_count).enabled) result.push_back(controller_surface::crossbar);
        if (this->TargetStatusVisible()) result.push_back(controller_surface::target_status);
        if (this->ContextVisible(this->buffs_) && this->buffs_.ShouldRender({})) result.push_back(controller_surface::buffs);
        if (this->ContextVisible(this->debuffs_) && this->debuffs_.ShouldRender({})) result.push_back(controller_surface::debuffs);
        if (this->ContextVisible(this->pet_) && this->pet_.ShouldRender({}) && this->pet_.HasStatusDetails()) result.push_back(controller_surface::pet_status);
        result.push_back(controller_surface::config);
        return result;
    }

    auto hud_runtime::ControllerSurfaceName(const controller_surface surface) const -> const char*
    {
        switch (surface)
        {
        case controller_surface::chat: return "Chat";
        case controller_surface::chat2: return "Chat 2";
        case controller_surface::party: return "Party";
        case controller_surface::enemies: return "Enemy List";
        case controller_surface::ready_check: return "Ready Check";
        case controller_surface::equipment: return "Equipment";
        case controller_surface::satchel: return "Satchel";
        case controller_surface::treasure: return "Treasure Pool";
        case controller_surface::hotbar: return "Hotbars";
        case controller_surface::crossbar: return "Crossbar";
        case controller_surface::target_status: return "Target status effects";
        case controller_surface::buffs: return "Buffs";
        case controller_surface::debuffs: return "Debuffs";
        case controller_surface::pet_status: return "Pet status effects";
        case controller_surface::notifications: return "Notification details";
        case controller_surface::config: return "Configuration";
        default: return "XIUI focus";
        }
    }

    auto hud_runtime::BeginControllerFocus(const controller_surface surface) -> bool
    {
        this->UpdateVisibilityContext();
        if (!this->visible_ || this->ControllerNativeBusy()) return false;
        const auto surfaces = this->ControllerSurfaces();
        if (surface != controller_surface::picker && std::find(surfaces.begin(), surfaces.end(), surface) == surfaces.end()) return false;
        if (!this->FinishConfigInput(true)) return false;
        if (surface != controller_surface::config && this->editing_ && !this->SetEditing(false)) return false;
        if (surface != controller_surface::satchel) this->SuspendSatchelInput();
        this->SuspendActionBarsInput(surface != controller_surface::config);
        this->ReleaseControllerFocus();
        this->ReleaseChatInput();
        switch (surface)
        {
        case controller_surface::treasure:
            this->treasure_.Update(this->core_, this->TreasureInputAvailable());
            if (!this->TreasureInputAvailable() || !this->treasure_.BeginBrowse()) return false;
            break;
        case controller_surface::notifications:
            this->controller_notification_id_ = this->notifications_.FirstDetail();
            if (this->controller_notification_id_ == 0) return false;
            break;
        case controller_surface::config:
            if (!this->SetEditing(true)) return false;
            this->config_.focus.active = true;
            break;
        case controller_surface::chat:
            if (!this->chat_.BeginControllerHistory()) return false;
            break;
        case controller_surface::chat2:
            if (!this->chat_second_.BeginControllerHistory()) return false;
            break;
        case controller_surface::party:
        {
            const std::array<party_module*, 3> groups{&this->party_, &this->alliance_first_, &this->alliance_second_};
            this->controller_party_group_ = 0;
            while (this->controller_party_group_ < groups.size() && !this->ControllerPartyVisible(this->controller_party_group_)) ++this->controller_party_group_;
            if (this->controller_party_group_ == groups.size() || !groups[this->controller_party_group_]->BeginControllerBrowse()) return false;
            break;
        }
        case controller_surface::enemies:
            if (!this->enemy_list_.BeginControllerBrowse()) return false;
            break;
        case controller_surface::target_status:
            if (!this->target_.BeginControllerStatus()) return false;
            break;
        case controller_surface::buffs:
            if (!this->buffs_.BeginControllerBrowse()) return false;
            break;
        case controller_surface::debuffs:
            if (!this->debuffs_.BeginControllerBrowse()) return false;
            break;
        case controller_surface::pet_status:
            if (!this->pet_.BeginControllerStatus()) return false;
            break;
        case controller_surface::picker: this->controller_picker_index_ = 0; break;
        case controller_surface::equipment: this->controller_equipment_slot_ = 0; break;
        case controller_surface::satchel:
            if (this->ControllerNativeMenuOpen()) return false;
            this->satchel_.Open(true);
            this->satchel_.BeginControllerBrowse();
            break;
        case controller_surface::hotbar:
        case controller_surface::crossbar:
            if (this->ControllerNativeMenuOpen()) return false;
            this->actionbars_.BeginControllerBrowse(surface == controller_surface::crossbar);
            break;
        case controller_surface::ready_check: this->controller_ready_button_ = this->ready_check_.ControllerButtonCount() > 1 ? 1 : 0; break;
        default: return false;
        }
        this->controller_surface_ = surface;
        this->controller_presented_ = true;
        this->ui_sounds_.MenuOpened(::GetTickCount64());
        return true;
    }

    auto hud_runtime::SelectHudTarget(const uint32_t id, const uint32_t index) -> bool
    {
        const auto* memory = this->core_ != nullptr ? this->core_->GetMemoryManager() : nullptr;
        const auto* target = memory != nullptr ? memory->GetTarget() : nullptr;
        const auto* entity = memory != nullptr ? memory->GetEntity() : nullptr;
        if (target == nullptr || entity == nullptr || id == 0 || index == 0 || index >= entity->GetEntityMapSize() ||
            entity->GetServerId(index) != id || (entity->GetRenderFlags0(index) & 0x200) == 0 ||
            (entity->GetRenderFlags0(index) & 0x4000) != 0) return false;
        target->SetTarget(index, false);
        this->ui_sounds_.Select(::GetTickCount64());
        return true;
    }

    auto hud_runtime::UpdateControllerFocus(void) -> void
    {
        if (this->controller_surface_ == controller_surface::notifications && !this->notifications_.Detail(this->controller_notification_id_))
            this->controller_notification_id_ = this->notifications_.FirstDetail();
        if (this->controller_surface_ == controller_surface::none) return;
        if (this->controller_surface_ == controller_surface::buffs) this->buffs_.RefreshControllerBrowse();
        if (this->controller_surface_ == controller_surface::debuffs) this->debuffs_.RefreshControllerBrowse();
        if (!this->visible_ || !this->ControllerSurfaceVisible() || this->ControllerNativeBusy()) { this->ReleaseControllerFocus(!this->GameWindowFocused()); return; }
        if (this->controller_surface_ == controller_surface::config && !this->editing_) this->ReleaseControllerFocus();
        if (this->controller_surface_ == controller_surface::party)
        {
            const std::array<party_module*, 3> groups{&this->party_, &this->alliance_first_, &this->alliance_second_};
            if (!this->ControllerPartyVisible(this->controller_party_group_) ||
                !groups[this->controller_party_group_]->ControllerMember()) this->ReleaseControllerFocus();
        }
        if (this->controller_surface_ == controller_surface::ready_check && this->ready_check_.ControllerButtonCount() == 0) this->ReleaseControllerFocus();
        if (this->controller_surface_ == controller_surface::equipment && !this->equipment_.ShouldRender({})) this->ReleaseControllerFocus();
        if (this->controller_surface_ == controller_surface::treasure && !this->treasure_.InputFocused()) this->ReleaseControllerFocus();
        if (this->controller_surface_ == controller_surface::satchel && (!this->SatchelInputAvailable() || !this->satchel_.InputFocused())) this->ReleaseControllerFocus();
        if ((this->controller_surface_ == controller_surface::hotbar || this->controller_surface_ == controller_surface::crossbar) &&
            (!this->ActionBarsInputAvailable() || !this->actionbars_.InputFocused())) this->ReleaseControllerFocus();
        if (this->controller_surface_ == controller_surface::enemies && !this->enemy_list_.ControllerMember()) this->ReleaseControllerFocus();
        if ((this->controller_surface_ == controller_surface::target_status || this->controller_surface_ == controller_surface::buffs ||
            this->controller_surface_ == controller_surface::debuffs || this->controller_surface_ == controller_surface::pet_status) && this->ControllerStatusBounds().width <= 0) this->ReleaseControllerFocus();
        if (this->controller_status_details_ && this->ControllerStatusBounds().width <= 0) this->controller_status_details_ = false;
        if ((this->controller_surface_ == controller_surface::chat && !this->chat_.HistoryFocused() && !this->chat_.Focused()) ||
            (this->controller_surface_ == controller_surface::chat2 && !this->chat_second_.HistoryFocused() && !this->chat_.Focused())) this->ReleaseControllerFocus();
    }

    auto hud_runtime::HandleController(const controller_event& event) -> bool
    {
        this->UpdateVisibilityContext();
        const bool captured = this->ControllerCaptured();
        if (event.action == controller_action::crossbar_bank || event.action == controller_action::crossbar_slot ||
            event.action == controller_action::palette_previous || event.action == controller_action::palette_next)
        {
            if (!this->CrossbarSettings().enabled) return false;
            this->action_executor_.SetRestricted(!this->ActionBarsExecutionAvailable());
            this->controller_presented_ = true;
            return this->actionbars_.HandleCrossbarEvent(event);
        }
        if (!this->visible_ || this->ControllerNativeBusy()) { this->ReleaseControllerFocus(!this->GameWindowFocused()); return false; }
        if (!event.pressed || event.action == controller_action::none) return captured;
        if (event.backend != controller_backend::none) this->keyboard_window_focus_ = false;
        if (event.repeated && event.action != controller_action::up && event.action != controller_action::down &&
            event.action != controller_action::left && event.action != controller_action::right) return captured;
        if (this->ActionBarsInputAvailable() && this->actionbars_.InputFocused())
        {
            this->action_executor_.SetRestricted(!this->ActionBarsExecutionAvailable());
            const bool consumed = this->actionbars_.HandleControllerAction(event.action, event.repeated);
            this->UpdateControllerFocus();
            return consumed;
        }
        if (event.action == controller_action::focus) return event.repeated ? captured : this->BeginControllerFocus(controller_surface::picker);
        this->UpdateControllerFocus();
        if (!captured && !this->editing_)
        {
            if (event.action == controller_action::cycle_window) return this->CycleWindowFocus(true);
            return false;
        }
        this->controller_presented_ = true;
        this->controller_hint_.clear();
        if (this->editing_)
        {
            this->controller_surface_ = controller_surface::config;
            return this->HandleControllerConfig(event.action, event.repeated);
        }
        if (this->controller_surface_ == controller_surface::picker)
        {
            const auto surfaces = this->ControllerSurfaces();
            this->controller_picker_index_ = std::min(this->controller_picker_index_, surfaces.size() - 1);
            if (event.action == controller_action::cancel) this->ReleaseControllerFocus();
            else if (event.action == controller_action::confirm) this->BeginControllerFocus(surfaces[this->controller_picker_index_]);
            else if (event.action == controller_action::up || event.action == controller_action::previous_tab)
                this->controller_picker_index_ = (this->controller_picker_index_ + surfaces.size() - 1) % surfaces.size();
            else if (event.action == controller_action::down || event.action == controller_action::next_tab || event.action == controller_action::cycle_window)
                this->controller_picker_index_ = (this->controller_picker_index_ + 1) % surfaces.size();
            return true;
        }
        if (this->controller_surface_ == controller_surface::satchel)
        {
            const bool consumed = this->satchel_.HandleControllerAction(event.action, event.repeated);
            this->UpdateControllerFocus();
            return consumed;
        }
        if (this->controller_surface_ == controller_surface::treasure)
        {
            const bool consumed = this->treasure_.HandleControllerAction(event.action, event.repeated);
            this->UpdateControllerFocus(); return consumed;
        }
        if (this->chat_.HistoryFocused() || this->chat_second_.HistoryFocused() || this->chat_.Focused())
        {
            auto& chat = this->chat_second_.HistoryFocused() ? this->chat_second_ : this->chat_;
            this->controller_surface_ = &chat == &this->chat_second_ ? controller_surface::chat2 : controller_surface::chat;
            if (event.action == controller_action::cycle_window && !chat.HistoryExpanded() && !this->chat_.Focused())
                return this->CycleWindowFocus(true);
            if (event.action == controller_action::actions && !this->ContextVisible(this->chat_))
            { this->controller_hint_ = "The chat editor is hidden in this context."; return true; }
            const bool consumed = chat.HandleControllerAction(event.action, event.repeated, this->design_system_, this->core_ != nullptr ? this->core_->GetChatManager() : nullptr);
            this->UpdateControllerFocus();
            return consumed;
        }
        if (event.action == controller_action::cycle_window &&
            (this->controller_surface_ == controller_surface::buffs || this->controller_surface_ == controller_surface::debuffs))
            return this->CycleWindowFocus(true);
        if (this->HandleControllerStatus(event.action)) return true;
        if (event.action == controller_action::cancel) { this->ReleaseControllerFocus(); return true; }
        if (this->controller_surface_ == controller_surface::party)
        {
            if (event.action == controller_action::cycle_window) return this->CycleWindowFocus(true);
            const std::array<party_module*, 3> groups{&this->party_, &this->alliance_first_, &this->alliance_second_};
            auto& group = *groups[this->controller_party_group_];
            const auto member = group.ControllerMember();
            if (event.action == controller_action::up || event.action == controller_action::down)
                group.MoveControllerBrowse(event.action == controller_action::up ? -1 : 1);
            else if (event.action == controller_action::left || event.action == controller_action::right ||
                event.action == controller_action::previous_tab)
            {
                const int direction = event.action == controller_action::left || event.action == controller_action::previous_tab ? -1 : 1;
                for (size_t step = 1; step < groups.size(); ++step)
                {
                    const size_t next = (this->controller_party_group_ + groups.size() + direction * static_cast<int>(step)) % groups.size();
                    if (!this->ControllerPartyVisible(next) || !groups[next]->BeginControllerBrowse(0, member ? member->slot % 6 + static_cast<uint32_t>(next * 6) : UINT32_MAX)) continue;
                    group.EndControllerBrowse(); this->controller_party_group_ = next; break;
                }
            }
            else if (event.action == controller_action::confirm && member)
            {
                if (!member->in_zone || !member->entity_valid || !this->SelectHudTarget(member->id, member->index))
                    this->controller_hint_ = "This member cannot be targeted here.";
            }
            else if (event.action == controller_action::actions && member && this->ContextVisible(this->chat_) && this->chat_.WindowVisible())
            {
                if (this->chat_.OpenTell(member->name))
                { this->chat_.BeginControllerCompose(); group.EndControllerBrowse(); this->controller_surface_ = controller_surface::chat; }
            }
            return true;
        }
        if (this->controller_surface_ == controller_surface::notifications)
        {
            if (event.action == controller_action::up || event.action == controller_action::left || event.action == controller_action::down || event.action == controller_action::right)
                this->controller_notification_id_ = this->notifications_.NextDetail(this->controller_notification_id_,
                    event.action == controller_action::up || event.action == controller_action::left ? -1 : 1);
            return true;
        }
        if (this->controller_surface_ == controller_surface::enemies)
        {
            if (event.action == controller_action::up || event.action == controller_action::down)
                this->enemy_list_.MoveControllerBrowse(event.action == controller_action::up ? -1 : 1);
            else if (event.action == controller_action::left || event.action == controller_action::right)
                this->enemy_list_.MoveControllerBrowseHorizontal(event.action == controller_action::left ? -1 : 1);
            else if (event.action == controller_action::confirm)
            {
                const auto member = this->enemy_list_.ControllerMember();
                if (!member || !this->SelectHudTarget(member->id, member->index))
                    this->controller_hint_ = "This enemy cannot be targeted here.";
            }
            return true;
        }
        if (this->controller_surface_ == controller_surface::ready_check)
        {
            const size_t count = this->ready_check_.ControllerButtonCount();
            if (count == 0) { this->ReleaseControllerFocus(); return true; }
            this->controller_ready_button_ = std::min(this->controller_ready_button_, count - 1);
            if (event.action == controller_action::left || event.action == controller_action::right)
                this->controller_ready_button_ = (this->controller_ready_button_ + 1) % count;
            if (event.action == controller_action::confirm)
            { this->ready_check_.ActivateControllerButton(this->controller_ready_button_, event.timestamp); this->UpdateControllerFocus(); }
            return true;
        }
        if (this->controller_surface_ == controller_surface::equipment)
        {
            const int columns = static_cast<int>(this->equipment_.SlotColumns());
            const int delta = event.action == controller_action::up ? -columns : event.action == controller_action::down ? columns :
                event.action == controller_action::left ? -1 : event.action == controller_action::right ? 1 : 0;
            this->controller_equipment_slot_ = static_cast<size_t>(std::clamp(static_cast<int>(this->controller_equipment_slot_) + delta, 0, 15));
            return true;
        }
        return captured;
    }

    auto hud_runtime::ControllerCommand(const std::string_view command) -> bool
    {
        if (command == "controller status")
        {
            const auto& status = this->controller_status_;
            if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
                this->core_->GetChatManager()->Writef(1, false,
                    "[XIUI] Controller: %s; enumeration %s; connection %s; input %s; bindings %s; focus %s.",
                    status.backend == controller_backend::xinput ? "XInput" : status.backend == controller_backend::direct_input ? "DirectInput" : "not detected",
                    status.enumeration_disabled ? "disabled" : "enabled", !status.connection_known ? "unknown" : status.connected ? "connected" : "disconnected",
                    status.received_input ? "observed" : "not observed", status.bindings_known ? "resolved" : "unavailable", this->ControllerSurfaceName(this->controller_surface_));
            return true;
        }
        if (command == "controller" || command == "input")
        {
            if (!this->BeginControllerFocus(controller_surface::config)) return true;
            this->SelectConfigModule(&this->general_);
            this->config_.section = general_module::input_section;
            return true;
        }
        if (command != "focus" && !command.starts_with("focus ")) return false;
        if (command == "focus off" || command == "focus none") { this->ReleaseControllerFocus(); return true; }
        controller_surface surface = controller_surface::picker;
        if (command == "focus notifications") surface = controller_surface::notifications;
        else if (command == "focus chat") surface = controller_surface::chat;
        else if (command == "focus chat2") surface = controller_surface::chat2;
        else if (command == "focus party") surface = controller_surface::party;
        else if (command == "focus enemies" || command == "focus enemylist") surface = controller_surface::enemies;
        else if (command == "focus config") surface = controller_surface::config;
        else if (command == "focus readycheck") surface = controller_surface::ready_check;
        else if (command == "focus equipment") surface = controller_surface::equipment;
        else if (command == "focus satchel") surface = controller_surface::satchel;
        else if (command == "focus treasure" || command == "focus pool") surface = controller_surface::treasure;
        else if (command == "focus hotbar" || command == "focus hotbars") surface = controller_surface::hotbar;
        else if (command == "focus crossbar") surface = controller_surface::crossbar;
        else if (command == "focus pet") surface = controller_surface::pet_status;
        else if (command == "focus target") surface = controller_surface::target_status;
        else if (command == "focus buffs" || command == "focus statuses") surface = controller_surface::buffs;
        else if (command == "focus debuffs") surface = controller_surface::debuffs;
        else if (command != "focus")
        {
            if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
                this->core_->GetChatManager()->Write(1, false, "[XIUI] Use /xiui focus [chat|chat2|party|enemies|target|buffs|debuffs|config|readycheck|equipment|satchel|treasure|hotbar|crossbar|off].");
            return true;
        }
        if (!this->BeginControllerFocus(surface) && this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
            this->core_->GetChatManager()->Write(1, false, "[XIUI] That surface is unavailable, or native text/targeting currently owns input.");
        return true;
    }

    auto hud_runtime::ControllerPickerBounds(const D3DVIEWPORT8& viewport) const -> rect
    {
        const float width = std::min(320.0f, static_cast<float>(viewport.Width) - 16);
        const float height = 86 + static_cast<float>(this->ControllerSurfaces().size()) * 32;
        return {viewport.X + (viewport.Width - width) / 2, viewport.Y + (viewport.Height - height) / 2, width, height};
    }

    auto hud_runtime::HandleControllerPointer(const uint32_t message, const float x, const float y, const D3DVIEWPORT8& viewport) -> bool
    {
        if (this->controller_pointer_down_)
        {
            if (message == WM_LBUTTONUP)
            {
                this->controller_pointer_down_ = false;
                this->SetChatMouseCapture(false);
                this->ReleasePointerCapture();
            }
            return true;
        }
        if (this->controller_surface_ != controller_surface::picker) return false;
        if (message == WM_LBUTTONDOWN)
        {
            this->controller_pointer_down_ = true;
            const auto bounds = this->ControllerPickerBounds(viewport);
            const auto surfaces = this->ControllerSurfaces();
            for (size_t i = 0; i < surfaces.size(); ++i)
            {
                if (Contains({bounds.x + 12, bounds.y + 42 + i * 32, bounds.width - 24, 30}, x, y))
                { this->BeginControllerFocus(surfaces[i]); return true; }
            }
            this->ReleaseControllerFocus();
        }
        return true;
    }

    auto hud_runtime::ChatFocusBounds(void) const -> rect
    {
        if (!this->visible_ || this->editing_ || this->chat_.Focused() || !this->GameWindowFocused()) return {};
        if (this->ContextVisible(this->chat_second_) && this->chat_second_.HistoryFocused()) return this->chat_second_.ControllerBounds(this->design_system_);
        if (this->ContextVisible(this->chat_) && this->chat_.HistoryFocused()) return this->chat_.ControllerBounds(this->design_system_);
        return {};
    }

    auto hud_runtime::KeyboardStatusFocusBounds(void) const -> rect
    {
        if (!this->keyboard_window_focus_ || !this->visible_ || this->editing_ || this->ControllerNativeBusy() ||
            !this->ControllerSurfaceVisible()) return {};
        return this->controller_surface_ == controller_surface::buffs || this->controller_surface_ == controller_surface::debuffs ?
            this->ControllerStatusBounds() : rect{};
    }

    auto hud_runtime::DrawControllerOverlay(const D3DVIEWPORT8& viewport, const module_render_context& context) -> void
    {
        const auto chatFocus = this->ChatFocusBounds();
        if (chatFocus.width > 0 && chatFocus.height > 0) this->design_system_.DrawFocusFrame(chatFocus, this->general_.FocusStyle());
        const auto statusFocus = this->KeyboardStatusFocusBounds();
        if (statusFocus.width > 0 && statusFocus.height > 0)
        {
            this->design_system_.DrawFocusFrame(statusFocus, this->general_.FocusStyle());
            return;
        }
        if (!this->ControllerCaptured()) return;
        const auto& theme = this->design_system_.GetTheme();
        if (this->controller_surface_ == controller_surface::picker)
        {
            const auto bounds = this->ControllerPickerBounds(viewport);
            this->config_controls_.Panel(bounds);
            auto style = theme.body_text; style.scale = 0.78f;
            this->config_controls_.Text("XIUI focus", {bounds.x + 14, bounds.y + 8, bounds.width - 28, 28}, style);
            const auto surfaces = this->ControllerSurfaces();
            this->controller_picker_index_ = std::min(this->controller_picker_index_, surfaces.size() - 1);
            for (size_t i = 0; i < surfaces.size(); ++i)
                this->config_controls_.NavigationItem({bounds.x + 12, bounds.y + 42 + i * 32, bounds.width - 24, 30},
                    this->ControllerSurfaceName(surfaces[i]), i == this->controller_picker_index_, false);
            style.scale = 0.65f;
            this->config_controls_.Text("Up/Down: choose   Confirm: open   Cancel: back", {bounds.x + 12, bounds.y + bounds.height - 30, bounds.width - 24, 24}, style);
            this->design_system_.DrawFocusFrame({bounds.x + 12, bounds.y + 42 + this->controller_picker_index_ * 32, bounds.width - 24, 30}, this->general_.FocusStyle());
            return;
        }
        if (this->editing_) return;
        if (this->controller_surface_ == controller_surface::notifications)
        {
            if (const auto detail = this->notifications_.Detail(this->controller_notification_id_))
            {
                this->design_system_.DrawFocusFrame(detail->bounds, this->general_.FocusStyle());
                this->config_controls_.Tooltip(this->notifications_.Details(detail->id).c_str(), detail->bounds, viewport);
            }
            return;
        }
        if (this->controller_surface_ == controller_surface::party)
        {
            const std::array<const party_module*, 3> groups{&this->party_, &this->alliance_first_, &this->alliance_second_};
            const auto focus = this->controller_status_details_ ? this->ControllerStatusBounds() : groups[this->controller_party_group_]->ControllerBounds();
            this->design_system_.DrawFocusFrame(focus, this->general_.FocusStyle());
            return;
        }
        if (this->controller_surface_ == controller_surface::chat || this->controller_surface_ == controller_surface::chat2 ||
            this->chat_.HistoryFocused() || this->chat_second_.HistoryFocused())
        {
            return;
        }
        rect focus{};
        const char* prompt{};
        if (this->controller_surface_ == controller_surface::enemies)
        {
            focus = this->controller_status_details_ ? this->ControllerStatusBounds() : this->enemy_list_.ControllerBounds();
            prompt = this->controller_status_details_ ? "Directions: status effect   Cancel: enemy row" :
                "Directions: enemy   Confirm: target   Actions: status details   Cancel: back";
        }
        else if (this->controller_surface_ == controller_surface::target_status || this->controller_surface_ == controller_surface::buffs ||
            this->controller_surface_ == controller_surface::debuffs || this->controller_surface_ == controller_surface::pet_status)
        {
            focus = this->ControllerStatusBounds();
            prompt = this->controller_surface_ == controller_surface::buffs || this->controller_surface_ == controller_surface::debuffs ?
                "Directions: status effect   Confirm: remove cancellable effect   Cancel: back" : "Directions: status effect   Cancel: back";
        }
        else if (this->controller_surface_ == controller_surface::ready_check)
        {
            focus = this->ready_check_.ControllerButtonBounds(this->controller_ready_button_);
            prompt = "Left/Right: choose   Confirm: select   Cancel: leave focus";
        }
        else if (this->controller_surface_ == controller_surface::equipment)
        {
            focus = this->equipment_.SlotBounds(this->controller_equipment_slot_);
            this->equipment_.RenderTooltip(context, focus.x + focus.width / 2, focus.y + focus.height / 2);
            prompt = "Directions: equipment slot   Cancel: back";
        }
        else if (this->controller_surface_ == controller_surface::treasure)
            prompt = "Up/Down: row   Left/Right: action   Confirm: select   Actions: details   Cancel: leave focus";
        if (prompt == nullptr) return;
        const rect footer{viewport.X + 16.0f, viewport.Y + viewport.Height - 44.0f, static_cast<float>(viewport.Width) - 32, 30};
        this->config_controls_.Panel(footer);
        auto style = theme.caption_text; style.scale = 0.7f;
        this->config_controls_.Text(this->controller_hint_.empty() ? prompt : this->controller_hint_.c_str(), {footer.x + 10, footer.y, footer.width - 20, footer.height}, style);
        if (focus.width > 0 && focus.height > 0) this->design_system_.DrawFocusFrame(focus, this->general_.FocusStyle());
    }
}
