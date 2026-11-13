#include "actionbars_module.hpp"
#include "config/config_controls.hpp"
#include "config/config_layout.hpp"
#include "config/config_window_resize.hpp"

#include <algorithm>
#include <cmath>

namespace xiui
{
    namespace
    {
        constexpr int window_move = 700, window_resize = 701, window_close = 702;
        auto Contains(const ui::rect& bounds, const float x, const float y) -> bool
        { return x >= bounds.x && y >= bounds.y && x < bounds.x + bounds.width && y < bounds.y + bounds.height; }
        auto Viewport(const ui::rect& value) -> ui::rect
        { return value.width > 0 && value.height > 0 ? value : ui::rect{0, 0, 1280, 720}; }
    }

    auto actionbars_module::IsWindowDialog(const dialog value) -> bool
    { return value == dialog::picker || value == dialog::editor || value == dialog::palettes || value == dialog::import || value == dialog::setup || value == dialog::keybinds || value == dialog::icons; }
    auto actionbars_module::WindowDialog(void) const -> dialog
    {
        if (IsWindowDialog(this->dialog_)) return this->dialog_;
        if (this->dialog_ == dialog::name || this->dialog_ == dialog::delete_palette || this->dialog_ == dialog::share_palettes) return dialog::palettes;
        if (this->dialog_ == dialog::delete_macro) return dialog::editor;
        return this->library_popup_ ? dialog::picker : dialog::none;
    }
    auto actionbars_module::WindowIndex(void) const -> size_t
    {
        switch (this->WindowDialog())
        {
        case dialog::palettes: case dialog::import: return 1;
        case dialog::editor: case dialog::icons: return 2;
        case dialog::setup: return 3;
        case dialog::keybinds: return 4;
        default: return 0;
        }
    }
    auto actionbars_module::WindowConfigView(void) const -> size_t
    {
        switch (this->WindowDialog())
        {
        case dialog::picker: case dialog::editor: case dialog::icons: return 11;
        case dialog::palettes: return 12;
        case dialog::import: return 13;
        case dialog::setup: return 10;
        default: return this->view_;
        }
    }
    auto actionbars_module::WindowMinimum(const size_t index) const -> ui::point
    {
        constexpr ui::point minimum[]{{520, 420}, {500, 468}, {480, 548}, {620, 416}, {620, 360}};
        if (index == 0)
        {
            const auto& metrics = this->library_metrics_;
            const float width = std::max({520.0f, metrics.sidebar_width + 2 * metrics.create_width + 44,
                metrics.sidebar_width + 2 * metrics.filter_width + 36 + 16 + 120});
            const float contentWidth = width - metrics.sidebar_width - 36;
            const int columns = std::clamp(static_cast<int>((contentWidth + 8) / (metrics.create_width + 8)), 1, 3);
            const int rows = (3 + columns - 1) / columns;
            const float footerHeight = static_cast<float>(rows) * (std::max(28.0f, metrics.button_height + 10) + 8) - 8;
            return {width, std::max(420.0f, 42 + 9 * std::max(28.0f, metrics.nav_height + 10) + 8 + footerHeight + 12)};
        }
        return minimum[std::min(index, size_t{4})];
    }
    auto actionbars_module::WindowBounds(void) const -> ui::rect
    {
        const auto index = this->WindowIndex(); auto bounds = this->workspace_bounds_[index];
        const auto viewport = Viewport(this->viewport_); const auto minimum = this->WindowMinimum(index);
        const float width = std::max(1.0f, viewport.width - 16), height = std::max(1.0f, viewport.height - 16);
        bounds.width = std::clamp(std::isfinite(bounds.width) ? bounds.width : 640.0f, std::min(minimum.x, width), width);
        bounds.height = std::clamp(std::isfinite(bounds.height) ? bounds.height : 480.0f, std::min(minimum.y, height), height);
        if (!std::isfinite(bounds.x) || bounds.x < 0) bounds.x = viewport.x + 32 + static_cast<float>(index) * 24;
        if (!std::isfinite(bounds.y) || bounds.y < 0) bounds.y = viewport.y + 64 + static_cast<float>(index) * 20;
        bounds.x = std::clamp(bounds.x, viewport.x + 8, std::max(viewport.x + 8, viewport.x + viewport.width - bounds.width - 8));
        bounds.y = std::clamp(bounds.y, viewport.y + 8, std::max(viewport.y + 8, viewport.y + viewport.height - bounds.height - 8));
        return bounds;
    }
    auto actionbars_module::WindowTitleBounds(void) const -> ui::rect
    { const auto bounds = this->WindowBounds(); return {bounds.x + 10, bounds.y + 3, std::max(0.0f, bounds.width - 48), 25}; }
    auto actionbars_module::WindowCloseBounds(void) const -> ui::rect
    { return ui::config_layout{this->WindowBounds()}.CloseButton(); }
    auto actionbars_module::WindowResizeBounds(void) const -> ui::rect
    { return ui::config_layout{this->WindowBounds()}.ResizeGrip(); }
    auto actionbars_module::WindowResizeEdgesAt(const float x, const float y) const -> uint8_t
    {
        if (!IsWindowDialog(this->dialog_)) return 0;
        if (this->WindowResizing() && this->pointer_down_) return this->window_resize_edges_;
        if (this->WindowDragging()) return 0;
        const auto bounds = this->WindowBounds(); constexpr float hit = 6;
        if (x < bounds.x - hit || y < bounds.y - hit || x > bounds.x + bounds.width + hit || y > bounds.y + bounds.height + hit) return 0;
        if (Contains(this->WindowResizeBounds(), x, y)) return ui::resize_right | ui::resize_bottom;
        uint8_t edges{};
        if (std::abs(x - bounds.x) <= hit) edges |= ui::resize_left;
        else if (std::abs(x - bounds.x - bounds.width) <= hit) edges |= ui::resize_right;
        if (std::abs(y - bounds.y) <= hit) edges |= ui::resize_top;
        else if (std::abs(y - bounds.y - bounds.height) <= hit) edges |= ui::resize_bottom;
        return edges;
    }
    auto actionbars_module::SetWindowFocused(const bool focused) -> void
    {
        if (!this->FloatingWindowOpen()) { this->window_focused_ = false; return; }
        if (!focused)
        {
            this->CommitField(); this->binding_capture_ = false;
            this->FinishSetupValue(true); this->CancelSetupControls();
            this->placing_ = this->browsing_ = this->navigation_focus_ = false;
            if (!this->pointer_down_) this->FinishWindowGesture(false);
        }
        this->window_focused_ = focused;
    }
    auto actionbars_module::WindowPointerOwns(const float x, const float y) const -> bool
    {
        return this->FloatingWindowOpen() && (Contains(this->WindowBounds(), x, y) || this->WindowResizeEdgesAt(x, y) != 0);
    }
    auto actionbars_module::BarPointerAvailable(const float x, const float y) const -> bool
    {
        return this->dialog_ == dialog::none || this->dialog_ == dialog::picker && !this->WindowPointerOwns(x, y);
    }
    auto actionbars_module::RememberLibrary(void) -> void
    {
        if (this->dialog_ != dialog::picker || this->library_popup_) return;
        this->CommitField();
        this->library_popup_ = library_context{this->selected_, this->draft_, this->view_, this->selected_bank_, this->focused_control_,
            this->draft_pet_, this->library_draft_, this->library_edit_, this->library_assignment_};
    }
    auto actionbars_module::SwapLibraryContext(library_context& context) -> void
    {
        std::swap(this->selected_, context.selected); std::swap(this->draft_, context.draft); std::swap(this->view_, context.view);
        std::swap(this->selected_bank_, context.bank); std::swap(this->focused_control_, context.control);
        std::swap(this->draft_pet_, context.pet); std::swap(this->library_draft_, context.library_draft); std::swap(this->library_edit_, context.library_edit);
        std::swap(this->library_assignment_, context.assignment);
    }
    auto actionbars_module::PointerCursor(const float x, const float y) const -> cursor_renderer::shape
    {
        using shape = cursor_renderer::shape;
        if (this->WindowDragging()) return shape::move;
        if (this->setup_choice_input_.Opened())
        {
            const auto viewport = this->SetupViewport();
            const auto scrollbar = this->setup_choice_input_.Scrollbar(viewport);
            return this->setup_choice_input_.Scrolling() || this->setup_choice_input_.IndexAt(x, y, viewport) ||
                scrollbar.Visible() && Contains(scrollbar.track, x, y) ? shape::hand : shape::arrow;
        }
        if (this->setup_slider_drag_ != 0) return shape::resize_horizontal;
        if (this->scrolling_) return shape::hand;
        if (this->BarPointerAvailable(x, y) && this->HitTest(x, y)) return shape::hand;
        if (this->IsWindowDialog(this->dialog_))
        {
            const auto edges = this->WindowResizeEdgesAt(x, y);
            const bool horizontal = (edges & (ui::resize_left | ui::resize_right)) != 0;
            const bool vertical = (edges & (ui::resize_top | ui::resize_bottom)) != 0;
            if (horizontal || vertical) return horizontal && vertical ? shape::resize_diagonal : horizontal ? shape::resize_horizontal : shape::resize_vertical;
        }
        if (this->dialog_ == dialog::none || !Contains(this->DialogBounds(), x, y)) return shape::arrow;
        const auto scrollbar = this->Scrollbar();
        if (scrollbar.Visible() && Contains(scrollbar.track, x, y)) return shape::hand;
        for (const auto& control : this->Controls())
        {
            if (!Contains(control.bounds, x, y)) continue;
            if (!control.enabled || control.kind == control_kind::label) return shape::arrow;
            if (control.id == window_move) return shape::move;
            if (control.id == window_resize) return shape::resize_diagonal;
            if (control.kind == control_kind::slider)
            {
                if (Contains(ui::config_layout::SliderValue(control.bounds), x, y)) return shape::hand;
                return x >= ui::config_layout::SliderTrack(control.bounds, control.label_width).x ? shape::resize_horizontal : shape::arrow;
            }
            return shape::hand;
        }
        return shape::arrow;
    }
    auto actionbars_module::BeginWindowGesture(const window_gesture gesture, const uint8_t edges) -> void
    {
        if (!IsWindowDialog(this->dialog_)) return;
        this->CommitField(); this->binding_capture_ = false;
        this->FinishSetupValue(true); this->CancelSetupControls();
        this->FinishWindowGesture(false);
        this->window_workspace_ = this->WindowIndex(); this->window_original_ = this->workspace_bounds_[this->window_workspace_];
        this->window_start_ = this->WindowBounds(); this->workspace_bounds_[this->window_workspace_] = this->window_start_;
        this->window_gesture_ = gesture; this->window_resize_edges_ = edges;
        this->window_focused_ = true; this->library_pressed_.reset(); this->library_drag_.reset(); this->scrolling_ = false;
    }
    auto actionbars_module::AdjustWindow(const float x, const float y) -> void
    {
        if (this->window_gesture_ == window_gesture::none) return;
        auto& bounds = this->workspace_bounds_[this->window_workspace_]; const auto viewport = Viewport(this->viewport_);
        if (this->WindowDragging())
        {
            bounds = this->window_start_;
            bounds.x = std::clamp(bounds.x + x, viewport.x + 8, std::max(viewport.x + 8, viewport.x + viewport.width - bounds.width - 8));
            bounds.y = std::clamp(bounds.y + y, viewport.y + 8, std::max(viewport.y + 8, viewport.y + viewport.height - bounds.height - 8));
        }
        else
        {
            const auto minimum = this->WindowMinimum(this->window_workspace_);
            const D3DVIEWPORT8 area{static_cast<DWORD>(std::max(0.0f, viewport.x + 8)), static_cast<DWORD>(std::max(0.0f, viewport.y + 8)),
                static_cast<DWORD>(std::max(1.0f, viewport.width - 16)), static_cast<DWORD>(std::max(1.0f, viewport.height - 16)), 0, 1};
            bounds = ui::ResizeConfigWindow(this->window_start_, this->window_resize_edges_, x, y, minimum.x, minimum.y, area);
        }
        if (this->dialog_ == dialog::picker) this->SetScroll(static_cast<int>(this->result_scroll_));
        if (this->dialog_ == dialog::icons) this->ScrollIcons(static_cast<int>(this->icon_scroll_));
        if (this->dialog_ == dialog::palettes && this->store_)
        {
            const auto count = this->store_->Palettes(this->selected_.surface, this->selected_.bar).size(), rows = this->PaletteRows();
            this->palette_scroll_ = std::min(this->palette_scroll_, count > rows ? count - rows : 0);
        }
    }
    auto actionbars_module::FinishWindowGesture(const bool accepted) -> void
    {
        if (this->window_gesture_ == window_gesture::none) return;
        auto& bounds = this->workspace_bounds_[this->window_workspace_];
        if (!accepted) bounds = this->window_original_;
        else if (bounds.x != this->window_start_.x || bounds.y != this->window_start_.y || bounds.width != this->window_start_.width || bounds.height != this->window_start_.height)
            this->settings_changed_ = true;
        this->window_gesture_ = window_gesture::none; this->window_resize_edges_ = 0;
    }
    auto actionbars_module::CancelPointerGesture(const bool awaitRelease) -> void
    {
        this->discard_pointer_release_ = awaitRelease && (this->pointer_down_ || this->discard_pointer_release_);
        this->FinishWindowGesture(false);
        this->pointer_down_ = this->bar_pointer_down_ = this->dragging_ = this->shift_drag_ = this->scrolling_ = false;
        this->CancelSetupControls();
        this->library_drag_.reset(); this->library_pressed_.reset(); this->pressed_control_ = 0;
    }
    auto actionbars_module::PaletteRows(void) const -> size_t
    { return static_cast<size_t>(std::clamp(static_cast<int>((this->DialogBounds().height - 112) / 30), 1, 64)); }
    auto actionbars_module::AddWindowControls(std::vector<control>& controls) const -> void
    {
        if (!IsWindowDialog(this->dialog_)) return;
        controls.push_back({window_move, this->WindowTitleBounds(), "Move window", {}, control_kind::button});
        controls.push_back({window_resize, this->WindowResizeBounds(), "Resize window", {}, control_kind::button});
        controls.push_back({window_close, this->WindowCloseBounds(), "Close window", {}, control_kind::button});
    }
    auto actionbars_module::HandleWindowControl(const int id) -> bool
    {
        if (id == window_close) { this->CloseDialog(); return true; }
        if (id == window_move) { this->BeginWindowGesture(window_gesture::move); return true; }
        if (id == window_resize) { this->BeginWindowGesture(window_gesture::resize, ui::resize_right | ui::resize_bottom); return true; }
        return false;
    }
    auto actionbars_module::DrawWindowChrome(const module_render_context& context) const -> void
    {
        const auto bounds = this->WindowBounds(); const auto& design = context.Design(); const auto& theme = design.GetTheme();
        const ui::config_layout layout{bounds};
        std::vector<ui::container_fill> fills{{layout.HeaderBackground(), 0xD0000000 | ((theme.colors.chrome & 0xFEFEFE) >> 1)}};
        if (this->dialog_ == dialog::picker)
        {
            const auto library = this->LibraryLayout();
            fills.push_back({{bounds.x + 5, bounds.y + 34, library.sidebar.width + 14, bounds.height - 39}, (theme.colors.surface_raised & 0xFFFFFF) | 0x60000000});
        }
        context.DrawContainer(bounds, {1, 5, 8, false}, fills);
        ui::config_controls controls{design, nullptr, context.NativeAssets()};
        if (this->config_text_) controls.SetTextSettings(*this->config_text_);
        const auto close = this->WindowCloseBounds();
        controls.IconButton(close, ui::icon_glyph::close, Contains(close, this->hover_x_, this->hover_y_));
        const auto grip = this->WindowResizeBounds();
        const auto color = this->WindowResizing() || Contains(grip, this->hover_x_, this->hover_y_) ? theme.colors.control_selected : theme.colors.muted;
        design.DrawTriangle(grip.x + 5, grip.y + 15, grip.x + 15, grip.y + 5, grip.x + 15, grip.y + 15, color);
    }
}
