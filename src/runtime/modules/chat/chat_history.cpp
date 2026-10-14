#include "chat_module.hpp"

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
        const float height = std::max(24.0f, design.TextHeight(this->chrome_.style) + 8);
        float x = left, y = viewport.y + margin;
        for (size_t index = 0; index < result.tabs.size(); ++index)
        {
            const float width = std::min(right - left, std::max(56.0f, design.MeasureText(chat_history_tabs[index], this->chrome_.style) + 24));
            if (x > left && x + width > right) { x = left; y += height + 2; }
            result.tabs[index] = {x, y, width, height};
            x += width + 2;
        }
        result.frame = {left, y + height + 2, right - left, std::max(1.0f, viewport.y + viewport.height - margin - y - height - 2)};
        const auto content = ui::container_renderer::ContentBounds(result.frame, this->container_);
        const float footerHeight = design.TextHeight(this->chrome_.style) + 8;
        result.footer = {content.x + 2, content.y + content.height - footerHeight, content.width - 4, footerHeight};
        result.log = {content.x + 2, content.y, std::max(1.0f, content.width - 14), std::max(1.0f, result.footer.y - content.y - 4)};
        result.scrollbar = {result.log.x + result.log.width + 4, result.log.y, 6, result.log.height};
        return result;
    }

    auto chat_module::SelectHistoryTab(const size_t index) -> void
    {
        this->history_view_.tab = index % std::size(chat_history_tabs);
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

    auto chat_module::HandleHistoryKey(const WPARAM key, const LPARAM flags, const bool blocked, const bool replaceNative, const ui::design_system& design) -> bool
    {
        if (!this->WindowVisible() || this->secondary_ || this->Focused() || this->resizing_) return false;
        const bool released = (static_cast<uint32_t>(flags) & 0x80000000u) != 0;
        if (released) return this->HistoryFocused();
        const bool repeated = (static_cast<uint32_t>(flags) & 0x40000000u) != 0;
        const bool control = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool alt = (::GetKeyState(VK_MENU) & 0x8000) != 0 || (static_cast<uint32_t>(flags) & 0x20000000u) != 0;
        const bool shift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (alt && key == VK_TAB) { this->Focus(false); return false; }
        if (!this->HistoryFocused())
        {
            if (key != 'F' || repeated || blocked || !replaceNative || control || alt || shift) return false;
            this->history_view_.focused = true;
            if (this->sounds_ != nullptr) this->sounds_->Select(::GetTickCount64());
            return true;
        }
        if (((key == VK_OEM_2 || key == VK_DIVIDE) && !shift && !control && !alt) || (key == 'R' && alt))
        { this->Focus(false); return false; }
        if (key == VK_ESCAPE) { this->Focus(false); return true; }
        if (key == VK_RETURN)
        {
            if (!repeated)
            {
                if (this->HistoryExpanded()) this->Focus(false);
                else
                {
                    this->history_view_.expanded = true;
                    this->SelectHistoryTab(0);
                    if (this->sounds_ != nullptr) this->sounds_->MenuOpened(::GetTickCount64());
                }
            }
            return true;
        }
        if (key == 'F')
        {
            if (!repeated && this->HistoryExpanded())
            {
                this->SelectHistoryTab(this->history_view_.tab + (shift ? std::size(chat_history_tabs) - 1 : 1));
                if (this->sounds_ != nullptr) this->sounds_->Select(::GetTickCount64());
            }
            return true;
        }
        const auto log = this->HistoryExpanded() ? this->HistoryLayout(design).log : this->Layout(design).log;
        this->RebuildRows(design, log.width);
        this->visible_rows_ = std::max<size_t>(1, static_cast<size_t>(log.height / (design.TextHeight(this->text_.style) + this->line_spacing_)));
        switch (key)
        {
        case VK_LEFT: case VK_PRIOR: this->Scroll(static_cast<int>(std::max<size_t>(1, this->visible_rows_ - 1))); break;
        case VK_RIGHT: case VK_NEXT: this->Scroll(-static_cast<int>(std::max<size_t>(1, this->visible_rows_ - 1))); break;
        case VK_UP: this->ScrollMessage(1); break;
        case VK_DOWN: this->ScrollMessage(-1); break;
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
        if (!this->HistoryExpanded()) return;
        this->SetHistoryViewport(context.Viewport());
        const auto layout = this->HistoryLayout(context.Design());
        context.DrawContainer(layout.frame, this->container_);
        auto label = this->chrome_; label.style.alignment = ui::text_alignment::center;
        for (size_t index = 0; index < layout.tabs.size(); ++index)
            context.DrawButton(layout.tabs[index], chat_history_tabs[index], index == this->history_view_.tab ||
                Contains(layout.tabs[index], this->history_view_.pointer.x, this->history_view_.pointer.y), label);
        this->DrawLog(snapshot, context, layout.log, layout.scrollbar);
        auto help = this->chrome_; help.style.scale *= 0.85f; help.style.alignment = ui::text_alignment::left;
        context.DrawText("F: next tab   Shift+F: previous   Left/Right: page   Up/Down: message   Enter/Esc: close", layout.footer, layout.footer, help);
    }
}
