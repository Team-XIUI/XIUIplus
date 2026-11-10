#include "config/module_visibility.hpp"
#include "ashita_compat.hpp"
#include "hud_runtime.hpp"
#include "Memory.h"
#include "native_pattern.hpp"
#include "native_menu_focus.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <string_view>

namespace
{
    template<typename T>
    auto Read(const uintptr_t address, T& value) noexcept -> bool
    {
        if (address == 0) return false;
        __try { std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T)); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

}

namespace xiui
{
    auto hud_runtime::ConfigPreview(const hud_module& module) const -> bool
    {
        if (const auto index = this->NativeWindowIndex(module); index < native_window_count) return this->NativeEditMode() && this->native_windows_[index].OnScreen();
        if (this->NativeEditMode()) return false;
        if (!this->editing_) return false;
        if (this->config_.edit_mode) return true;
        if (!this->config_.preview_selected || this->config_.basic) return false;
        const auto* selected = this->MovementSelectionModule();
        if (selected != nullptr && !selected->HasEditableBounds()) selected = this->config_.module;
        return selected == &module;
    }

    auto hud_runtime::ModuleDisplayed(const hud_module& module) const -> bool
    {
        if (const auto index = this->NativeWindowIndex(module); index < native_window_count) return this->NativeEditMode() && this->native_windows_[index].OnScreen();
        if (this->NativeEditMode()) return false;
        return this->ContextVisible(module) && (this->ConfigPreview(module) ? module.PreviewVisible() : module.ShouldRender(this->layout_snapshot_));
    }

    auto hud_runtime::ModuleEditControlsVisible(const hud_module& module) const -> bool
    {
        if (!this->editing_ || !this->ModuleDisplayed(module) || (!module.CanMove() && !module.ConfigScaleEditable())) return false;
        if (this->config_.edit_mode || this->ConfigPreview(module) || this->interaction_module_ == &module ||
            (this->config_.focus.moving && this->config_.focus.moving_id == module.Id())) return true;
        const float x = static_cast<float>(this->mouse_x_), y = static_cast<float>(this->mouse_y_);
        return !Contains(this->ConfigVisibleBounds(), x, y) && !this->config_.choice_input.Opened() &&
            !this->config_.search_open && !this->ProfileSurface(this->hud_viewport_) &&
            (this->config_.active_color == nullptr || !Contains(this->ConfigColorBounds(this->hud_viewport_), x, y)) &&
            (Contains(this->MovementBounds(module), x, y) || Contains(this->ScaleGrip(module), x, y));
    }

    auto hud_runtime::ToggleLayoutEditMode(void) -> void
    {
        this->FinishScale(false); this->FinishControllerMove(false); this->FinishInteraction();
        this->config_.edit_mode = !this->config_.edit_mode;
        this->config_.preview_selected = false;
        this->config_.tooltip.clear();
        if (this->config_.focus.active) this->RefreshControllerConfigFocus();
    }

    auto hud_runtime::ContextVisible(const hud_module& module) const -> bool
    {
        if (this->SetupVisible() && !this->SetupModuleVisible(module)) return false;
        return this->visible_ && (!std::string_view{module.Id()}.starts_with("notifications.group") || this->ContextVisible(this->notifications_)) &&
            (!std::string_view{module.Id()}.starts_with("world.") || this->ContextVisible(this->world_info_)) &&
            this->general_.Visibility().Visible(module.Id(), this->visibility_context_, this->ConfigPreview(module));
    }

    auto hud_runtime::TargetStatusVisible(void) const -> bool
    {
        return this->target_.HasStatusDetails() &&
            ((this->ContextVisible(this->target_) && this->target_.ShouldRender({})) ||
             (this->ContextVisible(this->target_recipient_) && this->target_recipient_.ShouldRender({})) ||
             (this->ContextVisible(this->subtarget_) && this->subtarget_.ShouldRender({})));
    }

    auto hud_runtime::UpdateVisibilityContext(void) -> void
    {
        this->visibility_context_ = this->visibility_source_.Read(this->core_, this->native_ui_.menu_visible);
        if (!this->GameWindowFocused()) this->SuspendWindowInput();
        else if (this->window_input_suspended_)
        {
            this->window_input_suspended_ = false;
            this->observed_native_focus_ = this->visibility_context_.window_focus;
            this->observed_native_focus_id_ = this->visibility_context_.window_focus_id;
            this->observed_native_focus_name_ = this->visibility_context_.window_focus_name;
        }
        this->general_.Visibility().SetContext(this->visibility_context_);
        this->chat_.SetDialogueActive(this->visibility_context_.dialogue_active);
        this->target_.SetMobInfoDisplayAllowed(this->ContextVisible(this->target_));
        this->target_recipient_.SetDisplayAllowed(this->ContextVisible(this->target_recipient_));
        this->subtarget_.SetDisplayAllowed(this->ContextVisible(this->subtarget_));
        this->pet_target_.SetDisplayAllowed(this->ContextVisible(this->pet_target_));
        if (this->scale_gesture_.module != nullptr && (!this->ModuleDisplayed(*this->scale_gesture_.module) || !this->GameWindowFocused() || this->visibility_context_.event_active)) this->FinishScale(false);
        const bool hiddenChat = !this->ContextVisible(this->chat_);
        const bool hiddenSecond = !this->ContextVisible(this->chat_second_);
        const bool hiddenReady = !this->ContextVisible(this->ready_check_);
        const bool pointerHidden = (this->chat_pointer_ != nullptr && !this->ContextVisible(*this->chat_pointer_)) ||
            (hiddenReady && this->ready_check_.PointerDown()) ||
            (this->hud_target_pressed_ && this->hud_target_module_ != nullptr && !this->ContextVisible(*this->hud_target_module_));
        if (pointerHidden)
        {
            this->visibility_pointer_release_ = true;
            this->hud_target_pressed_ = false;
            this->hud_target_module_ = nullptr;
            this->ReleaseChatInput();
        }
        if (hiddenChat)
        {
            this->chat_.Focus(false);
            this->chat_.CancelPointer();
            this->chat_native_context_suspended_ = false;
        }
        if (hiddenSecond) { this->chat_second_.Focus(false); this->chat_second_.CancelPointer(); }
        if (hiddenReady) { this->ready_check_.CancelPointer(); this->ready_check_.SetHover(-1, -1); }
        if (!this->SatchelInputAvailable(true)) this->SuspendSatchelInput();
        if (hiddenChat && hiddenSecond && !this->editing_ && !this->SetupVisible() && this->setup_.pointer_buttons == 0 && !this->ready_check_.PointerDown() && !this->controller_pointer_down_ &&
            !this->satchel_.PointerDown() && !this->actionbars_.PointerDown() && !this->treasure_.PointerDown() &&
            !(this->ActionBarsInputAvailable(false, true) && this->actionbars_.PointerOwns(static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_))) &&
            !(this->SatchelInputAvailable(true) && this->satchel_.PointerOwns(static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_))))
        {
            this->SetChatMouseCapture(false);
            this->ReleasePointerCapture();
        }
        this->UpdateNativeWindowFocus();
        this->UpdateControllerFocus();
    }

    auto hud_runtime::ControllerSurfaceVisible(void) const -> bool
    {
        switch (this->controller_surface_)
        {
        case controller_surface::notifications: return this->ContextVisible(this->notifications_) && this->notifications_.Detail(this->controller_notification_id_).has_value();
        case controller_surface::chat: return this->ContextVisible(this->chat_);
        case controller_surface::chat2: return this->ContextVisible(this->chat_second_);
        case controller_surface::party:
        {
            const std::array<const party_module*, 3> groups{&this->party_, &this->alliance_first_, &this->alliance_second_};
            return this->controller_party_group_ < groups.size() && this->ContextVisible(*groups[this->controller_party_group_]);
        }
        case controller_surface::enemies: return this->ContextVisible(this->enemy_list_);
        case controller_surface::ready_check: return this->ContextVisible(this->ready_check_);
        case controller_surface::equipment: return this->ContextVisible(this->equipment_);
        case controller_surface::satchel: return this->ContextVisible(this->satchel_) && this->satchel_.ShouldRender({});
        case controller_surface::treasure: return this->TreasureInputAvailable() && this->treasure_.InputFocused();
        case controller_surface::hotbar:
        case controller_surface::crossbar: return this->ActionBarsInputAvailable() && this->actionbars_.InputFocused();
        case controller_surface::target_status: return this->TargetStatusVisible();
        case controller_surface::buffs: return this->ContextVisible(this->buffs_) && this->buffs_.ShouldRender({});
        case controller_surface::debuffs: return this->ContextVisible(this->debuffs_) && this->debuffs_.ShouldRender({});
        case controller_surface::pet_status: return this->ContextVisible(this->pet_) && this->pet_.ShouldRender({}) && this->pet_.HasStatusDetails();
        default: return this->visible_;
        }
    }

    auto hud_runtime::ControllerPartyVisible(const size_t group) const -> bool
    {
        const std::array<const party_module*, 3> groups{&this->party_, &this->alliance_first_, &this->alliance_second_};
        return group < groups.size() && !this->ControllerNativeMenuOpen() && this->general_.PartyEnabled() &&
            this->ContextVisible(*groups[group]) && groups[group]->ShouldRender({});
    }

    auto module_visibility_source::Initialize(void) -> void
    {
        *this = {};
        const auto base = Ashita::Memory::GetModuleBase("FFXiMain.dll");
        const auto size = Ashita::Memory::GetModuleSize("FFXiMain.dll");
        if (base == 0 || size == 0) return;
        const auto operand = [&](const char* pattern, const uintptr_t offset) {
            const auto match = FindUniquePattern(base, size, pattern);
            uintptr_t address{};
            if (match == 0 ||
                !::Read(match + offset, address) || address < base || size < sizeof(uintptr_t) || address - base > size - sizeof(uintptr_t)) return uintptr_t{};
            return address;
        };
        this->menu_slot_ = operand("8B480C85C974??8B510885D274??3B05", 16);
        constexpr const char* eventPattern = "A0????????84C0741AA1????????85C0741166A1????????663B05????????0F94C0C3";
        this->event_slot_ = operand(eventPattern, 1);
        this->event_owner_slot_ = operand(eventPattern, 10);
        this->event_entity_slot_ = operand(eventPattern, 20);
        this->player_entity_slot_ = operand(eventPattern, 27);
        this->interface_object_ = operand("8B4424046A016A0050B9????????E8????????F6D81BC040C3", 10);
    }

    auto module_visibility_source::Read(IAshitaCore* core, const bool nativeMenuOpen) const -> module_visibility_context
    {
        module_visibility_context context{};
        context.menu_open = nativeMenuOpen;
        const auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        const auto target = memory != nullptr ? xiui::ashita::View(memory->GetTarget()) : nullptr;
        const auto party = memory != nullptr ? xiui::ashita::View(memory->GetParty()) : nullptr;
        const auto entity = memory != nullptr ? xiui::ashita::View(memory->GetEntity()) : nullptr;
        context.menu_open = context.menu_open || (target != nullptr && target->GetIsMenuOpen() != 0);
        if (party != nullptr && entity != nullptr)
        {
            const auto index = party->GetMemberTargetIndex(0);
            if (index != 0 && index < entity->GetEntityMapSize())
            {
                context.event_active = entity->GetStatus(index) == 4;
                context.dialogue_active = context.event_active || entity->GetStatusServer(index) == 4;
            }
            const auto playerId = party->GetMemberServerId(0);
            if (playerId != 0)
                for (uint32_t slot = 1; slot < 18; ++slot)
                    if (party->GetMemberIsActive(slot) != 0 && party->GetMemberServerId(slot) != 0 && party->GetMemberServerId(slot) != playerId)
                    {
                        context.in_party = true;
                        break;
                    }
        }
        uint8_t value{};
        uintptr_t eventOwner{};
        uint16_t eventEntity{}, playerEntity{};
        // The native enable byte alone does not establish an active event.
        context.event_available = ::Read(this->event_slot_, value) && value <= 1 &&
            ::Read(this->event_owner_slot_, eventOwner) && ::Read(this->event_entity_slot_, eventEntity) && ::Read(this->player_entity_slot_, playerEntity);
        if (context.event_available) context.event_active = value == 1 && eventOwner != 0 && eventEntity == playerEntity;
        context.dialogue_active = context.dialogue_active || context.event_active;
        context.interface_available = this->interface_object_ != 0 && ::Read(this->interface_object_ + 0xB4, value) && value <= 1;
        if (context.interface_available) context.interface_hidden = value == 1;
        std::array<char, 16> name{};
        context.menu_available = NativeMenuName(this->menu_slot_, name, &context.window_focus_id);
        context.macro_available = context.menu_available;
        if (context.menu_available)
        {
            context.window_focus_slot = this->menu_slot_;
            constexpr std::string_view ignored[]{"inline", "playermo", "chatctrl", "magselec", "magic", "abiselec", "ability", "mount"};
            constexpr std::string_view macros[]{"mcr1pall", "mcr2pall", "mcr1long", "mcr2long"};
            const auto id = NativeMenuShortName(name);
            std::memcpy(&context.window_focus_name, name.data() + 8, sizeof(context.window_focus_name));
            context.window_focus = id.empty() ? native_window_focus::unknown :
                id == "playermo" ? native_window_focus::commands :
                id == "fulllog" ? native_window_focus::chatlog :
                id == "chatctrl" ? native_window_focus::chatmode :
                id == "logwindo" || id == "log1win" ? native_window_focus::chat :
                id == "logwin2" || id == "log2win" ? native_window_focus::chat2 :
                id == "buff" ? native_window_focus::statuses : native_window_focus::other;
            context.menu_open = !id.empty() && std::find(std::begin(ignored), std::end(ignored), id) == std::end(ignored);
            context.macro_palette = std::find(std::begin(macros), std::end(macros), id) != std::end(macros);
            context.map_open = id.find("map") != std::string_view::npos;
        }
        return context;
    }
} // namespace xiui
