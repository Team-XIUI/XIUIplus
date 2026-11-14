#include "hud/hud_runtime.hpp"
#include "ashita_compat.hpp"

#include <array>
#include <utility>

namespace xiui
{
    auto hud_runtime::CancelLocalStatus(const status_effects_module& module, const status_effect_ref& effect) -> bool
    {
        auto* resources = this->core_ != nullptr ? this->core_->GetResourceManager() : nullptr;
        const auto* memory = this->core_ != nullptr ? this->core_->GetMemoryManager() : nullptr;
        const auto player = memory != nullptr ? xiui::ashita::View(memory->GetPlayer()) : nullptr;
        const auto party = memory != nullptr ? xiui::ashita::View(memory->GetParty()) : nullptr;
        const auto* icons = player != nullptr ? player->GetStatusIcons() : nullptr;
        if (icons == nullptr || !LocalStatusSnapshotReady({icons, 32})) icons = player != nullptr ? player->GetBuffs() : nullptr;
        const bool requested = resources != nullptr && icons != nullptr && party != nullptr &&
            module.CancelEffect(this->core_->GetPacketManager(), resources->GetStatusIconByIndex(effect.id), party->GetMemberServerId(0), {icons, 32}, effect);
        if (requested) this->ui_sounds_.Select(::GetTickCount64());
        return requested;
    }

    auto hud_runtime::StatusPointerAvailable(const status_effects_module& module) const -> bool
    {
        return this->visible_ && !this->editing_ && this->ContextVisible(module) && module.ShouldRender({}) && !this->ControllerNativeBusy() &&
            !this->chat_.HistoryExpanded() && !this->chat_second_.HistoryExpanded();
    }

    auto hud_runtime::StatusPointerOwns(const float x, const float y) const -> bool
    {
        if (this->status_pointer_ != nullptr) return true;
        for (const auto* module : {&this->buffs_, &this->debuffs_})
            if (this->StatusPointerAvailable(*module) && module->EffectAt(x, y)) return true;
        return false;
    }

    auto hud_runtime::HandleStatusMouse(const uint32_t message, const float x, const float y, const bool inputBlocked) -> bool
    {
        if (message == WM_RBUTTONUP && this->status_pointer_ != nullptr)
        {
            auto& module = *std::exchange(this->status_pointer_, nullptr);
            this->ReleasePointerCapture();
            const auto released = !this->PointerInputBlocked(inputBlocked) && this->StatusPointerAvailable(module) ? module.EffectAt(x, y) : std::nullopt;
            if (released && *released == this->status_pointer_effect_ && !this->CancelLocalStatus(module, *released) &&
                this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
                this->core_->GetChatManager()->Write(1, false, i18n::Text("[XIUI+] This effect cannot be cancelled."));
            return true;
        }
        if ((message != WM_RBUTTONDOWN && message != WM_RBUTTONDBLCLK) || this->PointerInputBlocked(inputBlocked)) return false;
        for (auto* module : {&this->buffs_, &this->debuffs_})
        {
            const auto effect = this->StatusPointerAvailable(*module) ? module->EffectAt(x, y) : std::nullopt;
            if (!effect) continue;
            this->status_pointer_ = module;
            this->status_pointer_effect_ = *effect;
            auto* properties = this->core_ != nullptr ? this->core_->GetProperties() : nullptr;
            const HWND window = properties != nullptr ? properties->GetFinalFantasyHwnd() : nullptr;
            if (window != nullptr)
            {
                ::SetCapture(window);
                if (::GetCapture() == window) this->mouse_capture_window_ = window;
            }
            this->SetChatMouseCapture(true);
            return true;
        }
        return false;
    }

    auto hud_runtime::ControllerStatusBounds(void) const -> rect
    {
        if (this->controller_surface_ == controller_surface::buffs) return this->buffs_.ControllerBounds();
        if (this->controller_surface_ == controller_surface::debuffs) return this->debuffs_.ControllerBounds();
        if (this->controller_surface_ == controller_surface::target_status) return this->target_.ControllerStatusBounds();
        if (this->controller_surface_ == controller_surface::pet_status) return this->pet_.ControllerStatusBounds();
        if (!this->controller_status_details_) return {};
        if (this->controller_surface_ == controller_surface::enemies) return this->enemy_list_.ControllerStatusBounds();
        if (this->controller_surface_ == controller_surface::party)
        {
            const std::array<const party_module*, 3> groups{&this->party_, &this->alliance_first_, &this->alliance_second_};
            if (this->controller_party_group_ < groups.size()) return groups[this->controller_party_group_]->ControllerStatusBounds();
        }
        return {};
    }

    auto hud_runtime::HandleControllerStatus(const controller_action action) -> bool
    {
        if (this->controller_surface_ == controller_surface::pet_status)
        {
            if (action == controller_action::cancel) this->ReleaseControllerFocus();
            else if (action == controller_action::up || action == controller_action::left || action == controller_action::previous_tab)
                this->pet_.MoveControllerStatus(-1);
            else if (action == controller_action::down || action == controller_action::right || action == controller_action::next_tab)
                this->pet_.MoveControllerStatus(1);
            return true;
        }
        if (this->controller_surface_ == controller_surface::buffs || this->controller_surface_ == controller_surface::debuffs)
        {
            auto& module = this->controller_surface_ == controller_surface::buffs ? this->buffs_ : this->debuffs_;
            if (action == controller_action::cancel) this->ReleaseControllerFocus();
            else if (action == controller_action::confirm)
            {
                const auto effect = module.ControllerEffectRef();
                const bool requested = effect && this->CancelLocalStatus(module, *effect);
                this->controller_hint_ = requested ? "Effect cancellation requested." : "This effect cannot be cancelled.";
                if (!requested && this->keyboard_window_focus_ && this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
                    this->core_->GetChatManager()->Write(1, false, i18n::Text("[XIUI+] This effect cannot be cancelled."));
            }
            else if (action == controller_action::up || action == controller_action::down)
                module.MoveControllerBrowse(action == controller_action::up ? -1 : 1);
            else if (action == controller_action::left || action == controller_action::right ||
                action == controller_action::previous_tab || action == controller_action::next_tab)
                module.MoveControllerBrowseHorizontal(action == controller_action::left || action == controller_action::previous_tab ? -1 : 1);
            return true;
        }
        if (this->controller_surface_ == controller_surface::target_status)
        {
            if (action == controller_action::cancel) this->ReleaseControllerFocus();
            else if (action == controller_action::up || action == controller_action::left || action == controller_action::previous_tab)
                this->target_.MoveControllerStatus(-1);
            else if (action == controller_action::down || action == controller_action::right || action == controller_action::next_tab)
                this->target_.MoveControllerStatus(1);
            return true;
        }
        const std::array<party_module*, 3> groups{&this->party_, &this->alliance_first_, &this->alliance_second_};
        auto* party = this->controller_surface_ == controller_surface::party && this->controller_party_group_ < groups.size()
            ? groups[this->controller_party_group_] : nullptr;
        const bool enemy = this->controller_surface_ == controller_surface::enemies;
        if (!this->controller_status_details_)
        {
            if (party != nullptr && action == controller_action::next_tab)
                this->controller_status_details_ = party->BeginControllerStatus();
            else if (enemy && action == controller_action::actions)
                this->controller_status_details_ = this->enemy_list_.BeginControllerStatus();
            else return false;
            if (!this->controller_status_details_) this->controller_hint_ = "No visible status effects to inspect.";
            return true;
        }
        if (party == nullptr && !enemy) { this->controller_status_details_ = false; return false; }
        if (action == controller_action::cancel || (party != nullptr && action == controller_action::actions))
        {
            if (party != nullptr) party->EndControllerStatus();
            else this->enemy_list_.EndControllerStatus();
            this->controller_status_details_ = false;
            return action == controller_action::cancel;
        }
        const int direction = action == controller_action::up || action == controller_action::left || action == controller_action::previous_tab ? -1 :
            action == controller_action::down || action == controller_action::right || action == controller_action::next_tab ? 1 : 0;
        if (direction != 0)
        {
            if (party != nullptr) party->MoveControllerStatus(direction);
            else this->enemy_list_.MoveControllerStatus(direction);
        }
        return true;
    }

    auto hud_runtime::DrawStatusTooltips(const D3DVIEWPORT8& viewport, const module_render_context& context) -> void
    {
        auto* properties = this->core_ != nullptr ? this->core_->GetProperties() : nullptr;
        if (properties == nullptr || ::GetForegroundWindow() != properties->GetFinalFantasyHwnd()) return;
        auto* resources = this->core_->GetResourceManager();
        const float x = static_cast<float>(this->mouse_x_), y = static_cast<float>(this->mouse_y_);
        if (this->ControllerCaptured() || this->KeyboardStatusFocusBounds().width > 0)
        {
            if (this->controller_surface_ == controller_surface::buffs)
                this->buffs_.RenderStatusTooltip(context, resources, x, y, true);
            else if (this->controller_surface_ == controller_surface::debuffs)
                this->debuffs_.RenderStatusTooltip(context, resources, x, y, true);
            else if (this->controller_surface_ == controller_surface::target_status)
                this->target_.RenderStatusTooltip(context, resources, x, y, true);
            else if (this->controller_surface_ == controller_surface::pet_status)
                this->pet_.RenderStatusTooltip(context, resources, x, y, true);
            else if (this->controller_status_details_ && this->controller_surface_ == controller_surface::enemies)
                this->enemy_list_.RenderStatusTooltip(context, resources, x, y, true);
            else if (this->controller_status_details_ && this->controller_surface_ == controller_surface::party)
            {
                const std::array<const party_module*, 3> groups{&this->party_, &this->alliance_first_, &this->alliance_second_};
                groups[this->controller_party_group_]->RenderStatusTooltip(context, resources, x, y, true);
            }
            return;
        }
        if (this->ProfileSurface(viewport) || this->config_.choice_input.Opened() || this->interaction_ != interaction::none ||
            this->ChatAt(x, y) != nullptr || this->controller_surface_ == controller_surface::picker) return;
        if (this->editing_ && (Contains(this->ConfigVisibleBounds(), x, y) ||
            (this->config_.active_color != nullptr && Contains(this->ConfigColorBounds(viewport), x, y)))) return;
        if (this->TargetStatusVisible()) this->target_.RenderStatusTooltip(context, resources, x, y);
        if (this->ModuleDisplayed(this->pet_))
            this->pet_.RenderStatusTooltip(context, resources, x, y);
        if (this->ContextVisible(this->enemy_list_) && this->enemy_list_.ShouldRender({})) this->enemy_list_.RenderStatusTooltip(context, resources, x, y);
        for (const auto* group : {&this->party_, &this->alliance_first_, &this->alliance_second_})
            if (this->ContextVisible(*group) && group->ShouldRender({})) group->RenderStatusTooltip(context, resources, x, y);
        for (const auto* module : {&this->buffs_, &this->debuffs_})
            if (this->ContextVisible(*module)) module->RenderStatusTooltip(context, resources, x, y);
    }
}
