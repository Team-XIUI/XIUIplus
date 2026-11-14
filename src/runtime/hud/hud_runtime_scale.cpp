#include "hud/hud_runtime.hpp"

#include <cmath>
#include <cstdio>

namespace xiui
{
    auto hud_runtime::ResolveHudTransforms(const D3DVIEWPORT8& viewport) -> void
    {
        this->hud_viewport_ = viewport;
        const float global = this->general_.HudScale();
        for (auto* module : this->modules_.All())
        {
            auto& state = module->State();
            state.scale.Sanitize();
            auto transform = module->SupportsScale() ? ui::hud_transform::At(state.bounds.x, state.bounds.y, global * state.scale.x, global * state.scale.y) : ui::hud_transform{};
            const bool scaling = this->scale_gesture_.module == module && !this->scale_gesture_.resizing_bounds;
            if (module->HasEditableBounds() && module->PositionBoundsReady())
            {
                const auto& gesture = this->scale_gesture_;
                const auto layout = scaling ? rect{state.bounds.x + gesture.layout.x - gesture.original_bounds.x,
                    state.bounds.y + gesture.layout.y - gesture.original_bounds.y, gesture.layout.width, gesture.layout.height} : this->LayoutBounds(*module);
                const auto bounds = transform.Map(layout);
                transform.offset_x += std::clamp(bounds.x, 0.0f, std::max(0.0f, viewport.Width - bounds.width)) - bounds.x;
                transform.offset_y += std::clamp(bounds.y, 0.0f, std::max(0.0f, viewport.Height - bounds.height)) - bounds.y;
            }
            state.transform = transform;
        }
    }

    auto hud_runtime::MovementBounds(const hud_module& module) const -> rect
    { return module.State().transform.Map(this->LayoutBounds(module)); }

    auto hud_runtime::ScaleGrip(const hud_module& module) const -> rect
    {
        if (const auto grip = module.EditGripBounds(this->design_system_)) return *grip;
        auto bounds = this->MovementBounds(module);
        if (this->scale_gesture_.module == &module)
        {
            bounds.x = this->scale_gesture_.visual.x; bounds.y = this->scale_gesture_.visual.y;
            if (this->scale_gesture_.resizing_bounds)
            {
                bounds.width = this->scale_gesture_.visual.width + (module.State().bounds.width - this->scale_gesture_.original_bounds.width) * module.State().transform.x;
                bounds.height = this->scale_gesture_.visual.height + (module.State().bounds.height - this->scale_gesture_.original_bounds.height) * module.State().transform.y;
            }
            else
            {
                bounds.width = this->scale_gesture_.visual.width * module.State().scale.x / this->scale_gesture_.original.x;
                bounds.height = this->scale_gesture_.visual.height * module.State().scale.y / this->scale_gesture_.original.y;
                bounds.y += this->scale_gesture_.visual.height - bounds.height;
            }
        }
        return {std::clamp(bounds.x + bounds.width - 12, 0.0f, std::max(0.0f, this->hud_viewport_.Width - 12.0f)),
            std::clamp(bounds.y, 0.0f, std::max(0.0f, this->hud_viewport_.Height - 12.0f)), 12, 12};
    }

    auto hud_runtime::EditGripAt(const float x, const float y) const -> hud_module*
    {
        if (!this->editing_) return nullptr;
        const auto& modules = this->modules_.All();
        for (auto at = modules.rbegin(); at != modules.rend(); ++at)
            if ((*at)->SupportsScale() && (*at)->EditGrip() != edit_grip_kind::none && (*at)->HasEditableBounds() &&
                this->ModuleDisplayed(**at) && Contains(this->ScaleGrip(**at), x, y)) return *at;
        return nullptr;
    }

    auto hud_runtime::BeginScale(hud_module& module, const bool keyboard) -> void
    {
        if (!module.SupportsScale() || !module.HasEditableBounds() || !this->ModuleDisplayed(module) || module.EditGrip() == edit_grip_kind::none) return;
        this->FinishScale(false);
        this->FinishConfigInput(true, false);
        if (this->general_.FollowMovedModuleSettings()) this->FocusModuleConfig(module);
        this->ResolveHudTransforms(this->hud_viewport_);
        this->scale_gesture_ = {&module, module.State().scale, module.State().bounds, this->MovementBounds(module), this->LayoutBounds(module),
            {static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_)}, keyboard, false, module.EditGrip() == edit_grip_kind::resize};
        if (keyboard) { this->config_.focus.active = true; this->controller_surface_ = controller_surface::config; }
        else { this->interaction_module_ = &module; this->interaction_ = interaction::scale_module; }
    }

    auto hud_runtime::UpdateScale(const float x, const float y) -> void
    {
        const auto& gesture = this->scale_gesture_;
        if (gesture.module == nullptr) return;
        if (gesture.resizing_bounds)
        {
            auto& state = gesture.module->State();
            const auto& original = gesture.original_bounds;
            const float maxWidth = std::min(state.maximum_width, original.width + std::max(0.0f, this->hud_viewport_.Width - gesture.visual.x - gesture.visual.width) / state.transform.x);
            const float maxHeight = original.height + std::max(0.0f, gesture.visual.y) / state.transform.y;
            state.bounds.width = std::clamp(original.width + (x - gesture.pointer.x) / state.transform.x, state.minimum_width, std::max(state.minimum_width, maxWidth));
            state.bounds.height = std::clamp(original.height - (y - gesture.pointer.y) / state.transform.y, state.minimum_height, std::max(state.minimum_height, maxHeight));
            state.bounds.y = original.y + original.height - state.bounds.height;
            state.content_bounds = {};
            return;
        }
        const float width = std::max(1.0f, gesture.visual.width), height = std::max(1.0f, gesture.visual.height);
        const float dx = x - gesture.pointer.x, dy = gesture.pointer.y - y;
        float factorX = 1 + dx / width, factorY = 1 + dy / height;
        if (gesture.original.locked)
        {
            // Project onto the corner diagonal without switching the controlling axis.
            factorX = factorY = 1 + (dx * width + dy * height) / (width * width + height * height);
        }
        this->ApplyScaleFactors(factorX, factorY);
    }

    auto hud_runtime::ApplyScaleFactors(float x, float y) -> void
    {
        const auto& gesture = this->scale_gesture_;
        if (gesture.module == nullptr || gesture.resizing_bounds) return;
        auto& state = gesture.module->State();
        const float bottom = gesture.visual.y + gesture.visual.height;
        const float minX = .25f / gesture.original.x, minY = .25f / gesture.original.y;
        const float maxX = std::min(3 / gesture.original.x,
            std::max(1.0f, (this->hud_viewport_.Width - gesture.visual.x) / std::max(1.0f, gesture.visual.width)));
        const float maxY = std::min(3 / gesture.original.y, std::max(1.0f, bottom / std::max(1.0f, gesture.visual.height)));
        if (gesture.original.locked) x = y = std::clamp(x, std::max(minX, minY), std::min(maxX, maxY));
        else { x = std::clamp(x, minX, maxX); y = std::clamp(y, minY, maxY); }
        state.scale.x = gesture.original.x * x; state.scale.y = gesture.original.y * y;
        const float global = this->general_.HudScale();
        state.bounds.x = gesture.visual.x - (gesture.layout.x - gesture.original_bounds.x) * global * state.scale.x;
        state.bounds.y = bottom - (gesture.layout.y + gesture.layout.height - gesture.original_bounds.y) * global * state.scale.y;
        if (x == 1 && y == 1) { state.bounds.x = gesture.original_bounds.x; state.bounds.y = gesture.original_bounds.y; }
        this->ResolveHudTransforms(this->hud_viewport_);
    }

    auto hud_runtime::FinishScale(const bool apply) -> void
    {
        const auto gesture = this->scale_gesture_;
        if (gesture.module == nullptr) return;
        this->scale_gesture_ = {};
        if (!apply) { gesture.module->State().scale = gesture.original; gesture.module->State().bounds = gesture.original_bounds; }
        if (gesture.resizing_bounds)
        {
            gesture.module->State().content_bounds = {};
            if (apply) gesture.module->OnBoundsResized(gesture.original_bounds);
        }
        if (apply && (gesture.module->State().bounds.x != gesture.original_bounds.x || gesture.module->State().bounds.y != gesture.original_bounds.y))
            this->CommitModulePosition(*gesture.module);
        if (this->interaction_ == interaction::scale_module) { this->interaction_ = interaction::none; this->interaction_module_ = nullptr; }
        gesture.module->State().scale_checkpoint = gesture.module->State().scale;
        this->ResolveHudTransforms(this->hud_viewport_);
        if (apply) this->SaveSettings();
    }

    auto hud_runtime::HandleScaleAction(const controller_action value, const bool repeated) -> bool
    {
        auto& gesture = this->scale_gesture_;
        if (gesture.module == nullptr) return false;
        if (value == controller_action::cancel || value == controller_action::confirm)
        { if (!repeated) this->FinishScale(value == controller_action::confirm); return true; }
        if (value == controller_action::actions) { if (!repeated) gesture.fine = !gesture.fine; return true; }
        if (gesture.resizing_bounds)
        {
            const float step = gesture.fine ? 1.0f : 8.0f;
            const auto& state = gesture.module->State();
            this->UpdateScale(gesture.pointer.x + (state.bounds.width - gesture.original_bounds.width) * state.transform.x +
                (value == controller_action::left ? -step : value == controller_action::right ? step : 0),
                gesture.pointer.y - (state.bounds.height - gesture.original_bounds.height) * state.transform.y +
                (value == controller_action::up ? -step : value == controller_action::down ? step : 0));
            return true;
        }
        const float step = gesture.fine ? .01f : .05f;
        auto& scale = gesture.module->State().scale;
        const float dx = value == controller_action::left ? -step : value == controller_action::right ? step : 0;
        const float dy = value == controller_action::up ? step : value == controller_action::down ? -step : 0;
        if (scale.locked && (dx != 0 || dy != 0))
        {
            const float factor = 1 + (dx + dy) / (dx != 0 ? scale.x : scale.y);
            const float safe = std::clamp(factor, std::max(.25f / scale.x, .25f / scale.y), std::min(3 / scale.x, 3 / scale.y));
            scale.x *= safe; scale.y *= safe;
        }
        else { scale.x += dx; scale.y += dy; }
        scale.Sanitize();
        this->ApplyScaleFactors(scale.x / gesture.original.x, scale.y / gesture.original.y);
        return true;
    }

    auto hud_runtime::DrawScaleGrips(void) -> void
    {
        for (const auto* module : this->modules_.All())
            if (module->SupportsScale() && module->EditGrip() == edit_grip_kind::scale && module->HasEditableBounds() && this->ModuleEditControlsVisible(*module))
            {
                const auto grip = this->ScaleGrip(*module);
                const auto color = this->design_system_.GetTheme().colors.accent;
                for (int line = 0; line < 3; ++line)
                    this->design_system_.FillRect({grip.x + 3 + line * 3, grip.y + 2, 2, static_cast<float>(2 + line * 3)}, color);
            }
        if (this->scale_gesture_.module != nullptr)
        {
            const auto& scale = this->scale_gesture_.module->State().scale;
            char text[256]{};
            if (this->scale_gesture_.resizing_bounds)
                std::snprintf(text, sizeof(text), "%s  Size %.0f x %.0f  |  Enter/Confirm: save  Escape/Cancel: restore", this->scale_gesture_.module->DisplayName(), this->scale_gesture_.module->State().bounds.width, this->scale_gesture_.module->State().bounds.height);
            else if (scale.locked && std::abs(scale.x - scale.y) < .0001f)
                std::snprintf(text, sizeof(text), "%s  Scale %.0f%%  |  Enter/Confirm: save  Escape/Cancel: restore", this->scale_gesture_.module->DisplayName(), scale.x * 100);
            else
                std::snprintf(text, sizeof(text), "%s  Scale X %.0f%%  Y %.0f%%  |  Enter/Confirm: save  Escape/Cancel: restore", this->scale_gesture_.module->DisplayName(), scale.x * 100, scale.y * 100);
            auto style = this->design_system_.GetTheme().caption_text; style.scale = .75f;
            const auto& at = this->scale_gesture_.visual;
            this->config_controls_.Text(text, {std::max(8.0f, at.x), std::max(8.0f, at.y - 26), 700, 24}, style);
        }
    }
}
