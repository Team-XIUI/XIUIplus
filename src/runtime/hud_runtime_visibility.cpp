#include "config/module_visibility.hpp"
#include "hud_runtime.hpp"
#include "Memory.h"

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

    auto MenuName(const uintptr_t slot, std::array<char, 16>& name) -> bool
    {
        uintptr_t first{}, resource{};
        if (!Read(slot, first)) return false;
        if (first == 0) return true;
        return Read(first + 4, resource) && resource != 0 && Read(resource + 0x46, name) &&
            std::all_of(name.begin(), name.end(), [](const unsigned char value) { return value == 0 || value == ' ' || (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9'); });
    }

    auto ShortName(const std::array<char, 16>& name) -> std::string_view
    {
        std::string_view text{name.data(), name.size()};
        const auto end = text.find('\0');
        if (end != std::string_view::npos) text = text.substr(0, end);
        const auto last = text.find_last_not_of(' ');
        if (last == std::string_view::npos) return {};
        text = text.substr(0, last + 1);
        if (text.starts_with("menu ")) text.remove_prefix(text.find_first_not_of(' ', 4));
        return text;
    }
}

namespace xiui
{
    auto hud_runtime::ContextVisible(const hud_module& module) const -> bool
    {
        return this->visible_ && (!std::string_view{module.Id()}.starts_with("notifications.group") || this->ContextVisible(this->notifications_)) &&
            (!std::string_view{module.Id()}.starts_with("world.") || this->ContextVisible(this->world_info_)) &&
            this->general_.Visibility().Visible(module.Id(), this->visibility_context_, this->editing_);
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
        this->general_.Visibility().SetContext(this->visibility_context_);
        this->target_.SetMobInfoDisplayAllowed(this->ContextVisible(this->target_));
        this->target_recipient_.SetDisplayAllowed(this->ContextVisible(this->target_recipient_));
        this->subtarget_.SetDisplayAllowed(this->ContextVisible(this->subtarget_));
        this->pet_target_.SetDisplayAllowed(this->ContextVisible(this->pet_target_));
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
        if (!this->SatchelInputAvailable()) this->SuspendSatchelInput();
        if (hiddenChat && hiddenSecond && !this->editing_ && !this->ready_check_.PointerDown() && !this->controller_pointer_down_ &&
            !this->satchel_.PointerDown() && !this->actionbars_.PointerDown() && !this->treasure_.PointerDown() &&
            !(this->ActionBarsInputAvailable() && this->actionbars_.PointerOwns(static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_))) &&
            !(this->SatchelInputAvailable() && this->satchel_.PointerOwns(static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_))))
        {
            this->SetChatMouseCapture(false);
            this->ReleasePointerCapture();
        }
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
        case controller_surface::treasure: return !this->ControllerNativeMenuOpen() && this->ContextVisible(this->treasure_) && this->treasure_.ShouldRender({}) && this->treasure_.InputFocused();
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
            const auto match = Ashita::Memory::FindPattern(base, size, pattern, 0, 0);
            uintptr_t address{};
            if (match == 0 || Ashita::Memory::FindPattern(base, size, pattern, 0, 1) != 0 ||
                !::Read(match + offset, address) || address < base || size < sizeof(uintptr_t) || address - base > size - sizeof(uintptr_t)) return uintptr_t{};
            return address;
        };
        this->menu_slot_ = operand("8B480C85C974??8B510885D274??3B05", 16);
        this->event_slot_ = operand("A0????????84C0741AA1????????85C0741166A1????????663B05????????0F94C0C3", 1);
        this->interface_object_ = operand("8B4424046A016A0050B9????????E8????????F6D81BC040C3", 10);
    }

    auto module_visibility_source::Read(IAshitaCore* core, const bool nativeMenuOpen) const -> module_visibility_context
    {
        module_visibility_context context{};
        context.menu_open = nativeMenuOpen;
        const auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        const auto* target = memory != nullptr ? memory->GetTarget() : nullptr;
        const auto* party = memory != nullptr ? memory->GetParty() : nullptr;
        const auto* entity = memory != nullptr ? memory->GetEntity() : nullptr;
        context.menu_open = context.menu_open || (target != nullptr && target->GetIsMenuOpen() != 0);
        if (party != nullptr && entity != nullptr)
        {
            const auto index = party->GetMemberTargetIndex(0);
            if (index != 0 && index < entity->GetEntityMapSize()) context.event_active = entity->GetStatus(index) == 4;
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
        context.event_available = ::Read(this->event_slot_, value) && value <= 1;
        if (context.event_available) context.event_active = value == 1;
        context.interface_available = this->interface_object_ != 0 && ::Read(this->interface_object_ + 0xB4, value) && value <= 1;
        if (context.interface_available) context.interface_hidden = value == 1;
        std::array<char, 16> name{};
        context.menu_available = MenuName(this->menu_slot_, name);
        context.macro_available = context.menu_available;
        if (context.menu_available)
        {
            constexpr std::string_view ignored[]{"inline", "playermo", "chatctrl", "magselec", "magic", "abiselec", "ability", "mount"};
            constexpr std::string_view macros[]{"mcr1pall", "mcr2pall", "mcr1long", "mcr2long"};
            const auto id = ShortName(name);
            context.menu_open = !id.empty() && std::find(std::begin(ignored), std::end(ignored), id) == std::end(ignored);
            context.macro_palette = std::find(std::begin(macros), std::end(macros), id) != std::end(macros);
            context.map_open = id.find("map") != std::string_view::npos;
        }
        return context;
    }
} // namespace xiui
