#pragma once

#include "modules/module.hpp"

#include <algorithm>

namespace xiui::ui
{
    inline constexpr const char* profile_actions[]{"Duplicate profile", "Share profile", "Import profile", "Delete profile"};

    class config_layout
    {
    public:
        static constexpr float sidebar_width = 196.0f;
        static constexpr float default_width = 1100.0f;
        static constexpr float default_height = 600.0f;
        static constexpr float minimum_width = 760.0f;
        static constexpr float submenu_width = 176.0f;
        static constexpr float content_inset = 8.0f;
        static constexpr float grid_y = 74.0f;
        static constexpr float row_height = 56.0f;

        explicit config_layout(const rect& bounds, size_t moduleCount = 0, bool sections = false, bool submenu = false, float extraTop = 0, float selectorHeight = 0)
            : bounds_(bounds), module_count_(moduleCount), sections_(sections), submenu_(submenu), extra_top_(extraTop), selector_height_(selectorHeight) {}

        auto ModuleTab(size_t index) const -> rect
        {
            const float pitch = this->module_count_ > 0
                ? std::min(30.0f, std::max(1.0f, this->bounds_.height - 124.0f) / static_cast<float>(this->module_count_)) : 30.0f;
            return {this->bounds_.x + 8.0f, this->bounds_.y + 38.0f + static_cast<float>(index) * pitch,
                sidebar_width - 16.0f, std::min(28.0f, std::max(1.0f, pitch - 2.0f))};
        }

        auto CloseButton(void) const -> rect
        {
            return {this->bounds_.x + this->bounds_.width - 28.0f, this->bounds_.y + 3.0f, 24.0f, 22.0f};
        }

        auto HeaderAction(size_t index = 0) const -> rect
        {
            return {this->bounds_.x + this->bounds_.width - 56.0f - index * 28.0f, this->bounds_.y + 3.0f, 24.0f, 22.0f};
        }

        auto HeaderBackground(void) const -> rect
        {
            return {this->bounds_.x + 5, this->bounds_.y + 5, std::max(0.0f, this->bounds_.width - 10), 23};
        }

        auto SidebarBackground(void) const -> rect
        {
            return {this->bounds_.x + 5, this->bounds_.y + 28, sidebar_width - 5, std::max(0.0f, this->bounds_.height - 33)};
        }

        auto SubmenuBackground(void) const -> rect
        {
            return {this->bounds_.x + sidebar_width, this->bounds_.y + 28 + this->SectionHeight(), this->submenu_ ? submenu_width : 0, std::max(0.0f, this->bounds_.height - 33 - this->SectionHeight())};
        }

        auto SectionBackground(void) const -> rect
        { return {this->bounds_.x + sidebar_width, this->bounds_.y + 28, this->bounds_.width - sidebar_width - 5, this->SectionHeight()}; }

        auto Submenu(size_t index, size_t count) const -> rect
        {
            const float pitch = count != 0 ? std::min(28.0f, std::max(1.0f, this->bounds_.height - 62 - this->SectionHeight()) / static_cast<float>(count)) : 28;
            return {this->bounds_.x + sidebar_width + 8, this->bounds_.y + 35 + this->SectionHeight() + static_cast<float>(index) * pitch,
                submenu_width - 16, std::max(1.0f, pitch - 2)};
        }

        auto Footer(void) const -> rect { return {this->ContentX(), this->bounds_.y + this->bounds_.height - 20, this->ContentWidth(), 17}; }
        auto ContentTabs(void) const -> rect { return {this->ContentX(), this->bounds_.y + this->ContentTop(), this->ContentWidth(), 28}; }
        auto ContentSubtabs(void) const -> rect { auto bounds = this->ContentTabs(); bounds.y += 34; return bounds; }
        auto ElementTabs(void) const -> rect { return {this->ContentX(), this->bounds_.y + this->ContentTop() + this->extra_top_, this->ContentWidth(), 28}; }
        auto ContentArea(void) const -> rect { return {this->ContentX(), this->bounds_.y + this->OptionsTop(), this->ContentWidth(), std::max(1.0f, this->bounds_.height - this->OptionsTop() - 24)}; }
        auto MinimumWidth(void) const -> float { return minimum_width + (this->submenu_ ? submenu_width : 0); }

        auto HeaderDragArea(void) const -> rect
        {
            return {this->bounds_.x, this->bounds_.y, this->bounds_.width - 86.0f, 28.0f};
        }

        auto ResetButton(void) const -> rect
        {
            return {this->bounds_.x + 8.0f, this->bounds_.y + this->bounds_.height - 78.0f, sidebar_width - 16.0f, 34.0f};
        }

        auto ProfileSelector(void) const -> rect { return {this->bounds_.x + 8, this->bounds_.y + this->bounds_.height - 36, sidebar_width - 76, 26}; }
        auto ProfileAdd(void) const -> rect { const auto selector = this->ProfileSelector(); return {selector.x + selector.width + 4, selector.y, 26, 26}; }
        auto ProfileActionsButton(void) const -> rect { const auto add = this->ProfileAdd(); return {add.x + add.width + 4, add.y, 26, 26}; }
        auto ProfileActionsMenu(void) const -> rect
        {
            const auto button = this->ProfileActionsButton();
            const float height = std::size(profile_actions) * 28.0f + 8;
            return {this->bounds_.x + 8, button.y - height - 4, sidebar_width - 16, height};
        }
        auto ProfileActionRow(size_t index) const -> rect { const auto menu = this->ProfileActionsMenu(); return {menu.x + 4, menu.y + 4 + index * 28, menu.width - 8, 28}; }
        auto ProfileMenu(size_t count) const -> rect { const auto selector = this->ProfileSelector(); const float height = std::min<size_t>(7, count) * 28.0f + 8; return {selector.x, selector.y - height - 4, sidebar_width - 16, height}; }
        auto ProfileRow(size_t index, size_t count) const -> rect { const auto menu = this->ProfileMenu(count); return {menu.x + 4, menu.y + 4 + index * 28, menu.width - 8, 26}; }

        static auto NavigationIcon(const rect& row) -> rect { return {row.x + 5, row.y, 30, row.height}; }
        static auto NavigationLabel(const rect& row) -> rect { return {row.x + 38, row.y, row.width - 46, row.height}; }

        auto OptionRow(int visibleIndex) const -> rect
        {
            constexpr float columnGap = 10.0f;
            const float width = std::max(1.0f, (this->ContentWidth() - columnGap) * 0.5f);
            return {
                this->ContentX() + static_cast<float>(visibleIndex % 2) * (width + columnGap),
                this->bounds_.y + this->OptionsTop() + static_cast<float>(visibleIndex / 2) * row_height,
                width,
                row_height};
        }

        auto Category(size_t index, size_t count) const -> rect
        {
            constexpr float gap = 6.0f;
            if (count == 0 || index >= count) return {};
            const float width = std::clamp((this->ContentWidth() - gap * static_cast<float>(count - 1)) / static_cast<float>(count), 1.0f, 176.0f);
            return {this->ContentX() + static_cast<float>(index) * (width + gap), this->bounds_.y + 36.0f + this->SectionHeight(), width, 30.0f};
        }

        auto Section(size_t index, size_t count = 2) const -> rect
        {
            if (count == 0 || index >= count) return {};
            const float available = this->bounds_.width - sidebar_width - content_inset * 2;
            const float width = (available - 6 * static_cast<float>(count - 1)) / static_cast<float>(count);
            return {this->bounds_.x + sidebar_width + content_inset + static_cast<float>(index) * (width + 6), this->bounds_.y + 35, width, 26};
        }

        auto VisibleRows(void) const -> int
        {
            return std::max(1, static_cast<int>((this->bounds_.height - this->OptionsTop() - 24.0f) / row_height));
        }

        auto ColorEditor(const D3DVIEWPORT8& viewport) const -> rect
        {
            const float width = std::min(300.0f, std::max(1.0f, static_cast<float>(viewport.Width) - 16));
            const float height = std::min(404.0f, std::max(1.0f, static_cast<float>(viewport.Height) - 16));
            constexpr float gap = 6.0f;
            const float left = this->bounds_.x - width - gap;
            const float right = this->bounds_.x + this->bounds_.width + gap;
            float x{};
            float y = this->bounds_.y + 34.0f;
            if (left >= 0.0f)
                x = left;
            else if (right + width <= static_cast<float>(viewport.Width))
                x = right;
            else
            {
                x = this->bounds_.x + this->bounds_.width - width - 8.0f;
                y = this->bounds_.y + this->bounds_.height - height - 4.0f;
            }
            return {
                std::clamp(x, 0.0f, std::max(0.0f, static_cast<float>(viewport.Width) - width)),
                std::clamp(y, 0.0f, std::max(0.0f, static_cast<float>(viewport.Height) - height)),
                width,
                height};
        }

        static auto SliderTrack(const rect& row) -> rect
        {
            const auto value = SliderValue(row);
            const float x = row.x + row.width * 0.52f;
            return {x, row.y + 13.0f, std::max(1.0f, value.x - x - 6), 4.0f};
        }

        static auto SliderValue(const rect& row) -> rect
        {
            const float width = std::min(64.0f, row.width * 0.37f * 0.48f);
            return {row.x + row.width - width - 6, row.y + 3, width, 20};
        }

        static auto ChoiceButton(const rect& row) -> rect
        {
            const float width = std::min(148.0f, row.width * 0.44f);
            return {row.x + row.width - width - 4, row.y + 3, width, 24};
        }

        static auto ChoicePrevious(const rect& row) -> rect
        { const auto button = ChoiceButton(row); return {button.x, button.y, 22, button.height}; }
        static auto ChoiceNext(const rect& row) -> rect
        { const auto button = ChoiceButton(row); return {button.x + button.width - 22, button.y, 22, button.height}; }
        static auto ChoiceLabel(const rect& row) -> rect
        { const auto button = ChoiceButton(row); return {button.x + 22, button.y, std::max(0.0f, button.width - 44), button.height}; }

        static auto TextValue(const rect& row) -> rect
        {
            const float width = row.width * 0.68f;
            return {row.x + row.width - width - 4, row.y + 3, width, 24};
        }

        static auto OverrideSwatch(const rect& row) -> rect { return {row.x + row.width - 34, row.y + 5, 28, 20}; }
        static auto OverrideSource(const rect& row) -> rect { return {row.x + row.width - 102, row.y + 3, 62, 24}; }

        static auto Description(const rect& row) -> rect { return {row.x + 3, row.y + 24, row.width - 8, 16}; }

        static auto Tooltip(const rect& anchor, const float requestedWidth, const D3DVIEWPORT8& viewport) -> rect
        {
            constexpr float margin = 8, height = 28;
            const float left = static_cast<float>(viewport.X) + margin, top = static_cast<float>(viewport.Y) + margin;
            const float right = static_cast<float>(viewport.X + viewport.Width) - margin;
            const float bottom = static_cast<float>(viewport.Y + viewport.Height) - margin;
            const float width = std::min(requestedWidth, std::max(1.0f, right - left));
            const bool below = anchor.width > sidebar_width;
            float x = below ? anchor.x : anchor.x + anchor.width + margin;
            const float y = below ? anchor.y + anchor.height + margin : anchor.y + (anchor.height - height) * 0.5f;
            if (!below && x + width > right) x = anchor.x - width - margin;
            return {std::clamp(x, left, std::max(left, right - width)), std::clamp(y, top, std::max(top, bottom - height)), width, height};
        }

    private:
        rect bounds_{};
        size_t module_count_{};
        bool sections_{};
        bool submenu_{};
        float extra_top_{};
        float selector_height_{};
        auto SectionHeight(void) const -> float { return this->sections_ ? 34.0f : 0.0f; }
        auto ContentTop(void) const -> float { return (this->submenu_ ? 35.0f : grid_y) + this->SectionHeight(); }
        auto OptionsTop(void) const -> float { return this->ContentTop() + this->extra_top_ + this->selector_height_; }

        auto ContentX(void) const -> float { return this->bounds_.x + sidebar_width + (this->submenu_ ? submenu_width : 0) + content_inset; }
        auto ContentWidth(void) const -> float { return std::max(1.0f, this->bounds_.width - sidebar_width - (this->submenu_ ? submenu_width : 0) - content_inset * 2.0f); }
    };
} // namespace xiui::ui
