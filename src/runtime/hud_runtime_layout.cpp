#include "hud_runtime.hpp"

#include <cmath>
#include <cstdio>

namespace xiui
{
    auto hud_runtime::MovementBounds(const hud_module& module) const -> rect
    {
        const auto* chat = &module == &this->chat_ ? &this->chat_ : &module == &this->chat_second_ ? &this->chat_second_ : nullptr;
        if (chat != nullptr) return chat->VisualBounds(this->design_system_);
        const auto* party = &module == &this->party_ ? &this->party_ : &module == &this->alliance_first_ ? &this->alliance_first_ : &module == &this->alliance_second_ ? &this->alliance_second_ : nullptr;
        return party != nullptr ? party->VisualBounds(this->editing_) : module.State().EditBounds();
    }

    auto hud_runtime::SnapModuleLayout(const D3DVIEWPORT8& viewport) -> void
    {
        this->layout_snap_targets_.clear();
        for (const auto* module : this->modules_.All())
            if (module != this->interaction_module_ && module->HasEditableBounds() && module->PreviewVisible())
                this->layout_snap_targets_.push_back(this->MovementBounds(*module));
        const auto moving = this->MovementBounds(*this->interaction_module_);
        this->layout_snap_ = ui::SnapLayoutBounds(moving, this->layout_snap_targets_,
            {0, 0, static_cast<float>(viewport.Width), static_cast<float>(viewport.Height)}, this->general_.Grid());
        auto& bounds = this->interaction_module_->State().bounds;
        bounds.x += this->layout_snap_.bounds.x - moving.x;
        bounds.y += this->layout_snap_.bounds.y - moving.y;
    }

    auto hud_runtime::DrawLayoutBackground(const rect& viewport) -> void
    {
        const auto& grid = this->general_.Grid();
        if (grid.visible) this->design_system_.DrawLayoutGrid(viewport, grid.spacing, grid.opacity);
        if (this->interaction_ == interaction::drag_module && this->drag_moved_)
        {
            const auto color = (this->design_system_.GetTheme().colors.accent & 0x00FFFFFF) | 0x90000000;
            if (this->layout_snap_.vertical) this->design_system_.FillRect({*this->layout_snap_.vertical, viewport.y, 1, viewport.height}, color);
            if (this->layout_snap_.horizontal) this->design_system_.FillRect({viewport.x, *this->layout_snap_.horizontal, viewport.width, 1}, color);
        }
    }

    auto hud_runtime::MovementToolbar(const D3DVIEWPORT8& viewport) const -> std::optional<ui::movement_toolbar>
    {
        if (!this->editing_ || this->active_module_ == nullptr || !this->active_module_->HasEditableBounds() || !this->active_module_->PreviewVisible()) return std::nullopt;
        return ui::movement_toolbar::Above(this->MovementBounds(*this->active_module_),
            {0, 0, static_cast<float>(viewport.Width), static_cast<float>(viewport.Height)});
    }

    auto hud_runtime::MovementToolbarTarget(const D3DVIEWPORT8& viewport) const -> uint64_t
    {
        const auto toolbar = this->MovementToolbar(viewport);
        if (!toolbar) return 0;
        for (size_t index = 0; index < 2; ++index)
            if (Contains(toolbar->Button(index), static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_)))
                return (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(this->active_module_)) << 32) | (0x50000 + index);
        return 0;
    }

    auto hud_runtime::ProcessMovementToolbarClick(const D3DVIEWPORT8& viewport) -> bool
    {
        const auto toolbar = this->MovementToolbar(viewport);
        if (!toolbar || !Contains(toolbar->bounds, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_))) return false;
        this->interaction_ = interaction::module_toolbar;
        const auto visual = this->MovementBounds(*this->active_module_);
        auto& bounds = this->active_module_->State().bounds;
        bool changed{};
        if (Contains(toolbar->Button(0), static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_)))
        { bounds.x += std::round((viewport.Width - visual.width) * 0.5f) - visual.x; changed = true; }
        if (Contains(toolbar->Button(1), static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_)))
        { bounds.y += std::round((viewport.Height - visual.height) * 0.5f) - visual.y; changed = true; }
        if (changed)
        {
            this->ClampLayout(viewport);
            this->active_module_->OnPositionChanged();
        }
        return true;
    }

    auto hud_runtime::DrawMovementToolbar(const D3DVIEWPORT8& viewport) -> void
    {
        const auto toolbar = this->MovementToolbar(viewport);
        if (!toolbar) return;
        this->config_controls_.Panel(toolbar->bounds);
        auto style = this->design_system_.GetTheme().caption_text;
        style.alignment = ui::text_alignment::center;
        for (size_t index = 0; index < 2; ++index)
        {
            const auto button = toolbar->Button(index);
            const bool hovered = this->interaction_ == interaction::none && !Contains(this->ConfigVisibleBounds(), static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_)) &&
                !this->ProfileSurface(viewport) && !this->choice_input_.Opened() &&
                (this->active_color_ == nullptr || !Contains(this->ConfigLayout().ColorEditor(viewport), static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_))) &&
                Contains(button, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
            this->config_controls_.Button(button, index == 0 ? "Center X" : "Center Y", style, hovered);
        }
        const auto& bounds = this->active_module_->State().bounds;
        char coordinates[64]{}; std::snprintf(coordinates, sizeof(coordinates), "X: %.0f   Y: %.0f", bounds.x, bounds.y);
        this->config_controls_.Text(coordinates, toolbar->Coordinates(), style);
    }
}
