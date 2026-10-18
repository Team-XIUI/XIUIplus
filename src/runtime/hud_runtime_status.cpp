#include "hud_runtime.hpp"

#include <array>

namespace xiui
{
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
        if (this->ControllerCaptured())
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
        if (this->ProfileSurface(viewport) || this->choice_input_.Opened() || this->interaction_ != interaction::none ||
            this->ChatAt(x, y) != nullptr || this->controller_surface_ == controller_surface::picker) return;
        if (this->editing_ && (Contains(this->ConfigVisibleBounds(), x, y) ||
            (this->active_color_ != nullptr && Contains(this->ConfigLayout().ColorEditor(viewport), x, y)))) return;
        if (this->TargetStatusVisible()) this->target_.RenderStatusTooltip(context, resources, x, y);
        if (this->ContextVisible(this->pet_) && (this->editing_ ? this->pet_.PreviewVisible() : this->pet_.ShouldRender({})))
            this->pet_.RenderStatusTooltip(context, resources, x, y);
        if (this->ContextVisible(this->enemy_list_) && this->enemy_list_.ShouldRender({})) this->enemy_list_.RenderStatusTooltip(context, resources, x, y);
        for (const auto* group : {&this->party_, &this->alliance_first_, &this->alliance_second_})
            if (this->ContextVisible(*group) && group->ShouldRender({})) group->RenderStatusTooltip(context, resources, x, y);
        for (const auto* module : {&this->buffs_, &this->debuffs_})
            if (this->ContextVisible(*module)) module->RenderStatusTooltip(context, resources, x, y);
    }
}
