#pragma once

#include "modules/module.hpp"
#include "config_scrollbar.hpp"

#include <algorithm>
#include <optional>

namespace xiui::ui
{
    class choice_dropdown
    {
    public:
        auto Open(const module_option& option, const rect& anchor) -> void
        {
            this->option_ = option; this->anchor_ = anchor;
            this->scrolling_ = false;
            this->open_ = option.choice != nullptr && option.choice_count > 0;
            this->highlighted_ = this->open_ ? std::clamp(*option.choice, 0, static_cast<int>(option.choice_count) - 1) : 0;
            this->first_ = std::max(0, this->highlighted_ - 3);
            this->Scroll(0);
        }
        auto Cancel(void) -> void { this->open_ = this->scrolling_ = false; }
        auto Opened(void) const -> bool { return this->open_; }
        auto Option(void) const -> const module_option& { return this->option_; }
        auto Highlighted(void) const -> int { return this->highlighted_; }
        auto First(void) const -> int { return this->first_; }
        auto Scrolling(void) const -> bool { return this->scrolling_; }
        auto VisibleCount(void) const -> int { return std::min(8, static_cast<int>(this->option_.choice_count)); }
        auto Scroll(int delta) -> void { this->first_ = std::clamp(this->first_ + delta, 0, std::max(0, static_cast<int>(this->option_.choice_count) - this->VisibleCount())); }
        auto Move(int delta) -> void
        {
            this->highlighted_ = std::clamp(this->highlighted_ + delta, 0, std::max(0, static_cast<int>(this->option_.choice_count) - 1));
            if (this->highlighted_ < this->first_) this->first_ = this->highlighted_;
            if (this->highlighted_ >= this->first_ + this->VisibleCount()) this->first_ = this->highlighted_ - this->VisibleCount() + 1;
        }
        auto Bounds(const D3DVIEWPORT8& viewport) const -> rect
        {
            const float left = static_cast<float>(viewport.X) + 4, top = static_cast<float>(viewport.Y) + 4;
            const float right = static_cast<float>(viewport.X + viewport.Width) - 4, bottom = static_cast<float>(viewport.Y + viewport.Height) - 4;
            const float width = std::min(std::max(this->option_.choice_preview ? 240.0f : 148.0f, this->anchor_.width), std::max(1.0f, right - left));
            const float height = this->VisibleCount() * 26.0f + 8 + (this->option_.choice_preview ? 20 : 0);
            const float y = this->anchor_.y + this->anchor_.height + 3;
            return {std::clamp(this->anchor_.x, left, std::max(left, right - width)),
                std::clamp(y + height <= bottom ? y : this->anchor_.y - height - 3, top, std::max(top, bottom - height)), width, height};
        }
        auto Row(int index, const D3DVIEWPORT8& viewport) const -> rect
        {
            const auto bounds = this->Bounds(viewport);
            return {bounds.x + 4, bounds.y + 4 + (index - this->first_) * 26.0f, bounds.width - 8 - (this->Scrollbar(viewport).Visible() ? 16 : 0), 26};
        }
        auto Scrollbar(const D3DVIEWPORT8& viewport) const -> config_scrollbar
        {
            const auto bounds = this->Bounds(viewport);
            return {{bounds.x + bounds.width - 16, bounds.y + 4, 12, this->VisibleCount() * 26.0f},
                static_cast<int>(this->option_.choice_count), this->VisibleCount(), this->first_};
        }
        auto HandleScrollPointer(const uint32_t message, const float x, const float y, const D3DVIEWPORT8& viewport) -> bool
        {
            if (!this->open_) return false;
            const auto scrollbar = this->Scrollbar(viewport);
            if (this->scrolling_ && (message == WM_MOUSEMOVE || message == WM_LBUTTONUP))
            {
                this->first_ = scrollbar.PositionAt(y, this->scroll_grab_);
                if (message == WM_LBUTTONUP) this->scrolling_ = false;
                return true;
            }
            if ((message != WM_LBUTTONDOWN && message != WM_LBUTTONDBLCLK) || !scrollbar.Visible() || !Contains(scrollbar.track, x, y)) return false;
            const auto thumb = scrollbar.Thumb();
            if (Contains(thumb, x, y)) { this->scrolling_ = true; this->scroll_grab_ = y - thumb.y; }
            else this->first_ = scrollbar.PageAt(y);
            return true;
        }
        auto IndexAt(float x, float y, const D3DVIEWPORT8& viewport) const -> std::optional<int>
        {
            for (int i = this->first_; i < this->first_ + this->VisibleCount(); ++i)
            {
                const auto row = this->Row(i, viewport);
                if (x >= row.x && y >= row.y && x < row.x + row.width && y < row.y + row.height) return i;
            }
            return std::nullopt;
        }
        auto PreviewButton(int index, const D3DVIEWPORT8& viewport) const -> rect
        {
            const auto row = this->Row(index, viewport);
            return {row.x + row.width - 25, row.y + 2, 22, 22};
        }
        auto PreviewAt(float x, float y, const D3DVIEWPORT8& viewport) const -> bool
        {
            const auto index = this->IndexAt(x, y, viewport);
            if (!this->option_.choice_preview || !index || static_cast<size_t>(*index) < this->option_.preview_first) return false;
            const auto button = this->PreviewButton(*index, viewport);
            return x >= button.x && y >= button.y && x < button.x + button.width && y < button.y + button.height;
        }

    private:
        module_option option_{};
        rect anchor_{};
        bool open_{}, scrolling_{};
        int highlighted_{}, first_{};
        float scroll_grab_{};
        static auto Contains(const rect& bounds, const float x, const float y) -> bool
        { return x >= bounds.x && y >= bounds.y && x < bounds.x + bounds.width && y < bounds.y + bounds.height; }
    };
}
