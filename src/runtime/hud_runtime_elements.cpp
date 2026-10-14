#include "hud_runtime.hpp"

namespace xiui
{
    auto hud_runtime::ElementSelector(void) const -> ui::element_selector
    {
        ui::element_selector result{};
        if (this->active_module_ == nullptr) return result;
        for (size_t index = 0; index < this->active_module_->OptionCount(); ++index)
        {
            if (!this->MatchesConfigOption(index)) continue;
            const auto option = this->active_module_->Option(index);
            if (option.kind != module_option_kind::choice || option.choice == nullptr || option.choices == nullptr || option.choice_count == 0) continue;
            if (option.selector == module_selector::element) { result.element = option; result.option_index = index; }
            if (option.selector == module_selector::scope) { result.scope = option; result.scope_index = index; }
        }
        if (result.element.choice == nullptr) return result;
        result.combined = result.scope.choice != nullptr && result.scope.choice_count == 2;
        if (result.combined) result.labels[result.count++] = result.scope.choices[0];
        for (size_t i = 0; i < result.element.choice_count && result.count < result.labels.size(); ++i)
            result.labels[result.count++] = result.element.choices[i];
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
            widths[i] = std::clamp(this->design_system_.MeasureText(selector.labels[i], style) + 24, 64.0f, 184.0f);
        return {layout.ElementTabs(), std::span{widths}.first(selector.count)};
    }

    auto hud_runtime::ConfigLayout(void) const -> ui::config_layout
    {
        const auto pageCount = this->VisibleConfigPages().size();
        const bool sections = this->HasConfigSections(), submenu = this->HasConfigSubmenu();
        float extra = this->IsGeneralAppearance() || this->IsExperienceDisplay() || this->IsAttribution() || this->IsInventorySettings() ? 40.0f : 0;
        if (this->IsGeneralAppearance() && this->general_.AppearanceTab() == 1) extra += 56;
        const ui::config_layout base{this->config_bounds_, pageCount, sections, submenu, extra};
        const auto height = this->ElementSelectorLayout(this->ElementSelector(), base).height;
        return ui::config_layout{this->config_bounds_, pageCount, sections, submenu, extra, height};
    }

    auto hud_runtime::ElementSelectorTarget(void) const -> uint64_t
    {
        const auto selector = this->ElementSelector();
        if (!selector.Enabled()) return 0;
        const auto layout = this->ElementSelectorLayout(selector, this->ConfigLayout());
        const auto index = layout.IndexAt(static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
        return index ? (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(this->active_module_)) << 32) | (0x60000 + selector.option_index * 16 + *index) : 0;
    }

    auto hud_runtime::ProcessElementSelectorClick(void) -> bool
    {
        const auto selector = this->ElementSelector();
        const auto layout = this->ElementSelectorLayout(selector, this->ConfigLayout());
        const auto index = layout.IndexAt(static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
        if (!index) return false;
        if (!selector.Enabled() || *index == selector.selected) return true;
        this->FinishConfigInput(true);
        this->active_color_ = nullptr; this->active_slider_ = nullptr; this->option_scroll_ = 0;
        if (selector.combined)
        {
            const int scope = *index == 0 ? 0 : 1;
            if (*selector.scope.choice != scope)
            {
                *selector.scope.choice = scope;
                this->active_module_->OnOptionChanged(selector.scope_index);
            }
        }
        if (!selector.combined || *index != 0)
        {
            *selector.element.choice = static_cast<int>(*index - (selector.combined ? 1 : 0));
            this->active_module_->OnOptionChanged(selector.option_index);
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
        for (size_t i = 0; i < layout.count; ++i)
        {
            const bool hovered = Contains(layout.tabs[i], static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
            this->config_controls_.Button(layout.tabs[i], selector.labels[i], style, selector.Enabled() && (selector.selected == i || hovered));
        }
    }
}
