#include "hud_runtime.hpp"
#include "config/contributor_list.hpp"

namespace xiui
{
    auto hud_runtime::ElementSelector(void) const -> ui::element_selector
    {
        ui::element_selector result{};
        if (this->config_.module == nullptr) return result;
        for (size_t index = 0; index < this->config_.module->OptionCount(); ++index)
        {
            if (!this->MatchesConfigOption(index)) continue;
            const auto option = this->config_.module->Option(index);
            if (option.kind != module_option_kind::choice || option.choice == nullptr || option.choices == nullptr || option.choice_count == 0) continue;
            if (option.selector == module_selector::element) { result.element = option; result.option_index = index; }
            if (option.selector == module_selector::scope) { result.scope = option; result.scope_index = index; }
        }
        if (result.element.choice == nullptr) return result;
        result.combined = result.scope.choice != nullptr && result.scope.choice_count == 2;
        if (result.combined) result.labels[result.count++] = result.scope.ChoiceText(0);
        for (size_t i = 0; i < result.element.choice_count && result.count < result.labels.size(); ++i)
            result.labels[result.count++] = result.element.ChoiceText(i);
        const auto selected = static_cast<size_t>(std::clamp(*result.element.choice, 0, static_cast<int>(result.element.choice_count) - 1));
        result.selected = result.combined ? (*result.scope.choice == 0 ? 0 : selected + 1) : selected;
        return result;
    }

    auto hud_runtime::ElementSelectorLayout(const ui::element_selector& selector, const ui::config_layout& layout) const -> ui::element_selector_layout
    {
        std::array<float, ui::element_selector::capacity> widths{};
        auto style = this->design_system_.GetTheme().body_text; style.scale = 0.72f;
        style = this->config_controls_.Style(style);
        for (size_t i = 0; i < selector.count; ++i)
            widths[i] = std::clamp(this->design_system_.MeasureText(selector.labels[i], style) + 24 + (selector.Status(i).has_value() ? 12 : 0), 64.0f, 184.0f);
        return {layout.ElementTabs(), std::span{widths}.first(selector.count)};
    }

    auto hud_runtime::ConfigLayout(void) const -> ui::config_layout
    {
        if (this->config_.basic) return ui::config_layout::Basic(this->config_.bounds);
        const auto pageCount = this->VisibleConfigPages().size();
        const bool sections = false, submenu = !this->ConfigTopicRows().empty();
        const auto submenuCount = this->ConfigTopicRows().size();
        float extra = this->IsExperienceDisplay() || this->IsInventorySettings() ? 40.0f : 0;
        if (this->ConfigCategories().count != 0) extra += ui::config_layout::category_height;
        if (this->IsGeneralAppearance() && (this->general_.AppearanceTab() == 1 || this->general_.AppearanceTab() == 3)) extra += 56;
        if (this->IsAttribution()) extra += 40;
        const ui::config_layout base{this->config_.bounds, pageCount, sections, submenu, extra, 0, false, -1, 0, this->config_.sidebar_collapsed, submenuCount};
        const auto height = this->ElementSelectorLayout(this->ElementSelector(), base).height;
        int firstCardRow = -1;
        const auto slots = this->GroupOptionSlots();
        for (size_t slot = 0; this->config_.module != nullptr && slot < slots.size(); ++slot)
            if (slots[slot] < this->config_.module->OptionCount() && this->config_.module->Option(slots[slot]).card)
            { firstCardRow = base.RowForSlot(static_cast<int>(slot)); break; }
        const ui::config_layout layout{this->config_.bounds, pageCount, sections, submenu, extra, height, false, firstCardRow, base.RowForSlot(this->config_.scroll), this->config_.sidebar_collapsed, submenuCount};
        const bool customLists = this->IsExperienceDisplay() && this->experience_display_.View() == experience_display_view::readouts;
        const bool scrollbar = this->IsXIUIContributors() ? ui::contributor_list_layout{layout}.MaximumScroll() > 0 :
            !customLists && slots.size() > static_cast<size_t>(layout.VisibleSlots());
        auto result = ui::config_layout{this->config_.bounds, pageCount, sections, submenu, extra, height, scrollbar, firstCardRow, base.RowForSlot(this->config_.scroll), this->config_.sidebar_collapsed, submenuCount};
        if (customLists) result = result.WithMinimumContentHeight(ui::experience_display_editor::geometry::MinimumHeight(result.ContentArea().width));
        return result;
    }

    auto hud_runtime::ElementSelectorTarget(void) const -> uint64_t
    {
        const auto selector = this->ElementSelector();
        if (!selector.Enabled()) return 0;
        const auto layout = this->ElementSelectorLayout(selector, this->ConfigLayout());
        const auto index = layout.IndexAt(this->ConfigMouseX(), this->ConfigMouseY());
        return index ? (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(this->config_.module)) << 32) | (0x60000 + selector.option_index * 16 + *index) : 0;
    }

    auto hud_runtime::ProcessElementSelectorClick(void) -> bool
    {
        const auto selector = this->ElementSelector();
        const auto layout = this->ElementSelectorLayout(selector, this->ConfigLayout());
        const auto index = layout.IndexAt(this->ConfigMouseX(), this->ConfigMouseY());
        if (!index) return false;
        if (!selector.Enabled() || *index == selector.selected) return true;
        this->FinishConfigInput(true);
        this->config_.active_color = nullptr; this->config_.active_slider = nullptr; this->config_.scroll = 0;
        if (selector.combined)
        {
            const int scope = *index == 0 ? 0 : 1;
            if (*selector.scope.choice != scope)
            {
                *selector.scope.choice = scope;
                this->config_.module->OnOptionChanged(selector.scope_index);
            }
        }
        if (!selector.combined || *index != 0)
        {
            *selector.element.choice = static_cast<int>(*index - (selector.combined ? 1 : 0));
            this->config_.module->OnOptionChanged(selector.option_index);
        }
        this->SaveSettings();
        return true;
    }

    auto hud_runtime::DrawElementSelector(void) -> void
    {
        const auto selector = this->ElementSelector();
        const auto layout = this->ElementSelectorLayout(selector, this->ConfigLayout());
        auto style = this->design_system_.GetTheme().body_text;
        style.scale = 0.72f; style.alignment = ui::text_alignment::center;
        if (!selector.Enabled()) style.top_color = style.bottom_color = 0x80FFFFFF;
        std::array<ui::slot_dot, ui::element_selector::capacity> dots{};
        size_t dotCount{};
        for (size_t i = 0; i < layout.count; ++i)
        {
            const auto& bounds = layout.tabs[i];
            const bool hovered = Contains(bounds, this->ConfigMouseX(), this->ConfigMouseY());
            const bool highlighted = selector.Enabled() && (selector.selected == i || hovered);
            if (const auto status = selector.Status(i); status.has_value())
            {
                this->config_controls_.Button(bounds, "", style, highlighted);
                this->config_controls_.ButtonText(selector.labels[i], {bounds.x + 12, bounds.y, bounds.width - 12, bounds.height}, style, highlighted, selector.Enabled() && hovered);
                dots[dotCount++] = {{bounds.x + 8, bounds.y + (bounds.height - 7) / 2, 7, 7}, *status ? 0xFF8FD16A : 0xFF161616};
            }
            else this->config_controls_.Button(bounds, selector.labels[i], style, highlighted, ui::button_tone::normal, selector.Enabled() && hovered);
        }
        this->design_system_.DrawSlotDots(std::span{dots}.first(dotCount));
    }
}
