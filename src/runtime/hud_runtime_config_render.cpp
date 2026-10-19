#include "hud_runtime.hpp"
#include "config/contributor_list.hpp"
#include "config/option_layout.hpp"
#include "config/config_window_resize.hpp"
#include "input/keyboard_policy.hpp"
#include "ui/sound_choices.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>

namespace xiui
{
    auto hud_runtime::DrawConfig(const D3DVIEWPORT8& viewport) -> void
    {
        this->NormalizeConfigSelection();
        this->ClampOptionScroll();
        const auto layout = this->ConfigLayout();
        this->config_.tooltip.clear();
        const auto panel = this->ConfigVisibleBounds();
        const auto& theme = this->design_system_.GetTheme();
        const std::array<ui::container_fill, 4> fills{{
            {layout.HeaderBackground(), 0xD0000000 | ((theme.colors.chrome & 0xFEFEFE) >> 1)},
            {layout.SidebarBackground(), (theme.colors.surface_raised & 0xFFFFFF) | 0x60000000},
            {layout.SubmenuBackground(), (theme.colors.surface_raised & 0xFFFFFF) | 0x30000000},
            {layout.SectionBackground(), (theme.colors.surface_raised & 0xFFFFFF) | 0x30000000}}};
        this->config_controls_.Panel(panel, std::span{fills}.first(this->config_.collapsed ? 1 : fills.size()));
        auto headerStyle = theme.accent_text;
        headerStyle.scale = 0.82f;
        this->config_controls_.Text("CONFIGURATION", layout.HeaderTitle(), headerStyle);
        const auto sidebarToggle = layout.SidebarToggle();
        const bool sidebarHovered = Contains(sidebarToggle, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
        this->config_controls_.IconButton(sidebarToggle, this->config_.sidebar_collapsed ? ui::icon_glyph::arrow_right : ui::icon_glyph::arrow_left, sidebarHovered);
        const rect close = layout.CloseButton();
        const bool closeHovered = Contains(close, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
        this->config_controls_.IconButton(close, ui::icon_glyph::close, closeHovered);

        const auto attribution = layout.HeaderAction();
        const bool attributionHovered = Contains(attribution, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
        this->config_controls_.IconButton(attribution, ui::icon_glyph::attribution, attributionHovered ||
            (this->config_.module == &this->general_ && this->config_.section == general_module::attribution_section));
        const char* tooltip = attributionHovered ? "Attribution" : nullptr;
        rect tooltipAnchor = attribution;
        if (sidebarHovered)
        {
            tooltip = this->config_.sidebar_collapsed ? "Expand sidebar" : "Collapse sidebar to icons";
            tooltipAnchor = sidebarToggle;
        }
        const auto eye = layout.HeaderAction(1);
        const bool eyeHovered = Contains(eye, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
        this->config_controls_.IconButton(eye, this->config_.collapsed ? ui::icon_glyph::eye_closed : ui::icon_glyph::eye, eyeHovered || this->config_.collapsed);
        if (eyeHovered)
        {
            tooltip = this->config_.collapsed ? "Expand configuration" : "Collapse to title bar";
            tooltipAnchor = eye;
        }
        if (this->config_.collapsed)
        {
            if (tooltip != nullptr) this->config_controls_.Tooltip(tooltip, tooltipAnchor, viewport);
            return;
        }
        const auto pages = this->VisibleConfigPages();
        for (size_t index = 0; index < pages.size(); ++index)
        {
            const auto& page = pages[index];
            const rect bounds = layout.ModuleTab(index);
            const bool active = page.Contains(this->config_.module);
            const bool hovered = Contains(bounds, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
            this->config_controls_.NavigationButton(bounds, page.id, page.name, active, hovered, this->config_.sidebar_collapsed);
            if (this->config_.sidebar_collapsed && hovered) { tooltip = page.name; tooltipAnchor = bounds; }
        }

        this->DrawProfileControls();

        const auto page = this->ActiveConfigPage();
        if (page.SectionCount() > 1)
        {
            constexpr const char* chatNames[]{"Window 1", "Window 2", "Bubbles"};
            for (size_t index = 0; index < page.SectionCount(); ++index)
            {
                const auto bounds = layout.Section(index, page.SectionCount());
                const bool active = page.primary == &this->actionbars_ && this->actionbars_.FloatingWindowOpen() && this->actionbars_.WindowFocused() ?
                    page.SectionView(index) == this->actionbars_.WindowConfigView() : page.SectionContains(index, this->config_.module);
                const bool highlighted = active || Contains(bounds, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
                auto style = active ? theme.accent_text : theme.caption_text;
                style.scale = 0.72f; style.alignment = ui::text_alignment::center;
                this->config_controls_.Button(bounds, std::strcmp(page.id, "chat") == 0 ? chatNames[index] : page.SectionName(index), style, highlighted);
            }
        }

        static constexpr const char* categoryNames[] = {"GENERAL", "APPEARANCE", "EFFECTS", "COLORS", "TEXT"};
        const auto topics = this->config_.module != nullptr ? this->config_.module->ConfigSections() : std::span<const module_config_section>{};
        if (!topics.empty())
        {
            this->config_.section = std::min(this->config_.section, topics.size() - 1);
            for (size_t index = 0; index < topics.size(); ++index)
            {
                const auto bounds = layout.Submenu(index, topics.size());
                const bool active = this->config_.section == index;
                const bool hovered = Contains(bounds, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
                this->config_controls_.NavigationItem(bounds, topics[index].name, active, hovered);
            }
        }
        else
        {
            const auto categories = this->ConfigCategories();
            for (size_t index = 0; index < categories.count; ++index)
            {
                const auto group = categories.groups[index];
                const rect bounds = layout.Category(index, categories.count);
                const bool active = this->config_.group == group;
                const bool hovered = Contains(bounds, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
                auto style = active ? theme.accent_text : theme.caption_text;
                style.scale = 0.72f;
                style.alignment = ui::text_alignment::center;
                this->config_controls_.Button(bounds, categoryNames[static_cast<size_t>(group)], style, active || hovered);
            }
        }

        if (this->config_.module != nullptr)
        {
            if (this->IsInventorySettings())
            {
                std::array<const char*, inventory_groups.size()> names{};
                size_t selected{};
                for (size_t index = 0; index < names.size(); ++index)
                {
                    names[index] = this->inventory_pages_[index]->DisplayName();
                    if (this->config_.module == this->inventory_pages_[index]) selected = index;
                }
                this->config_controls_.Segmented(layout.ContentTabs(), names, selected,
                    static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
            }
            if (this->IsExperienceDisplay()) this->experience_display_.Draw(this->experience_, this->config_controls_, this->design_system_,
                layout, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
            if (this->IsAttribution()) this->config_controls_.Segmented(layout.ContentTabs(), ui::attribution_views,
                this->general_.AttributionView(), static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
            if (this->IsGeneralAppearance()) this->config_controls_.Segmented(layout.ContentTabs(), general_module::appearance_tabs,
                this->general_.AppearanceTab(), static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
            this->DrawElementSelector();
            if (this->IsXIUIContributors()) this->DrawContributorList();
            else
            {
                const int visibleOptions = layout.VisibleSlots();
                const auto slots = this->GroupOptionSlots();
                for (int visibleIndex = 0; visibleIndex < visibleOptions; ++visibleIndex)
                {
                    const auto slot = static_cast<size_t>(this->config_.scroll + visibleIndex);
                    if (slot >= slots.size()) break;
                    const size_t optionIndex = slots[slot];
                    if (optionIndex >= this->config_.module->OptionCount())
                        continue;
                    const module_option option = this->config_.module->Option(optionIndex);
                    const rect optionRow = layout.OptionRow(visibleIndex);
                    const bool hovered = Contains(optionRow, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
                    const bool editingValue = this->config_.slider_input.Focused() && this->config_.slider_module == this->config_.module && this->config_.slider_option == optionIndex;
                    if (editingValue) this->config_.slider_bounds = ui::config_layout::SliderValue(optionRow);
                    const bool editingText = this->config_.text_input.Focused() && this->config_.text_module == this->config_.module && this->config_.text_option == optionIndex;
                    if (editingText) this->config_.text_bounds = ui::config_layout::TextValue(optionRow);
                    int hoveredButton = -1;
                    if (hovered && option.kind == module_option_kind::toggle_buttons)
                        for (size_t index = 0; index < option.choice_count; ++index)
                            if (Contains(ui::config_layout::ToggleButton(optionRow, index, option.choice_count), static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_)))
                                hoveredButton = static_cast<int>(index);
                    this->config_controls_.Option(optionRow, option, hovered, this->config_.active_color == option.color,
                        editingValue ? &this->config_.slider_input : nullptr, editingText ? &this->config_.text_input : nullptr,
                        this->config_.choice_input.Opened() && this->config_.choice_module == this->config_.module && this->config_.choice_option == optionIndex, hoveredButton);
                    if (option.available && option.kind == module_option_kind::choice && option.choice_count > 1 && hovered &&
                        Contains(ui::config_layout::ChoiceButton(optionRow), static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_)))
                    {
                        tooltip = Contains(ui::config_layout::ChoicePrevious(optionRow), static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_))
                            ? "Previous choice" : "Next choice";
                        tooltipAnchor = ui::config_layout::ChoiceButton(optionRow);
                    }
                    if (option.available && option.kind == module_option_kind::slider && hovered &&
                        Contains(ui::config_layout::SliderValue(optionRow), static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_)))
                    {
                        tooltip = editingValue && this->config_.slider_input.Invalid() ? "Enter a valid number, or press Escape to cancel." : "Click to type a value. Enter applies; Escape cancels.";
                        tooltipAnchor = ui::config_layout::SliderValue(optionRow);
                    }
                    const rect helpBounds = option.kind == module_option_kind::toggle_buttons ? rect{optionRow.x, optionRow.y, optionRow.width, 25} : ui::config_layout::Description(optionRow);
                    if (hovered && Contains(helpBounds, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_)))
                    {
                        tooltip = option.description;
                        tooltipAnchor = optionRow;
                    }
                }

                const int totalOptions = static_cast<int>(slots.size());
                if (this->IsGeneralAppearance() && this->general_.AppearanceTab() == 1)
                {
                    const auto& sample = this->general_.GlobalTextStyle();
                    auto area = layout.ContentSubtabs();
                    area.height = 48;
                    this->design_system_.DrawText("Aa Bb Cc 0123456789 - XIUI", area, sample);
                }
                else if (this->IsGeneralAppearance() && this->general_.AppearanceTab() == 3)
                {
                    const auto area = layout.ContentSubtabs();
                    const rect sample{area.x + 16, area.y + 10, std::min(220.0f, area.width - 32), 28};
                    this->config_controls_.Button(sample, "Controller focus", theme.body_text, false);
                    this->design_system_.DrawFocusFrame(sample, this->general_.FocusStyle());
                }
                if (totalOptions > visibleOptions)
                {
                    char scrollText[32]{};
                    std::snprintf(scrollText, sizeof(scrollText), "%d-%d / %d", this->config_.scroll + 1, std::min(totalOptions, this->config_.scroll + visibleOptions), totalOptions);
                    auto scrollStyle = theme.caption_text;
                    scrollStyle.scale = 0.68f;
                    scrollStyle.alignment = ui::text_alignment::center;
                    this->config_controls_.Text(scrollText, layout.Footer(), scrollStyle);
                }
            }
        }

        this->DrawConfigScrollbar();
        this->DrawControllerInputStatus();
        const auto grip = layout.ResizeGrip();
        const bool resizeHovered = Contains(grip, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
        const auto gripColor = resizeHovered || this->interaction_ == interaction::resize_config || this->config_.focus.resizing
            ? theme.colors.control_selected : theme.colors.muted;
        for (int row = 0; row < 3; ++row)
            for (int column = 0; column <= row; ++column)
                this->FillRect({grip.x + grip.width - 5 - column * 4, grip.y + grip.height - 5 - (row - column) * 4, 2, 2}, gripColor);
        if (resizeHovered)
        {
            tooltip = "Drag an edge or corner to resize. Alt+F8: resize with arrow keys.";
            tooltipAnchor = grip;
        }
        if (tooltip != nullptr && this->interaction_ == interaction::none)
        {
            this->config_.tooltip = tooltip;
            this->config_.tooltip_anchor = tooltipAnchor;
        }
    }

    auto hud_runtime::DrawContributorList(void) -> void
    {
        this->ClampOptionScroll();
        const ui::contributor_list_layout layout{this->ConfigLayout(), this->config_.scroll};
        const auto& theme = this->design_system_.GetTheme();
        auto heading = theme.title_text;
        heading.top_color = heading.bottom_color = theme.colors.gold;
        heading.scale = 0.9f;
        heading.alignment = ui::text_alignment::left;
        for (size_t group = 0; group < std::size(ui::contributor_groups); ++group)
        {
            const auto header = layout.Header(group);
            if (layout.Visible(header)) this->config_controls_.Text(ui::contributor_groups[group], header, heading);
        }
        const float x = static_cast<float>(this->mouse_x_), y = static_cast<float>(this->mouse_y_);
        const auto hovered = layout.MemberAt(x, y);
        for (size_t index = 0; index < ui::xiui_contributors.size(); ++index)
        {
            const auto row = layout.Member(index);
            if (!layout.Visible(row)) continue;
            const auto option = this->general_.Option(this->general_.ContributorsStart() + index);
            this->config_controls_.Option(row, option, hovered && *hovered == index, false);
            if (Contains(ui::config_layout::Description(row), x, y))
            {
                this->config_.tooltip = option.description;
                this->config_.tooltip_anchor = row;
            }
        }
        if (layout.MaximumScroll() > 0)
        {
            auto style = theme.caption_text;
            style.scale = 0.68f; style.alignment = ui::text_alignment::center;
            this->config_controls_.Text("Scroll for more contributors", this->ConfigLayout().Footer(), style);
        }
    }

    auto hud_runtime::DrawConfigTooltip(const D3DVIEWPORT8& viewport) -> void
    {
        if (this->config_.search_open) return;
        if (!this->config_.tooltip.empty() && this->interaction_ == interaction::none)
            this->config_controls_.Tooltip(this->config_.tooltip.c_str(), this->config_.tooltip_anchor, viewport);
    }

    auto hud_runtime::DrawColorEditor(const D3DVIEWPORT8& viewport) -> void
    {
        if (this->config_.active_color == nullptr) return;
        const auto panel = this->ConfigLayout().ColorEditor(viewport);
        const float x = static_cast<float>(this->mouse_x_), y = static_cast<float>(this->mouse_y_);
        this->config_.color_picker.Draw(this->design_system_, this->config_controls_, panel, x, y);
        if (!Contains(panel, x, y)) return;
        this->config_.tooltip.clear();
        const auto part = ui::color_picker::HitTest(panel, x, y);
        const ui::color_picker_layout layout{panel};
        if (part == ui::color_picker::part::restore)
        {
            this->config_.tooltip = "Restore the color from when this picker opened.";
            this->config_.tooltip_anchor = layout.Original();
        }
        else if (part == ui::color_picker::part::copy)
        {
            this->config_.tooltip = "Copy this hex color. Transparent colors include the alpha value.";
            this->config_.tooltip_anchor = layout.Hex();
        }
    }
}
