#include "hud_runtime.hpp"
#include "input/controller_movement_layout.hpp"
#include "config/contributor_list.hpp"

#include <cmath>
#include <cstdio>

namespace xiui
{
    auto hud_runtime::ConfigSidebarScrollbar(void) const -> ui::config_scrollbar
    {
        if (!this->editing_ || this->config_.basic || this->config_.collapsed) return {};
        this->ConfigSidebarRows();
        auto bounds = this->ConfigSidebarBounds();
        bounds.x += bounds.width - 8; bounds.width = 8;
        return {bounds, static_cast<int>(std::ceil(this->config_.sidebar_animation.content_height)),
            static_cast<int>(bounds.height), static_cast<int>(this->config_.sidebar_animation.scroll)};
    }

    auto hud_runtime::ConfigScrollbar(void) const -> ui::config_scrollbar
    {
        if (!this->editing_ || this->config_.collapsed || this->config_.module == nullptr) return {};
        const auto layout = this->ConfigLayout();
        if (!layout.HasScrollbar()) return {};
        auto track = layout.ScrollbarTrack();
        if (this->IsXIUIContributors())
        {
            const int page = std::max(1, static_cast<int>(track.height / 22));
            return {track, page + ui::contributor_list_layout{layout}.MaximumScroll(), page, this->config_.scroll};
        }
        const int page = layout.VisibleRows();
        track.height = std::min(track.height, layout.RowsHeight(page));
        return {track, layout.RowForSlot(layout.MaximumScroll(static_cast<int>(this->GroupOptionCount()))) + page, page, layout.RowForSlot(this->config_.scroll)};
    }

    auto hud_runtime::ProcessConfigScrollbarClick(void) -> bool
    {
        const float x = static_cast<float>(this->mouse_x_), y = static_cast<float>(this->mouse_y_);
        const auto sidebar = this->ConfigSidebarScrollbar();
        const bool side = sidebar.Visible() && Contains(sidebar.track, x, y);
        const auto scrollbar = side ? sidebar : this->ConfigScrollbar();
        if (!scrollbar.Visible() || !Contains(scrollbar.track, x, y)) return false;
        this->FinishConfigInput(true, false);
        this->config_.active_color = nullptr;
        this->config_.active_slider = nullptr;
        this->config_.sidebar_scrolling = side;
        if (side) this->config_.sidebar_animation.reveal = false;
        if (Contains(scrollbar.Thumb(), x, y))
        {
            this->config_.scrollbar_grab = y - scrollbar.Thumb().y;
            this->interaction_ = interaction::option_scrollbar;
        }
        else
        {
            if (side) this->config_.sidebar_animation.scroll = static_cast<float>(scrollbar.PageAt(y));
            else
            {
                this->config_.scroll = scrollbar.PageAt(y) * (this->IsXIUIContributors() ? 1 : this->ConfigLayout().Columns());
                this->ClampOptionScroll();
            }
        }
        return true;
    }

    auto hud_runtime::UpdateConfigScrollbar(void) -> void
    {
        const auto scrollbar = this->config_.sidebar_scrolling ? this->ConfigSidebarScrollbar() : this->ConfigScrollbar();
        if (!scrollbar.Visible()) return;
        if (this->config_.sidebar_scrolling)
        {
            this->config_.sidebar_animation.scroll = static_cast<float>(scrollbar.PositionAt(static_cast<float>(this->mouse_y_), this->config_.scrollbar_grab));
            return;
        }
        this->config_.scroll = scrollbar.PositionAt(static_cast<float>(this->mouse_y_), this->config_.scrollbar_grab) * (this->IsXIUIContributors() ? 1 : this->ConfigLayout().Columns());
        this->ClampOptionScroll();
    }

    auto hud_runtime::DrawConfigScrollbar(void) -> void
    {
        for (const bool side : {false, true})
        {
            const auto scrollbar = side ? this->ConfigSidebarScrollbar() : this->ConfigScrollbar();
            const bool dragging = this->interaction_ == interaction::option_scrollbar && this->config_.sidebar_scrolling == side;
            const bool hovered = this->interaction_ == interaction::none && !this->config_.choice_input.Opened() && !this->profile_name_.Focused() &&
                !this->profile_dropdown_ && !this->profile_actions_ && !this->config_.input_mode_menu &&
                Contains(scrollbar.track, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
            this->config_controls_.Scrollbar(scrollbar, hovered, dragging);
        }
    }

    auto hud_runtime::LayoutBounds(const hud_module& module) const -> rect
    {
        const auto* chat = &module == &this->chat_ ? &this->chat_ : &module == &this->chat_second_ ? &this->chat_second_ : nullptr;
        if (chat != nullptr) return chat->VisualBounds(this->design_system_);
        const auto* party = &module == &this->party_ ? &this->party_ : &module == &this->alliance_first_ ? &this->alliance_first_ : &module == &this->alliance_second_ ? &this->alliance_second_ : nullptr;
        return party != nullptr ? party->PlacementBounds(this->ConfigPreview(module)) : module.State().EditBounds();
    }

    auto hud_runtime::SnapModuleLayout(const D3DVIEWPORT8& viewport) -> void
    {
        this->ResolveHudTransforms(viewport);
        this->layout_snap_targets_.clear();
        for (const auto* module : this->modules_.All())
            if (module != this->interaction_module_ && module->HasEditableBounds() && this->ModuleDisplayed(*module))
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

    auto hud_runtime::MovementSelectionModule(void) const -> hud_module*
    {
        if (this->config_.basic) return this->config_.layout_selection;
        if (this->config_.module == &this->target_ && this->target_.ConfigView() == 1) return const_cast<target_child_module*>(&this->target_recipient_);
        if (this->config_.module == &this->target_ && this->target_.ConfigView() == 2) return const_cast<target_child_module*>(&this->subtarget_);
        if (this->config_.module == &this->target_ && this->target_.ConfigView() == 3) return const_cast<world_target_bar_module*>(&this->world_target_scale_);
        if (this->config_.module == &this->actionbars_ && this->actionbars_.SelectedBar() > 0)
            return this->actionbars_.SelectedBar() <= this->actionbar_children_.size() ?
                const_cast<actionbar_module*>(&this->actionbar_children_[this->actionbars_.SelectedBar() - 1]) : nullptr;
        if (this->config_.module == &this->world_info_ && this->world_info_.Independent())
            return const_cast<world_info_part_module*>(&this->world_parts_[this->world_info_.SelectedPart()]);
        if (this->config_.module == &this->notifications_ && this->notifications_.SelectedGroup() > 0)
            return const_cast<notification_group_module*>(&this->notification_groups_[this->notifications_.SelectedGroup() - 1]);
        if (this->config_.module == &this->target_ && this->target_.ConfigView() == 0 && this->config_.section == 9 && this->target_cast_.HasEditableBounds())
            return const_cast<target_cast_module*>(&this->target_cast_);
        if (this->config_.module == &this->target_ && this->target_.ConfigView() == 0 && this->config_.section == 11)
        {
            const auto group = this->target_.Settings().mob_info.selected_group;
            if (group > 0 && static_cast<size_t>(group) <= this->mob_info_groups_.size())
            {
                auto* module = const_cast<target_mob_info_module*>(&this->mob_info_groups_[static_cast<size_t>(group - 1)]);
                if (module->HasEditableBounds()) return module;
            }
        }
        return this->config_.module;
    }

    auto hud_runtime::MovementToolbar(const D3DVIEWPORT8& viewport) const -> std::optional<ui::movement_toolbar>
    {
        if (!this->editing_ || this->MovementSelectionModule() == nullptr || !this->MovementSelectionModule()->HasEditableBounds() || !this->ModuleDisplayed(*this->MovementSelectionModule())) return std::nullopt;
        const auto* module = this->MovementSelectionModule();
        if (!module->CanMove() && !module->ConfigScaleEditable()) return std::nullopt;
        return ui::movement_toolbar::AtTop({0, 0, static_cast<float>(viewport.Width), static_cast<float>(viewport.Height)},
            module->SupportsScale() && module->EditGrip() != edit_grip_kind::none);
    }

    auto hud_runtime::MovementToolbarTarget(const D3DVIEWPORT8& viewport) const -> uint64_t
    {
        const auto toolbar = this->MovementToolbar(viewport);
        if (!toolbar) return 0;
        for (size_t index = 0; index < 3; ++index)
            if (Contains(toolbar->Button(index), static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_)))
                return (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(this->MovementSelectionModule())) << 32) | (0x50000 + index);
        return 0;
    }

    auto hud_runtime::ProcessMovementToolbarClick(const D3DVIEWPORT8& viewport) -> bool
    {
        const auto toolbar = this->MovementToolbar(viewport);
        if (!toolbar || !Contains(toolbar->bounds, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_))) return false;
        this->interaction_ = interaction::module_toolbar;
        bool changed{};
        if (Contains(toolbar->Button(0), static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_)))
            changed = this->CenterModuleLayout(*this->MovementSelectionModule(), 0, viewport);
        if (Contains(toolbar->Button(1), static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_)))
            changed = this->CenterModuleLayout(*this->MovementSelectionModule(), 1, viewport) || changed;
        if (Contains(toolbar->Button(2), static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_))) this->BeginScale(*this->MovementSelectionModule(), true);
        if (changed)
            this->MovementSelectionModule()->OnPositionChanged();
        return true;
    }

    auto hud_runtime::CenterModuleLayout(hud_module& module, const size_t axis, const D3DVIEWPORT8& viewport) -> bool
    {
        if (axis > 1 || !module.State().enabled || !module.HasEditableBounds() || !module.CanMove() || !this->ModuleDisplayed(module) || !module.PositionBoundsReady()) return false;
        const auto visual = this->MovementBounds(module);
        auto& bounds = module.State().bounds;
        if (axis == 0) bounds.x += std::round((viewport.Width - visual.width) * 0.5f) - visual.x;
        else bounds.y += std::round((viewport.Height - visual.height) * 0.5f) - visual.y;
        this->ResolveHudTransforms(viewport);
        ClampMovementPosition(bounds, this->MovementBounds(module), viewport);
        return true;
    }

    auto hud_runtime::DrawMovementToolbar(const D3DVIEWPORT8& viewport) -> void
    {
        const auto toolbar = this->MovementToolbar(viewport);
        if (!toolbar) return;
        this->config_controls_.Panel(toolbar->bounds);
        auto style = this->design_system_.GetTheme().caption_text;
        style.scale = 0.7f;
        style.alignment = ui::text_alignment::center;
        const char* tooltip{};
        rect tooltipAnchor{};
        for (size_t index = 0; index < 3; ++index)
        {
            const auto button = toolbar->Button(index);
            if (button.width <= 0) continue;
            const bool hovered = this->interaction_ == interaction::none && !Contains(this->ConfigVisibleBounds(), static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_)) &&
                !this->ProfileSurface(viewport) && !this->config_.choice_input.Opened() &&
                (this->config_.active_color == nullptr || !Contains(this->ConfigLayout().ColorEditor(viewport), static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_))) &&
                Contains(button, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
            const bool resize = this->MovementSelectionModule()->EditGrip() == edit_grip_kind::resize;
            if (index == 2) this->config_controls_.Button(button, resize ? "Resize" : "Scale", style, hovered);
            else this->config_controls_.IconButton(button, index == 0 ? ui::icon_glyph::center_x : ui::icon_glyph::center_y, hovered);
            if (hovered) { tooltip = index == 0 ? "Center horizontally" : index == 1 ? "Center vertically" : resize ? "Resize viewport (Alt+F9). Arrows adjust width/height without changing fonts; Enter saves; Escape restores." : "Scale (Alt+F9). Arrows adjust; Enter saves; Escape restores."; tooltipAnchor = button; }
        }
        const auto* module = this->MovementSelectionModule();
        auto name = style; name.alignment = ui::text_alignment::left;
        this->config_controls_.Text(module->DisplayName(), toolbar->Label(), name);
        const auto& bounds = module->State().bounds;
        char coordinates[64]{}; std::snprintf(coordinates, sizeof(coordinates), "X %.0f  Y %.0f", bounds.x, bounds.y);
        this->config_controls_.Text(coordinates, toolbar->Coordinates(), style);
        if (tooltip != nullptr) this->config_controls_.Tooltip(tooltip, tooltipAnchor, viewport);
    }
}
