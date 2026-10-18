#include "hud_runtime.hpp"
#include "input/controller_movement_layout.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
    using action = xiui::controller_action;
    using region = xiui::config_controller_region;
    auto Direction(const action value) -> int
    { return value == action::left || value == action::up ? -1 : 1; }
    auto Directional(const action value) -> bool
    { return value == action::up || value == action::down || value == action::left || value == action::right; }
}

namespace xiui
{
    auto hud_runtime::ControllerMovableModules(void) const -> std::vector<hud_module*>
    {
        std::vector<hud_module*> modules;
        for (auto* module : this->modules_.All())
            if (module->State().enabled && module->HasEditableBounds() && module->PreviewVisible() && module->PositionBoundsReady()) modules.push_back(module);
        std::sort(modules.begin(), modules.end(), [](const auto* left, const auto* right) { return std::strcmp(left->DisplayName(), right->DisplayName()) < 0; });
        return modules;
    }

    auto hud_runtime::FinishControllerMove(const bool apply) -> void
    {
        auto& focus = this->controller_config_;
        if (!focus.moving) return;
        auto* module = static_cast<hud_module*>(nullptr);
        for (auto* candidate : this->modules_.All())
            if (focus.moving_id == candidate->Id()) { module = candidate; break; }
        if (module != nullptr)
        {
            if (!apply) module->State().bounds = focus.move_original;
            module->OnPositionChanged();
        }
        this->config_collapsed_ = focus.move_collapsed;
        focus.moving = false; focus.fine = false;
        focus.region = region::movement; focus.id = focus.moving_id; focus.moving_id.clear();
        this->layout_snap_ = {}; this->layout_snap_targets_.clear();
        if (module != nullptr) this->SaveSettings();
    }

    auto hud_runtime::HandleControllerMove(const controller_action value, const bool repeated) -> bool
    {
        auto& focus = this->controller_config_;
        const auto modules = this->ControllerMovableModules();
        const auto found = std::find_if(modules.begin(), modules.end(), [&](const auto* module) { return focus.moving_id == module->Id(); });
        if (found == modules.end()) { this->FinishControllerMove(false); return true; }
        auto& module = **found;
        if (value == action::cancel) { this->FinishControllerMove(false); return true; }
        if (value == action::confirm)
        {
            if (!repeated) { this->ui_sounds_.Select(::GetTickCount64()); this->FinishControllerMove(true); }
            return true;
        }
        if (value == action::actions && !repeated)
        { focus.fine = !focus.fine; this->ui_sounds_.Select(::GetTickCount64()); return true; }
        if ((value == action::next_tab || value == action::previous_tab) && !repeated)
        {
            if (this->CenterModuleLayout(module, value == action::next_tab ? 0 : 1, focus.viewport))
            { focus.move_desired = module.State().bounds; this->layout_snap_ = {}; this->ui_sounds_.Select(::GetTickCount64()); }
            return true;
        }
        if (!Directional(value)) return true;
        const auto& grid = this->general_.Grid();
        const float step = focus.fine ? 1.0f : std::clamp(std::round(grid.amount), 1.0f, 20.0f);
        if (value == action::left || value == action::right) focus.move_desired.x += Direction(value) * step;
        else focus.move_desired.y += Direction(value) * step;
        module.State().bounds = focus.move_desired;
        ClampMovementPosition(module.State().bounds, this->MovementBounds(module), focus.viewport);
        focus.move_desired = module.State().bounds;
        this->layout_snap_ = {};
        if (!focus.fine)
        {
            this->layout_snap_targets_.clear();
            for (const auto* other : modules)
                if (other != &module) this->layout_snap_targets_.push_back(this->MovementBounds(*other));
            const auto visual = this->MovementBounds(module);
            this->layout_snap_ = ui::SnapLayoutBounds(visual, this->layout_snap_targets_,
                {0, 0, static_cast<float>(focus.viewport.Width), static_cast<float>(focus.viewport.Height)}, grid);
            auto& bounds = module.State().bounds;
            bounds.x += this->layout_snap_.bounds.x - visual.x;
            bounds.y += this->layout_snap_.bounds.y - visual.y;
            ClampMovementPosition(bounds, this->MovementBounds(module), focus.viewport);
        }
        return true;
    }

    auto hud_runtime::DrawControllerMovement(const D3DVIEWPORT8& viewport) -> void
    {
        auto& focus = this->controller_config_;
        const auto& theme = this->design_system_.GetTheme();
        const auto modules = this->ControllerMovableModules();
        auto text = theme.caption_text; text.scale = 0.72f; text.alignment = ui::text_alignment::left;
        if (!focus.moving)
        {
            const controller_movement_layout bounds{this->config_bounds_, viewport, modules.size()};
            this->config_controls_.Panel(bounds.bounds);
            this->config_controls_.Text("MOVEMENT PREVIEW", {bounds.bounds.x + 10, bounds.bounds.y + 5, bounds.bounds.width - 20, 24}, text);
            for (int row = 0; row < bounds.rows; ++row)
            {
                const auto index = static_cast<size_t>(focus.movement_scroll + row);
                if (index >= modules.size()) break;
                const auto* module = modules[index];
                this->config_controls_.NavigationItem(bounds.Row(row), module->DisplayName(), focus.id == module->Id(), false);
            }
            auto caption = text; caption.scale = 0.65f;
            this->config_controls_.Text("Confirm: move selected module", {bounds.bounds.x + 10, bounds.bounds.y + bounds.bounds.height - 30, bounds.bounds.width - 20, 22}, caption);
            return;
        }
        const auto found = std::find_if(modules.begin(), modules.end(), [&](const auto* module) { return focus.moving_id == module->Id(); });
        if (found == modules.end()) { this->FinishControllerMove(false); return; }
        const auto& module = **found;
        const auto visual = this->MovementBounds(module);
        const auto guide = (theme.colors.accent & 0x00FFFFFF) | 0x90000000;
        if (this->layout_snap_.vertical) this->FillRect({*this->layout_snap_.vertical, 0, 1, static_cast<float>(viewport.Height)}, guide);
        if (this->layout_snap_.horizontal) this->FillRect({0, *this->layout_snap_.horizontal, static_cast<float>(viewport.Width), 1}, guide);
        const float width = std::min(640.0f, std::max(1.0f, viewport.Width - 16.0f));
        const rect panel{std::max(8.0f, (viewport.Width - width) * 0.5f), std::max(8.0f, viewport.Height - 96.0f), width, 84};
        this->config_controls_.Panel(panel);
        char heading[180]{};
        std::snprintf(heading, sizeof(heading), "Move %s    X: %.0f  Y: %.0f    Step: %.0f%s", module.DisplayName(), module.State().bounds.x, module.State().bounds.y,
            focus.fine ? 1.0f : std::clamp(std::round(this->general_.Grid().amount), 1.0f, 20.0f), focus.fine ? " (fine)" : "");
        this->config_controls_.Text(heading, {panel.x + 12, panel.y + 7, panel.width - 24, 22}, text);
        auto caption = text; caption.scale = 0.66f;
        this->config_controls_.Text("D-pad: move   Actions: fine step   Next tab: Center X   Previous tab: Center Y", {panel.x + 12, panel.y + 32, panel.width - 24, 20}, caption);
        this->config_controls_.Text("Confirm: save position   Cancel: restore position", {panel.x + 12, panel.y + 55, panel.width - 24, 20}, caption);
        this->config_tooltip_.clear();
        this->design_system_.DrawFocusFrame(visual, this->general_.FocusStyle());
    }

}
