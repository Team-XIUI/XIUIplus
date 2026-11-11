#include "hud_runtime.hpp"

namespace xiui
{
    auto hud_runtime::NativeWindowIndex(const hud_module& module) const -> size_t
    {
        for (size_t index = 0; index < this->native_windows_.size(); ++index)
            if (&this->native_windows_[index] == &module) return index;
        return native_window_count;
    }

    auto hud_runtime::MenuLayout(void) const -> native_menu_layout
    {
        native_menu_layout layout{};
        layout.viewport_width = static_cast<float>(this->hud_viewport_.Width);
        layout.viewport_height = static_cast<float>(this->hud_viewport_.Height);
        if (!this->visible_) return layout;
        for (size_t index = 0; index < this->native_windows_.size(); ++index)
        {
            layout.windows[index] = this->native_windows_[index].Placement();
            layout.windows[index].hidden = index >= native_hud_window_count && this->NativeWindowHidden(index);
            if (this->interaction_module_ == &this->native_windows_[index] ||
                (this->config_.focus.moving && this->config_.focus.moving_id == this->native_windows_[index].Id()))
                layout.windows[index].enabled = true;
        }
        return layout;
    }

    auto hud_runtime::NativeWindowAt(const float x, const float y) -> native_window_module*
    {
        native_window_module* selected{};
        for (auto& window : this->native_windows_)
        {
            const auto& bounds = window.State().bounds;
            if (this->ModuleDisplayed(window) && window.CanMove() && Contains(bounds, x, y) && (selected == nullptr ||
                bounds.width * bounds.height < selected->State().bounds.width * selected->State().bounds.height)) selected = &window;
        }
        return selected;
    }

    auto hud_runtime::SetNativeEditMode(const bool native) -> void
    {
        if (!this->editing_) return;
        if (this->config_.edit_mode && this->config_.native_edit == native) return;
        this->FinishScale(false); this->FinishControllerMove(false); this->FinishInteraction();
        this->FinishConfigInput(true, false); this->CloseChoiceInput(); this->CloseProfileControls();
        this->config_.active_color = nullptr; this->config_.header_menu_open = false;
        const auto now = ::GetTickCount64();
        auto& slide = this->config_.native_edit_slide;
        slide = {slide.Value(now), native ? 1.0f : 0.0f, now};
        this->config_.edit_mode = true; this->config_.native_edit = native; this->config_.preview_selected = false;
        if (native)
        {
            this->config_.native_edit_hidden = {};
            this->game_hud_.TakeVisibilityEdits();
            for (auto& window : this->native_windows_) window.TakeVisibilityEdit();
        }
        if (native) this->SetBasicSettings(false, false);
        if (native && this->config_.module != &this->commands_ && this->config_.module != &this->game_hud_)
            this->SelectConfigModule(&this->game_hud_);
        this->UpdateNativeWindows(this->hud_viewport_);
        this->RefreshControllerConfigFocus();
    }

    auto hud_runtime::UpdateNativeWindows(const D3DVIEWPORT8& viewport) -> void
    {
        const float sx = this->native_ui_.reference_width > 0 ? viewport.Width / this->native_ui_.reference_width : 1;
        const float sy = this->native_ui_.reference_height > 0 ? viewport.Height / this->native_ui_.reference_height : 1;
        const auto hudEdits = this->game_hud_.TakeVisibilityEdits();
        const auto hudHidden = this->game_hud_.HiddenMask();
        for (size_t index = 0; index < this->native_windows_.size(); ++index)
        {
            auto& window = this->native_windows_[index];
            const bool edited = window.TakeVisibilityEdit() || (index < native_hud_window_count && (hudEdits & (uint32_t{1} << index)) != 0);
            if (this->NativeEditMode() && edited) this->config_.native_edit_hidden[index] = index < native_hud_window_count ?
                (hudHidden & (uint32_t{1} << index)) != 0 : window.Hidden();
            window.SetEditing(this->NativeEditMode());
            const bool moving = this->interaction_module_ == &window || (this->config_.focus.moving && this->config_.focus.moving_id == window.Id());
            auto state = this->native_ui_.windows[index]; state.movable = state.movable && this->native_ui_.menu_available;
            window.Update(state, sx, sy, static_cast<float>(viewport.Width), static_cast<float>(viewport.Height), moving);
        }
    }

    auto hud_runtime::EditModeSwitchBounds(const D3DVIEWPORT8& viewport) const -> rect
    {
        const float width = std::min(28.0f, static_cast<float>(viewport.Width));
        const float height = std::min(60.0f, static_cast<float>(viewport.Height));
        return {viewport.X + viewport.Width - width, viewport.Y + (viewport.Height - height) * .5f, width, height};
    }

    auto hud_runtime::EditModeSwitchItemBounds(const D3DVIEWPORT8& viewport, const bool native) const -> rect
    {
        auto bounds = this->EditModeSwitchBounds(viewport);
        bounds.height *= .5f;
        if (native) bounds.y += bounds.height;
        return bounds;
    }

    auto hud_runtime::EditModeSwitchHelp(const bool native) const -> const char*
    { return native ? "Native UI Edit Mode" : "XIUI Edit Mode"; }

    auto hud_runtime::NativeVisibilityButton(const native_window_module& module) const -> rect
    {
        const auto& bounds = module.State().bounds;
        return {bounds.x + module.HeaderWidth() - 48, bounds.y >= 22 ? bounds.y - 22 : bounds.y + bounds.height, 48, 22};
    }

    auto hud_runtime::NativeEditTarget(const D3DVIEWPORT8& viewport) const -> uint64_t
    {
        if (!this->editing_ || !this->config_.edit_mode || this->ProfileSurface(viewport) || this->config_.choice_input.Opened() ||
            this->config_.active_color != nullptr || this->config_.header_menu_open || this->config_.search_open) return 0;
        const float x = static_cast<float>(this->mouse_x_), y = static_cast<float>(this->mouse_y_);
        if (Contains(this->EditModeSwitchBounds(viewport), x, y))
            return Contains(this->EditModeSwitchItemBounds(viewport, false), x, y) ? 0xE100 : 0xE101;
        if (!this->NativeEditMode() || Contains(this->ConfigVisibleBounds(), x, y)) return 0;
        for (size_t index = this->native_windows_.size(); index-- > 0;)
            if (this->ModuleDisplayed(this->native_windows_[index]) && Contains(this->NativeVisibilityButton(this->native_windows_[index]), x, y)) return 0xE200 + index;
        return 0;
    }

    auto hud_runtime::ProcessNativeEditClick(const D3DVIEWPORT8& viewport) -> bool
    {
        const auto target = this->NativeEditTarget(viewport);
        if (target == 0) return false;
        if (target < 0xE200) this->SetNativeEditMode(target == 0xE101);
        else
        {
            const size_t index = static_cast<size_t>(target - 0xE200);
            this->ToggleNativeWindowVisibility(index);
        }
        return true;
    }

    auto hud_runtime::ToggleNativeWindowVisibility(const size_t index) -> void
    {
        if (index >= native_window_count) return;
        const bool hidden = !this->NativeWindowHidden(index);
        if (this->NativeEditMode())
        {
            this->config_.native_edit_hidden[index] = hidden;
        }
        if (index < native_hud_window_count) this->game_hud_.SetWindowShown(index, !hidden);
        else this->native_windows_[index].SetHidden(hidden);
        this->SaveSettings();
    }

    auto hud_runtime::DrawEditModeSwitcher(const D3DVIEWPORT8& viewport) -> void
    {
        if (!this->config_.edit_mode) return;
        const auto bounds = this->EditModeSwitchBounds(viewport);
        const auto& colors = this->design_system_.GetTheme().colors;
        const auto hover = this->NativeEditTarget(viewport);
        const float half = bounds.height * .5f;
        this->design_system_.FillRoundedRect(bounds, colors.control, 6);
        this->design_system_.FillRoundedRect({bounds.x + 1, bounds.y + 1, std::max(0.0f, bounds.width - 2), std::max(0.0f, bounds.height - 2)}, colors.chrome, 5);
        const float position = this->config_.native_edit_slide.Value(::GetTickCount64());
        this->design_system_.FillRoundedRect({bounds.x + 2, bounds.y + 2 + position * half, std::max(0.0f, bounds.width - 4), std::max(0.0f, half - 4)},
            (colors.control_selected & 0x00FFFFFF) | 0x66000000, 4);
        for (size_t index = 0; index < 2; ++index)
        {
            const bool native = index == 1;
            const auto item = this->EditModeSwitchItemBounds(viewport, native);
            const bool hovered = hover == 0xE100 + index;
            const float size = std::max(0.0f, std::min(item.width - 8, item.height - 8));
            this->design_system_.DrawIcon({item.x + (item.width - size) * .5f, item.y + (item.height - size) * .5f, size, size},
                native ? ui::icon_glyph::gamehud : ui::icon_glyph::container, this->config_.native_edit == native || hovered ? colors.control_selected : colors.muted);
            if (hovered)
            {
                this->config_.tooltip = this->EditModeSwitchHelp(native);
                this->config_.tooltip_anchor = this->ConfigTransform().Inverse(item);
            }
        }
    }

    auto hud_runtime::NativeWindowHidden(const size_t index) const -> bool
    {
        if (this->NativeEditMode()) return this->config_.native_edit_hidden[index];
        return index < native_hud_window_count ? (this->game_hud_.HiddenMask() & (uint32_t{1} << index)) != 0 : this->native_windows_[index].Hidden();
    }

    auto hud_runtime::DrawNativeWindowButtons(void) -> void
    {
        const auto& theme = this->design_system_.GetTheme();
        if (this->NativeEditMode())
            for (size_t index = 0; index < this->native_windows_.size(); ++index)
            {
                if (!this->ModuleDisplayed(this->native_windows_[index])) continue;
                const bool hidden = this->NativeWindowHidden(index);
                this->config_controls_.Button(this->NativeVisibilityButton(this->native_windows_[index]), hidden ? "Show" : "Hide", theme.caption_text, false);
            }
    }

}
