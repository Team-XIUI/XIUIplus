#include "hud_runtime.hpp"
#include "config/contributor_list.hpp"
#include "config/option_layout.hpp"
#include "config/basic_settings_layout.hpp"
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
    auto hud_runtime::ConfigSoundTarget(const D3DVIEWPORT8& viewport) -> uint64_t
    {
        if (!this->visible_ || !this->editing_) return 0;
        if (const auto target = this->SettingsSearchTarget(viewport); target != 0 || this->config_.search_open) return target;
        this->NormalizeConfigSelection();
        if (this->config_.choice_input.Opened()) return this->ChoiceSoundTarget(viewport);
        if (const auto target = this->ProfileSoundTarget(viewport)) return target;
        if (this->ProfileSurface(viewport)) return 0;
        this->NormalizeConfigSelection();
        const auto x = static_cast<float>(this->mouse_x_), y = static_cast<float>(this->mouse_y_);
        const auto layout = this->ConfigLayout();
        if (!this->config_.collapsed && Contains(layout.EditModeButton(), x, y)) return 0xD300000000000002ULL;
        if (Contains(this->SettingsModeBounds(), x, y)) return 0xD300000000000001ULL;
        if (this->config_.basic && !this->config_.collapsed)
            for (size_t row = 0; row < ui::basic_settings_layout::control_count; ++row)
                if (Contains(this->BasicSettingBounds(row), x, y)) return 0xD300000000000010ULL + row;
        if (this->config_.active_color != nullptr && Contains(layout.ColorEditor(viewport), x, y))
        {
            const auto part = ui::color_picker::HitTest(layout.ColorEditor(viewport), x, y);
            return part == ui::color_picker::part::none ? 0 : 0x10000 + static_cast<uint64_t>(part);
        }
        if (!Contains(this->ConfigVisibleBounds(), x, y)) return this->MovementToolbarTarget(viewport);
        if (Contains(layout.CloseButton(), x, y)) return 1;
        if (Contains(layout.HeaderAction(), x, y)) return 3;
        if (Contains(layout.HeaderAction(1), x, y)) return 4;
        if (this->ConfigResizeEdges(x, y) != 0) return 5;
        if (const auto input = this->InputModeControlTarget(viewport); input != 0) return input;
        if (this->config_.basic) return 0;
        if (Contains(layout.SidebarToggle(), x, y)) return 6;
        if (this->config_.collapsed) return 0;
        if (Contains(this->ConfigSidebarBounds(), x, y))
            for (const auto& row : this->ConfigSidebarRows())
                if (row.available && Contains(row.bounds, x, y)) return 0x100000 + row.item;
        if (this->config_.module == nullptr) return 0;
        const auto key = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(this->config_.module)) << 32;
        if (this->IsAttribution())
            for (size_t index = 0; index < std::size(ui::attribution_views); ++index)
                if (Contains(ui::config_controls::Segment(layout.ContentTabs(), index, std::size(ui::attribution_views)), x, y))
                    return key | (0x40000 + index);
        if (this->IsInventorySettings())
            for (size_t index = 0; index < this->inventory_pages_.size(); ++index)
                if (Contains(ui::config_controls::Segment(layout.ContentTabs(), index, this->inventory_pages_.size()), x, y))
                    return (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(this->inventory_pages_[0])) << 32) | (0x42000 + index);
        const auto topics = this->ConfigTopicRows();
        if (!topics.empty())
        {
            for (size_t index = 0; index < topics.size(); ++index)
                if (Contains(ui::SettingsTopicBounds(layout.Submenu(index, topics.size()), topics[index]), x, y))
                    return key | (topics[index].subview >= 0 ? 0x31000 + topics[index].section * 16 + topics[index].subview :
                        topics[index].parent ? 0x3FF : 0x300 + topics[index].section);
        }
        else
        {
            const auto categories = this->ConfigCategories();
            for (size_t index = 0; index < categories.count; ++index)
                if (Contains(layout.Category(index, categories.count), x, y)) return key | (0x400 + index);
        }
        if (this->IsExperienceDisplay())
            if (const auto target = this->experience_display_.Target(this->experience_, layout, x, y)) return key | (0x20000 + target);
        if (this->IsXIUIContributors())
        {
            const auto member = ui::contributor_list_layout{layout, this->config_.scroll}.MemberAt(x, y);
            return member ? key | (0x1000 + this->general_.ContributorsStart() + *member) : 0;
        }
        if (const auto target = this->ElementSelectorTarget()) return target;
        for (int index = 0; index < layout.VisibleSlots(); ++index)
        {
            if (!Contains(layout.OptionRow(index), x, y)) continue;
            const auto optionIndex = this->GroupOptionIndex(static_cast<size_t>(this->config_.scroll + index));
            if (optionIndex >= this->config_.module->OptionCount()) return 0;
            const auto option = this->config_.module->Option(optionIndex);
            if (option.available && option.kind == module_option_kind::choice && option.choice_count > 1)
            {
                const auto row = layout.OptionRow(index);
                const uint64_t part = Contains(ui::config_layout::ChoicePrevious(row), x, y) ? 0 :
                    Contains(ui::config_layout::ChoiceNext(row), x, y) ? 2 : 1;
                return key | (0x50000 + optionIndex * 3 + part);
            }
            return option.available ? key | (0x1000 + optionIndex) : 0;
        }
        return 0;
    }

    auto hud_runtime::ProcessConfigClick(const D3DVIEWPORT8& viewport) -> bool
    {
        if (this->ProcessInputModeClick(viewport)) return true;
        if (this->ProcessProfileClick(viewport)) return true;
        if (!this->config_.collapsed && Contains(this->ConfigLayout().EditModeButton(), this->mouse_x_, this->mouse_y_))
        { this->ToggleLayoutEditMode(); return true; }
        if (this->config_.basic) return this->ProcessBasicSettingsClick(viewport);
        if (Contains(this->SettingsModeBounds(), this->mouse_x_, this->mouse_y_)) { this->SetBasicSettings(true); return true; }
        this->NormalizeConfigSelection();
        const float x = static_cast<float>(this->mouse_x_);
        const float y = static_cast<float>(this->mouse_y_);

        if (this->config_.active_color != nullptr)
        {
            const auto editor = this->ConfigLayout().ColorEditor(viewport);
            if (Contains(editor, x, y))
            {
                const auto part = ui::color_picker::HitTest(editor, x, y);
                if (part == ui::color_picker::part::close)
                    this->BeginCloseInteraction(interaction::close_color, ui::color_picker_layout{editor}.Close());
                else if (part == ui::color_picker::part::restore)
                {
                    this->config_.color_picker.Restore();
                    this->ApplyPickedColor();
                    this->SaveSettings();
                }
                else if (part == ui::color_picker::part::copy)
                {
                    const auto* properties = this->core_ != nullptr ? this->core_->GetProperties() : nullptr;
                    this->config_.color_picker.CopyHex(properties != nullptr ? properties->GetFinalFantasyHwnd() : nullptr);
                }
                else if (part == ui::color_picker::part::hex) this->config_.color_picker.BeginHex();
                else if (part == ui::color_picker::part::wheel || part == ui::color_picker::part::value || part == ui::color_picker::part::alpha)
                {
                    this->config_.color_picker.CancelHex();
                    this->config_.color_picker.BeginDrag(part);
                    this->interaction_ = interaction::color_pick;
                }
                this->UpdateInteraction(viewport);
                return true;
            }
            this->config_.active_color = nullptr;
        }

        const auto config = this->ConfigVisibleBounds();
        if (!Contains(config, x, y))
        {
            this->config_.active_color = nullptr;
            return this->ProcessMovementToolbarClick(viewport);
        }

        if (const auto edges = this->ConfigResizeEdges(x, y); edges != 0)
        {
            this->CloseProfileControls(); this->CloseChoiceInput(); this->config_.input_mode_menu = false;
            this->config_.active_color = nullptr; this->config_.active_slider = nullptr;
            this->interaction_ = interaction::resize_config;
            this->config_resize_edges_ = edges;
            this->config_resize_original_ = this->config_.bounds;
            this->config_resize_pointer_ = {x, y};
            return true;
        }

        if (Contains(this->ConfigLayout().SidebarToggle(), x, y))
        {
            this->ToggleConfigSidebar(viewport);
            return true;
        }

        if (Contains(this->ConfigLayout().HeaderAction(1), x, y))
        {
            this->FinishConfigInput(true, false);
            this->CloseProfileControls(); this->CloseChoiceInput(); this->ReleaseChatInput();
            this->config_.active_color = nullptr; this->config_.active_slider = nullptr;
            this->config_.collapsed = !this->config_.collapsed;
            this->ClampLayout(viewport);
            return true;
        }

        if (Contains(this->ConfigLayout().HeaderAction(), x, y))
        {
            this->config_.collapsed = false;
            this->FinishConfigInput(true, false);
            this->ReleaseChatInput();
            this->config_.module = &this->general_;
            this->config_.section = general_module::attribution_section;
            this->general_.SelectAttributionView(0);
            this->config_.active_color = nullptr; this->config_.active_slider = nullptr;
            this->config_.scroll = 0;
            return true;
        }

        const rect close = this->ConfigLayout().CloseButton();
        if (Contains(close, x, y))
        {
            this->BeginCloseInteraction(interaction::close_config, close);
            return true;
        }

        if (this->config_.collapsed) return false;

        if (this->ProcessConfigScrollbarClick()) return true;

        if (Contains(this->ConfigSidebarBounds(), x, y))
            for (const auto& row : this->ConfigSidebarRows())
                if (row.available && Contains(row.bounds, x, y))
                { this->SelectConfigSidebarItem(row.item); return true; }

        if (this->IsAttribution())
            for (size_t index = 0; index < std::size(ui::attribution_views); ++index)
                if (Contains(ui::config_controls::Segment(this->ConfigLayout().ContentTabs(), index, std::size(ui::attribution_views)), x, y))
                {
                    this->general_.SelectAttributionView(index);
                    this->config_.scroll = 0; this->config_.active_color = nullptr; this->config_.active_slider = nullptr;
                    return true;
                }

        if (this->IsInventorySettings())
            for (size_t index = 0; index < this->inventory_pages_.size(); ++index)
                if (Contains(ui::config_controls::Segment(this->ConfigLayout().ContentTabs(), index, this->inventory_pages_.size()), x, y))
                {
                    this->SelectConfigModule(this->inventory_pages_[index]);
                    return true;
                }

        const auto topics = this->ConfigTopicRows();
        if (!topics.empty())
        {
            for (size_t index = 0; index < topics.size(); ++index)
                if (Contains(ui::SettingsTopicBounds(this->ConfigLayout().Submenu(index, topics.size()), topics[index]), x, y))
                {
                    this->SelectConfigTopicRow(index);
                    return true;
                }
        }
        else
        {
            const auto categories = this->ConfigCategories();
            for (size_t index = 0; index < categories.count; ++index)
            {
                if (Contains(this->ConfigLayout().Category(index, categories.count), x, y))
                {
                    this->config_.group = categories.groups[index];
                    this->config_.active_color = nullptr;
                    this->config_.scroll = 0;
                    return true;
                }
            }
        }

        if (this->IsExperienceDisplay())
        {
            const auto previous = this->experience_display_.View();
            const auto result = this->experience_display_.Click(this->experience_, this->ConfigLayout(), x, y);
            if (result != ui::experience_display_editor::result::none)
            {
                if (previous != this->experience_display_.View()) { this->config_.scroll = 0; this->config_.active_color = nullptr; }
                if (result == ui::experience_display_editor::result::changed) this->SaveSettings();
                return true;
            }
        }

        if (this->IsXIUIContributors())
        {
            if (const auto member = ui::contributor_list_layout{this->ConfigLayout(), this->config_.scroll}.MemberAt(x, y))
                this->general_.OnOptionChanged(this->general_.ContributorsStart() + *member);
            return true;
        }

        if (this->ProcessElementSelectorClick()) return true;

        if (y < config.y + 28.0f)
            return false;

        const int visibleOptions = this->ConfigLayout().VisibleSlots();
        int visibleIndex = -1;
        for (int index = 0; index < visibleOptions; ++index)
        {
            if (Contains(this->ConfigLayout().OptionRow(index), x, y))
            {
                visibleIndex = index;
                break;
            }
        }
        if (visibleIndex < 0)
            return true;

        const size_t optionIndex = this->GroupOptionIndex(static_cast<size_t>(this->config_.scroll + visibleIndex));
        if (this->config_.module == nullptr || optionIndex >= this->config_.module->OptionCount())
            return true;
        const module_option option = this->config_.module->Option(optionIndex);
        if (!option.available)
            return true;
        this->config_.option = optionIndex;
        if (option.kind == module_option_kind::toggle_buttons)
        {
            const auto row = this->ConfigLayout().OptionRow(visibleIndex);
            for (size_t index = 0; index < option.choice_count; ++index)
                if (Contains(ui::config_layout::ToggleButton(row, index, option.choice_count), x, y) && this->config_.ActivateToggle(option, index))
                {
                    this->config_.module->OnOptionChanged(optionIndex);
                    this->SaveSettings();
                    break;
                }
            return true;
        }
        else if (option.kind == module_option_kind::toggle && option.toggle != nullptr)
        {
            this->config_.ActivateToggle(option);
            this->config_.module->OnOptionChanged(optionIndex);
        }
        else if (option.kind == module_option_kind::color_override && option.color != nullptr && option.toggle != nullptr)
        {
            const auto row = this->ConfigLayout().OptionRow(visibleIndex);
            if (Contains(ui::config_layout::OverrideSwatch(row), x, y))
            {
                *option.toggle = true;
                this->config_.active_color = option.color;
                this->config_.color_picker.Open(*option.color);
                this->config_.module->OnOptionChanged(optionIndex);
                this->SaveSettings();
                return true;
            }
            *option.toggle = !*option.toggle;
            this->config_.module->OnOptionChanged(optionIndex);
        }
        else if (option.kind == module_option_kind::color && option.color != nullptr)
        {
            this->config_.active_color = option.color;
            this->config_.color_picker.Open(*option.color);
            return true;
        }
        else if (option.kind == module_option_kind::slider && option.slider != nullptr)
        {
            this->config_.active_color = nullptr;
            const auto row = this->ConfigLayout().OptionRow(visibleIndex);
            if (Contains(ui::config_layout::SliderValue(row), x, y))
            {
                this->ReleaseChatInput();
                this->config_.slider_input.Open(option);
                this->config_.slider_module = this->config_.module;
                this->config_.slider_option = optionIndex;
                this->config_.slider_bounds = ui::config_layout::SliderValue(row);
                return true;
            }
            this->config_.active_slider = option.slider;
            this->config_.slider_minimum = option.minimum;
            this->config_.slider_maximum = option.maximum;
            this->config_.slider_step = option.step;
            this->config_.slider_row = row;
            this->interaction_ = interaction::option_slider;
            this->SetActiveSliderFromMouse();
            return true;
        }
        else if (option.kind == module_option_kind::dropdown && option.choice != nullptr && option.choice_count > 0)
        {
            this->config_.active_color = nullptr;
            this->ReleaseChatInput();
            this->config_.choice_input.Open(option, ui::config_layout::ChoiceButton(this->ConfigLayout().OptionRow(visibleIndex)));
            this->config_.choice_module = this->config_.module;
            this->config_.choice_option = optionIndex;
            return true;
        }
        else if (option.kind == module_option_kind::choice && option.choice != nullptr && option.choice_count > 0)
        {
            const int count = static_cast<int>(option.choice_count);
            const int direction = Contains(ui::config_layout::ChoicePrevious(this->ConfigLayout().OptionRow(visibleIndex)), x, y) ? -1 : 1;
            *option.choice = (std::clamp(*option.choice, 0, count - 1) + direction + count) % count;
            this->config_.active_color = nullptr;
            this->config_.module->OnOptionChanged(optionIndex);
        }
        else if (option.kind == module_option_kind::text_input && option.text != nullptr)
        {
            this->config_.active_color = nullptr;
            this->ReleaseChatInput();
            this->config_.text_input.Open(*option.text, option.text_limit);
            this->config_.text_module = this->config_.module;
            this->config_.text_option = optionIndex;
            this->config_.text_value = option.text;
            this->config_.text_bounds = ui::config_layout::TextValue(this->ConfigLayout().OptionRow(visibleIndex));
            return true;
        }
        else if (option.kind == module_option_kind::action)
            return this->ActivateConfigAction(*this->config_.module, optionIndex);
        else
            return false;

        this->SaveSettings();
        return true;
    }

    auto hud_runtime::ResolveCursorShape(const D3DVIEWPORT8& viewport) const -> cursor_shape
    {
        if (this->ActionBarsPointerOwns(static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_)))
            return this->actionbars_.PointerCursor(static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
        if (this->SettingsSearchTarget(viewport) != 0) return cursor_shape::hand;
        if (this->config_.search_open) return cursor_shape::arrow;
        if (this->InputModeControlTarget(viewport) != 0) return cursor_shape::hand;
        if (this->config_.choice_input.Opened())
            return this->config_.choice_input.IndexAt(static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_), viewport) ? cursor_shape::hand : cursor_shape::arrow;
        if (this->profile_name_.Focused())
        {
            return this->ProfileSoundTarget(viewport) != 0 ? cursor_shape::hand : cursor_shape::arrow;
        }
        if (this->ProfileSoundTarget(viewport) != 0) return cursor_shape::hand;
        if (this->profile_dropdown_) return cursor_shape::arrow;
        if (Contains(this->SettingsModeBounds(), this->mouse_x_, this->mouse_y_)) return cursor_shape::hand;
        switch (this->interaction_)
        {
        case interaction::drag_module:
        case interaction::drag_config:
            return cursor_shape::move;
        case interaction::scale_module: return cursor_shape::resize_diagonal;
        case interaction::resize_config:
            return (this->config_resize_edges_ & (ui::resize_left | ui::resize_right)) != 0
                ? ((this->config_resize_edges_ & (ui::resize_top | ui::resize_bottom)) != 0 ? cursor_shape::resize_diagonal : cursor_shape::resize_horizontal)
                : cursor_shape::resize_vertical;
        case interaction::color_pick:
        case interaction::module_toolbar:
        case interaction::option_scrollbar:
            return cursor_shape::hand;
        case interaction::option_slider:
            return cursor_shape::resize_horizontal;
        default:
            break;
        }

        const float x = static_cast<float>(this->mouse_x_);
        const float y = static_cast<float>(this->mouse_y_);
        if (const auto edges = this->ConfigResizeEdges(x, y); edges != 0)
            return (edges & (ui::resize_left | ui::resize_right)) != 0
                ? ((edges & (ui::resize_top | ui::resize_bottom)) != 0 ? cursor_shape::resize_diagonal : cursor_shape::resize_horizontal)
                : cursor_shape::resize_vertical;
        const auto previewCursor = [&] {
            if (const auto toolbar = this->MovementToolbar(viewport); toolbar && Contains(toolbar->bounds, x, y))
                return this->MovementToolbarTarget(viewport) != 0 ? cursor_shape::hand : cursor_shape::arrow;
            if (this->EditGripAt(x, y) != nullptr) return cursor_shape::resize_diagonal;
            const auto& modules = this->modules_.All();
            for (auto module = modules.rbegin(); module != modules.rend(); ++module)
                if ((*module)->HasEditableBounds() && (*module)->CanMove() && this->ModuleDisplayed(**module) && Contains(this->MovementBounds(**module), x, y))
                    return cursor_shape::move;
            return cursor_shape::arrow;
        };
        const auto layout = this->ConfigLayout();
        if (this->config_.collapsed)
        {
            if (Contains(layout.CloseButton(), x, y) || Contains(layout.HeaderAction(), x, y) || Contains(layout.HeaderAction(1), x, y) ||
                (!this->config_.basic && Contains(layout.SidebarToggle(), x, y))) return cursor_shape::hand;
            if (Contains(this->ConfigHeaderDragArea(), x, y)) return cursor_shape::move;
            if (Contains(this->ConfigVisibleBounds(), x, y)) return cursor_shape::arrow;
            return previewCursor();
        }
        if (this->config_.active_color != nullptr)
        {
            const auto editor = layout.ColorEditor(viewport);
            if (Contains(editor, x, y))
                return ui::color_picker::HitTest(editor, x, y) == ui::color_picker::part::none ? cursor_shape::arrow : cursor_shape::hand;
        }

        if (Contains(layout.EditModeButton(), x, y)) return cursor_shape::hand;
        if (this->config_.basic)
        {
            if (Contains(this->SettingsModeBounds(), x, y) || Contains(layout.CloseButton(), x, y) ||
                Contains(layout.HeaderAction(), x, y) || Contains(layout.HeaderAction(1), x, y)) return cursor_shape::hand;
            for (size_t index = 0; index < ui::basic_settings_layout::control_count; ++index)
            {
                const auto row = this->BasicSettingBounds(index);
                if (Contains(row, x, y)) return index == 2 && !Contains(ui::basic_settings_layout{this->config_.bounds}.ScaleValue(), x, y)
                    ? cursor_shape::resize_horizontal : cursor_shape::hand;
            }
            if (Contains(this->ConfigHeaderDragArea(), x, y)) return cursor_shape::move;
            return Contains(this->ConfigVisibleBounds(), x, y) ? cursor_shape::arrow : previewCursor();
        }

        const auto scrollbar = this->ConfigScrollbar();
        if (scrollbar.Visible() && Contains(scrollbar.track, x, y)) return cursor_shape::hand;
        const auto sidebarScrollbar = this->ConfigSidebarScrollbar();
        if (sidebarScrollbar.Visible() && Contains(sidebarScrollbar.track, x, y)) return cursor_shape::hand;
        const rect close = layout.CloseButton();
        if (Contains(close, x, y) || Contains(layout.HeaderAction(), x, y) || Contains(layout.HeaderAction(1), x, y) || Contains(layout.SidebarToggle(), x, y))
            return cursor_shape::hand;

        if (Contains(this->ConfigSidebarBounds(), x, y))
            for (const auto& row : this->ConfigSidebarRows())
                if (row.available && Contains(row.bounds, x, y)) return cursor_shape::hand;
        if (this->HasConfigSubmenu())
        {
            const auto topics = this->ConfigTopicRows();
            for (size_t index = 0; index < topics.size(); ++index)
                if (Contains(ui::SettingsTopicBounds(layout.Submenu(index, topics.size()), topics[index]), x, y)) return cursor_shape::hand;
        }
        else
        {
            const auto categories = this->ConfigCategories();
            for (size_t index = 0; index < categories.count; ++index)
                if (Contains(layout.Category(index, categories.count), x, y)) return cursor_shape::hand;
        }

        if (this->IsInventorySettings())
            for (size_t index = 0; index < this->inventory_pages_.size(); ++index)
                if (Contains(ui::config_controls::Segment(layout.ContentTabs(), index, this->inventory_pages_.size()), x, y)) return cursor_shape::hand;
        if (this->IsAttribution())
            for (size_t index = 0; index < std::size(ui::attribution_views); ++index)
                if (Contains(ui::config_controls::Segment(layout.ContentTabs(), index, std::size(ui::attribution_views)), x, y)) return cursor_shape::hand;
        if (this->IsExperienceDisplay() && this->experience_display_.Target(this->experience_, layout, x, y) != 0) return cursor_shape::hand;
        if (this->IsXIUIContributors())
            if (ui::contributor_list_layout{layout, this->config_.scroll}.MemberAt(x, y)) return cursor_shape::hand;

        if (const auto selector = this->ElementSelector(); selector.Enabled() && this->ElementSelectorLayout(selector, layout).IndexAt(x, y))
            return cursor_shape::hand;

        if (this->config_.module != nullptr && !this->IsXIUIContributors())
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
                const auto row = layout.OptionRow(visibleIndex);
                if (Contains(row, x, y))
                    return !option.available ? cursor_shape::arrow : (option.kind == module_option_kind::slider && !Contains(ui::config_layout::SliderValue(row), x, y) ? cursor_shape::resize_horizontal : cursor_shape::hand);
            }
        }

        if (Contains(this->ConfigHeaderDragArea(), x, y))
            return cursor_shape::move;
        if (Contains(this->ConfigVisibleBounds(), x, y))
            return cursor_shape::arrow;

        return previewCursor();
    }

    auto hud_runtime::ApplyPickedColor(void) -> void
    {
        if (this->config_.active_color == nullptr || *this->config_.active_color == this->config_.color_picker.Color()) return;
        *this->config_.active_color = this->config_.color_picker.Color();
        if (this->config_.module != nullptr)
            this->config_.module->OnOptionChanged(this->config_.option);
    }

    auto hud_runtime::SetActiveSliderFromMouse(void) -> void
    {
        if (this->config_.active_slider == nullptr)
            return;
        const rect track = this->config_.basic ? ui::basic_settings_layout{this->config_.bounds}.ScaleTrack() :
            ui::config_layout::SliderTrack(this->config_.slider_row);
        const float ratio = std::clamp((static_cast<float>(this->mouse_x_) - track.x) / std::max(1.0f, track.width), 0.0f, 1.0f);
        float value = this->config_.slider_minimum + (this->config_.slider_maximum - this->config_.slider_minimum) * ratio;
        *this->config_.active_slider = ui::slider_value_input::Normalize(value, this->config_.slider_minimum, this->config_.slider_maximum, this->config_.slider_step);
        if (this->config_.module != nullptr)
            this->config_.module->OnOptionChanged(this->config_.option);
    }
}
