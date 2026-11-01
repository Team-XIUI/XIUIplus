#include "chat_module.hpp"
#include "chat_theme.hpp"

#include <algorithm>
#include <cmath>

namespace
{
    auto Contains(const xiui::ui::rect& bounds, const float x, const float y) -> bool
    { return x >= bounds.x && y >= bounds.y && x < bounds.x + bounds.width && y < bounds.y + bounds.height; }
}

namespace xiui
{
    auto chat_module::HistoryLayout(const ui::design_system& design) const -> history_layout
    {
        history_layout result{};
        const auto& viewport = this->history_view_.viewport;
        const float margin = std::min(16.0f, viewport.width * 0.02f);
        const float left = viewport.x + margin, right = viewport.x + viewport.width - margin;
        const float height = std::max(24.0f, design.TextHeight(ChatThemeStyle(design, this->chrome_.style)) + 8);
        float x = left, y = viewport.y + margin;
        for (size_t index = 0; index < result.tabs.size(); ++index)
        {
            if (!this->HistoryTabAvailable(index)) continue;
            const float width = std::min(right - left, std::max(56.0f, design.MeasureText(this->HistoryTabName(index), ChatThemeStyle(design, this->chrome_.style, true)) + 24));
            if (x > left && x + width > right) { x = left; y += height + 2; }
            result.tabs[index] = {x, y, width, height};
            x += width + 2;
        }
        result.frame = {left, y + height + 2, right - left, std::max(1.0f, viewport.y + viewport.height - margin - y - height - 2)};
        const auto content = ui::container_renderer::ContentBounds(result.frame, this->container_);
        const float footerHeight = design.TextHeight(ChatThemeStyle(design, this->chrome_.style)) + 8;
        result.footer = {content.x + 2, content.y + content.height - footerHeight, content.width - 4, footerHeight};
        result.log = {content.x + 2, content.y, std::max(1.0f, content.width - 14), std::max(1.0f, result.footer.y - content.y - 4)};
        result.scrollbar = {result.log.x + result.log.width + 4, result.log.y, 6, result.log.height};
        return result;
    }

    auto chat_module::HistoryTabAvailable(const size_t index) const -> bool
    {
        if (index >= std::size(chat_history_tabs)) return false;
        if (index != static_cast<size_t>(chat_history_tab::window2)) return true;
        const auto* second = this->secondary_ ? this : this->second_window_;
        return second != nullptr && second->WindowVisible() &&
            (this->native_filters_ == nullptr || this->native_filters_->WindowHasRouting(true) != false);
    }

    auto chat_module::SelectHistoryTab(const size_t index) -> void
    {
        this->history_view_.tab = index % std::size(chat_history_tabs);
        if (!this->HistoryTabAvailable(this->history_view_.tab)) this->history_view_.tab = static_cast<size_t>(chat_history_tab::window1);
        this->cache_revision_ = UINT64_MAX;
    }

    auto chat_module::ScrollMessage(const int direction) -> void
    {
        auto& scroll = this->CurrentScroll();
        const size_t end = this->rows_.size() - std::min(scroll, this->rows_.size());
        size_t next = end;
        if (direction > 0 && end > 0)
        {
            const auto sequence = this->rows_[end - 1].sequence;
            do { --next; } while (next > 0 && this->rows_[next - 1].sequence == sequence);
        }
        else if (direction < 0 && end < this->rows_.size())
        {
            const auto sequence = this->rows_[end].sequence;
            do { ++next; } while (next < this->rows_.size() && this->rows_[next].sequence == sequence);
        }
        this->Scroll(static_cast<int>(end) - static_cast<int>(next));
        const size_t maximum = this->rows_.size() > this->visible_rows_ ? this->rows_.size() - this->visible_rows_ : 0;
        scroll = std::min(scroll, maximum);
    }

    auto chat_module::BeginHistory(const bool controller, const bool playSound) -> bool
    {
        if (!this->WindowVisible() || this->resizing_) return false;
        this->SelectHistoryTab(this->history_view_.tab);
        if (this->parent_ != nullptr) this->parent_->Focus(false);
        if (this->second_window_ != nullptr) this->second_window_->Focus(false);
        this->focused_ = false;
        this->controller_composition_ = this->controller_channel_adjusting_ = false;
        this->editor_.ResetCompletion();
        this->history_view_.focused = true;
        this->controller_history_ = controller;
        if (playSound && this->sounds_ != nullptr) this->sounds_->Select(::GetTickCount64());
        return true;
    }

    auto chat_module::BeginControllerCompose(void) -> bool
    {
        auto& editor = this->parent_ != nullptr ? *this->parent_ : *this;
        if (!editor.WindowVisible()) return false;
        this->Focus(false);
        editor.error_.clear();
        editor.Focus(true);
        editor.controller_composition_ = true;
        editor.controller_compose_action_ = 0;
        editor.controller_return_history_ = this;
        return true;
    }

    auto chat_module::SetControllerHistoryPrompts(std::string compact, std::string expanded) -> void
    {
        this->controller_compact_prompt_ = std::move(compact);
        this->controller_expanded_prompt_ = std::move(expanded);
    }

    auto chat_module::HandleControllerAction(const controller_action action, const bool repeated, const ui::design_system& design, IChatManager* chat) -> bool
    {
        if (this->Focused() && !this->controller_composition_ && action != controller_action::none)
        {
            this->controller_composition_ = true;
            this->controller_compose_action_ = 0;
            this->controller_return_history_ = this;
        }
        if (this->ControllerComposing()) return this->HandleControllerCompose(action, repeated, chat);
        if (this->parent_ != nullptr && this->parent_->ControllerComposing())
            return this->parent_->HandleControllerCompose(action, repeated, chat);
        if (!this->HistoryFocused()) return false;
        this->controller_history_ = true;
        if (action == controller_action::actions) return repeated || this->BeginControllerCompose();
        return this->NavigateHistory(action, repeated, design, true);
    }

    auto chat_module::NavigateHistory(const controller_action action, const bool repeated, const ui::design_system& design, const bool layered) -> bool
    {
        if (!this->HistoryTabAvailable(this->history_view_.tab)) this->SelectHistoryTab(static_cast<size_t>(chat_history_tab::window1));
        if (action == controller_action::cancel)
        {
            if (!repeated)
            {
                if (layered && this->HistoryExpanded()) this->history_view_.expanded = false;
                else this->Focus(false);
                this->cache_revision_ = UINT64_MAX;
            }
            return true;
        }
        if (action == controller_action::confirm)
        {
            if (!repeated)
            {
                if (this->HistoryExpanded())
                {
                    if (layered) this->history_view_.expanded = false;
                    else this->Focus(false);
                    this->cache_revision_ = UINT64_MAX;
                }
                else
                {
                    this->history_view_.scroll = {};
                    this->history_view_.expanded = true;
                    this->SelectHistoryTab(static_cast<size_t>(chat_history_tab::window1));
                    if (this->sounds_ != nullptr) this->sounds_->MenuOpened(::GetTickCount64());
                }
            }
            return true;
        }
        if (action == controller_action::next_tab || action == controller_action::previous_tab || action == controller_action::cycle_window)
        {
            if (!repeated && this->HistoryExpanded())
            {
                auto next = this->history_view_.tab;
                do { next = (next + (action == controller_action::previous_tab ? std::size(chat_history_tabs) - 1 : 1)) % std::size(chat_history_tabs); }
                while (!this->HistoryTabAvailable(next));
                this->SelectHistoryTab(next);
                if (this->sounds_ != nullptr) this->sounds_->Select(::GetTickCount64());
            }
            return true;
        }
        const auto log = this->HistoryExpanded() ? this->HistoryLayout(design).log : this->Layout(design).log;
        this->RebuildRows(design, log.width);
        this->visible_rows_ = std::max<size_t>(1, static_cast<size_t>(log.height / (design.TextHeight(ChatThemeStyle(design, this->text_.style)) + this->line_spacing_)));
        switch (action)
        {
        case controller_action::left: this->Scroll(static_cast<int>(std::max<size_t>(1, this->visible_rows_ - 1))); break;
        case controller_action::right: this->Scroll(-static_cast<int>(std::max<size_t>(1, this->visible_rows_ - 1))); break;
        case controller_action::up: this->ScrollMessage(1); break;
        case controller_action::down: this->ScrollMessage(-1); break;
        default: return false;
        }
        const size_t maximum = this->rows_.size() > this->visible_rows_ ? this->rows_.size() - this->visible_rows_ : 0;
        this->CurrentScroll() = std::min(this->CurrentScroll(), maximum);
        return true;
    }

    auto chat_module::HandleHistoryKey(const WPARAM key, const LPARAM flags, const bool blocked, const bool, const ui::design_system& design) -> bool
    {
        if (blocked || !this->WindowVisible() || this->Focused() || this->resizing_) return false;
        const bool released = (static_cast<uint32_t>(flags) & 0x80000000u) != 0;
        if (released) return false;
        const bool repeated = (static_cast<uint32_t>(flags) & 0x40000000u) != 0;
        const bool control = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool alt = (::GetKeyState(VK_MENU) & 0x8000) != 0 || (static_cast<uint32_t>(flags) & 0x20000000u) != 0;
        const bool shift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (alt && key == VK_TAB) return false;
        if (!this->HistoryFocused())
            return false;
        if (((key == VK_OEM_2 || key == VK_DIVIDE) && !shift && !control && !alt) || (key == 'R' && alt))
        { this->Focus(false); return false; }
        if (key != VK_ESCAPE && key != VK_RETURN && key != 'F' && key != VK_LEFT && key != VK_PRIOR &&
            key != VK_RIGHT && key != VK_NEXT && key != VK_UP && key != VK_DOWN && key != VK_HOME && key != VK_END) return false;
        this->controller_history_ = false;
        switch (key)
        {
        case VK_ESCAPE: return this->NavigateHistory(controller_action::cancel, repeated, design, true);
        case VK_RETURN: return this->NavigateHistory(controller_action::confirm, repeated, design, false);
        case 'F': return this->NavigateHistory(shift ? controller_action::previous_tab : controller_action::next_tab, repeated, design, false);
        case VK_LEFT: case VK_PRIOR: return this->NavigateHistory(controller_action::left, repeated, design, false);
        case VK_RIGHT: case VK_NEXT: return this->NavigateHistory(controller_action::right, repeated, design, false);
        case VK_UP: return this->NavigateHistory(controller_action::up, repeated, design, false);
        case VK_DOWN: return this->NavigateHistory(controller_action::down, repeated, design, false);
        default: break;
        }
        const auto log = this->HistoryExpanded() ? this->HistoryLayout(design).log : this->Layout(design).log;
        this->RebuildRows(design, log.width);
        this->visible_rows_ = std::max<size_t>(1, static_cast<size_t>(log.height / (design.TextHeight(ChatThemeStyle(design, this->text_.style)) + this->line_spacing_)));
        switch (key)
        {
        case VK_HOME: this->CurrentScroll() = this->rows_.size(); break;
        case VK_END: this->CurrentScroll() = 0; break;
        default: break;
        }
        const size_t maximum = this->rows_.size() > this->visible_rows_ ? this->rows_.size() - this->visible_rows_ : 0;
        this->CurrentScroll() = std::min(this->CurrentScroll(), maximum);
        return true;
    }

    auto chat_module::HistorySoundTarget(const ui::design_system& design, const float x, const float y) const -> uint32_t
    {
        const auto layout = this->HistoryLayout(design);
        for (size_t index = 0; index < layout.tabs.size(); ++index)
            if (Contains(layout.tabs[index], x, y)) return static_cast<uint32_t>(200 + index);
        return 0;
    }

    auto chat_module::HandleHistoryMouse(const uint32_t message, const WPARAM wParam, const float x, const float y, const ui::design_system& design) -> bool
    {
        if (!Contains(this->history_view_.viewport, x, y) && !this->pointer_down_) return false;
        this->history_view_.pointer = {x, y};
        const auto layout = this->HistoryLayout(design);
        if (message == WM_MOUSEWHEEL)
        {
            this->RebuildRows(design, layout.log.width);
            this->Scroll(static_cast<short>(HIWORD(wParam)) / WHEEL_DELTA * 3);
        }
        if (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK)
        {
            if (!this->pointer_down_)
                for (size_t index = 0; index < layout.tabs.size(); ++index)
                    if (Contains(layout.tabs[index], x, y))
                    {
                        this->SelectHistoryTab(index);
                        if (this->sounds_ != nullptr) this->sounds_->Select(::GetTickCount64());
                        break;
                    }
            this->pointer_down_ = true;
        }
        if (message == WM_LBUTTONUP) this->pointer_down_ = false;
        return true;
    }

    auto chat_module::RenderHistory(const hud_snapshot& snapshot, const module_render_context& context) -> void
    {
        const auto transform = context.Design().Transform(this->state_.transform);
        if (!this->HistoryExpanded()) return;
        if (!this->HistoryTabAvailable(this->history_view_.tab)) this->SelectHistoryTab(static_cast<size_t>(chat_history_tab::window1));
        this->SetHistoryViewport(context.Viewport());
        const auto layout = this->HistoryLayout(context.Design());
        this->DrawWindowContainer(context, layout.frame);
        auto label = this->chrome_; label.style.alignment = ui::text_alignment::center;
        for (size_t index = 0; index < layout.tabs.size(); ++index)
            if (layout.tabs[index].height > 0)
                DrawChatTab(context, layout.tabs[index], this->HistoryTabName(index), index == this->history_view_.tab ||
                    Contains(layout.tabs[index], this->history_view_.pointer.x, this->history_view_.pointer.y), label);
        this->DrawLog(snapshot, context, layout.log, layout.scrollbar);
        auto help = ChatThemeProfile(context.Design(), this->chrome_); help.style.scale *= 0.85f; help.style.alignment = ui::text_alignment::left;
        context.DrawText(this->controller_history_ ? this->controller_expanded_prompt_.c_str() :
            "F: next tab   Shift+F: previous   Left/Right: page   Up/Down: message   Enter: close   Esc: back", layout.footer, layout.footer, help);
    }
}
