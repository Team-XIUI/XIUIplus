#include "hud_runtime.hpp"

#include "config/attributions.hpp"
#include "config/contributor_list.hpp"
#include "config/config_window_resize.hpp"
#include "input/controller_movement_layout.hpp"

#include <cmath>

namespace
{
    using action = xiui::controller_action;
    using region = xiui::config_controller_region;

    auto Direction(const action value) -> int
    { return value == action::left || value == action::up || value == action::previous_tab ? -1 : 1; }

    auto Directional(const action value) -> bool
    { return value == action::up || value == action::down || value == action::left || value == action::right; }
}

namespace xiui
{
    auto hud_runtime::ResetControllerConfigFocus(void) -> void
    {
        this->FinishControllerMove(false);
        this->FinishConfigResize(false);
        this->controller_config_ = {};
        this->input_mode_menu_ = false;
    }

    auto hud_runtime::BeginConfigResize(const bool keyboard) -> void
    {
        this->FinishConfigInput(true, false);
        this->CloseProfileControls(); this->CloseChoiceInput(); this->input_mode_menu_ = false;
        this->active_color_ = nullptr; this->active_slider_ = nullptr;
        this->config_collapsed_ = false;
        auto& focus = this->controller_config_;
        if (this->device_ != nullptr) this->device_->GetViewport(&focus.viewport);
        focus.active = true; focus.adjusting = false; focus.resizing = true; focus.fine = false;
        focus.resize_keyboard = keyboard;
        focus.resize_original = this->config_bounds_;
        focus.region = region::utilities; focus.id = "resize";
        this->controller_surface_ = controller_surface::config;
    }

    auto hud_runtime::FinishConfigResize(const bool apply) -> void
    {
        auto& focus = this->controller_config_;
        if (!focus.resizing) return;
        if (!apply) this->config_bounds_ = focus.resize_original;
        focus.resizing = false; focus.resize_keyboard = false; focus.fine = false;
        this->ClampOptionScroll();
        if (apply) this->SaveSettings();
    }

    auto hud_runtime::HandleConfigResize(const controller_action value, const bool repeated) -> bool
    {
        auto& focus = this->controller_config_;
        if (value == action::cancel || value == action::confirm)
        {
            if (!repeated) this->FinishConfigResize(value == action::confirm);
            return true;
        }
        if (value == action::actions && !repeated) { focus.fine = !focus.fine; return true; }
        if (!Directional(value)) return true;
        const float step = focus.fine ? 1.0f : 10.0f;
        const float dx = value == action::left ? -step : value == action::right ? step : 0;
        const float dy = value == action::up ? -step : value == action::down ? step : 0;
        this->ApplyConfigResize(this->config_bounds_, ui::resize_right | ui::resize_bottom, dx, dy, focus.viewport);
        return true;
    }

    auto hud_runtime::ControllerConfigItems(const config_controller_region area) -> std::vector<config_controller_item>
    {
        std::vector<config_controller_item> items;
        const auto layout = this->ConfigLayout();
        const auto add = [&](std::string id, const rect& bounds, const size_t index, const char* help) {
            items.push_back({std::move(id), bounds, index, help != nullptr ? help : ""});
        };
        if (area == region::utilities)
        {
            add("input", layout.InputMode(), 0, "Input mode: Auto, Keyboard & Mouse, or Controller. Both devices remain available.");
            add("attribution", layout.HeaderAction(), 1, "Attribution");
            add("collapse", layout.HeaderAction(1), 2, this->config_collapsed_ ? "Expand configuration" : "Collapse to title bar");
            add("close", layout.CloseButton(), 3, "Close configuration and return to the game");
            if (!this->config_collapsed_)
                add("resize", layout.ResizeGrip(), 4, "Resize configuration. D-pad changes width and height; Actions switches fine steps. Confirm saves; Cancel restores.");
            add("sidebar", layout.SidebarToggle(), 5, this->config_sidebar_collapsed_ ? "Expand sidebar" : "Collapse sidebar to icons");
            add("search", this->SettingsSearchBounds(), 6, "Search settings (Ctrl+F). Confirm opens; use a keyboard to type, directions for results and Actions to clear.");
            return items;
        }
        if (area == region::movement)
        {
            const auto modules = this->ControllerMovableModules();
            const controller_movement_layout bounds{this->config_bounds_, this->controller_config_.viewport, modules.size()};
            for (size_t index = 0; index < modules.size(); ++index)
                add(modules[index]->Id(), bounds.Row(static_cast<int>(index) - this->controller_config_.movement_scroll), index,
                    "Confirm enters Move. D-pad nudges; Actions switches fine movement. Confirm saves, Cancel restores.");
            return items;
        }
        if (this->config_collapsed_) return items;
        if (area == region::categories)
        {
            const auto pages = this->VisibleConfigPages();
            for (size_t index = 0; index < pages.size(); ++index)
                add(pages[index].id, layout.ModuleTab(index), index, pages[index].name);
        }
        else if (area == region::sections)
        {
            const auto page = this->ActiveConfigPage();
            if (page.SectionCount() > 1)
                for (size_t index = 0; index < page.SectionCount(); ++index)
                    add(page.SectionName(index), layout.Section(index, page.SectionCount()), index, page.SectionName(index));
        }
        else if (area == region::topics)
        {
            if (this->active_module_ == nullptr) return items;
            const auto topics = this->active_module_->ConfigSections();
            for (size_t index = 0; index < topics.size(); ++index)
                add(topics[index].name, layout.Submenu(index, topics.size()), index, topics[index].description);
            if (topics.empty())
            {
                static constexpr const char* names[]{"General", "Appearance", "Effects", "Colors", "Text"};
                const auto categories = this->ConfigCategories();
                for (size_t index = 0; index < categories.count; ++index)
                    add(names[static_cast<size_t>(categories.groups[index])], layout.Category(index, categories.count), index, "Settings category");
            }
        }
        else if (area == region::content_tabs)
        {
            if (this->IsInventorySettings())
                for (size_t index = 0; index < this->inventory_pages_.size(); ++index)
                    add(this->inventory_pages_[index]->Id(), ui::config_controls::Segment(layout.ContentTabs(), index, this->inventory_pages_.size()), index, this->inventory_pages_[index]->DisplayName());
            if (this->IsGeneralAppearance())
                for (size_t index = 0; index < std::size(general_module::appearance_tabs); ++index)
                    add(general_module::appearance_tabs[index], ui::config_controls::Segment(layout.ContentTabs(), index, std::size(general_module::appearance_tabs)), index, general_module::appearance_tabs[index]);
            if (this->IsAttribution())
                for (size_t index = 0; index < std::size(ui::attribution_views); ++index)
                    add(ui::attribution_views[index], ui::config_controls::Segment(layout.ContentTabs(), index, std::size(ui::attribution_views)), index, ui::attribution_views[index]);
            if (this->IsExperienceDisplay())
            {
                static constexpr const char* names[]{"Bar info", "Readouts", "Layout"};
                for (size_t index = 0; index < std::size(names); ++index)
                    add(names[index], ui::config_controls::Segment(layout.ContentTabs(), index, std::size(names)), index, names[index]);
            }
        }
        else if (area == region::elements)
        {
            const auto selector = this->ElementSelector();
            if (!selector.Enabled()) return items;
            const auto bounds = this->ElementSelectorLayout(selector, layout);
            for (size_t index = 0; index < selector.count; ++index)
                add(selector.labels[index], bounds.tabs[index], index, selector.element.description);
        }
        else if (area == region::options && this->active_module_ != nullptr)
        {
            if (this->IsExperienceDisplay() && this->experience_display_.View() == experience_display_view::readouts)
                return this->experience_display_.ControllerItems(this->experience_, layout);
            if (this->IsXIUIContributors())
            {
                const ui::contributor_list_layout bounds{layout, this->option_scroll_};
                for (size_t index = 0; index < ui::xiui_contributors.size(); ++index)
                {
                    const auto option = this->general_.Option(this->general_.ContributorsStart() + index);
                    if (option.available) add(option.label, bounds.Member(index), this->general_.ContributorsStart() + index, option.description);
                }
                return items;
            }
            const auto slots = this->GroupOptionSlots();
            for (size_t filtered = 0; filtered < slots.size(); ++filtered)
            {
                const size_t index = slots[filtered];
                if (index >= this->active_module_->OptionCount()) continue;
                const auto option = this->active_module_->Option(index);
                if (!option.available) continue;
                const auto row = layout.OptionRow(static_cast<int>(filtered) - this->option_scroll_);
                add(std::string{this->active_module_->Id()} + "/" + std::to_string(this->active_module_->ConfigView()) + "/" + std::to_string(index), row, index, option.description);
            }
        }
        else if (area == region::profiles)
        {
            if (this->profiles_.Ready())
            {
                add("profile", layout.ProfileSelector(), 0, "Select a profile");
                add("add", layout.ProfileAdd(), 1, "Create a profile. Use a keyboard to type its name.");
            }
            add("actions", layout.ProfileActionsButton(), 2, "Profile actions and Reset to defaults for the entire XIUI configuration");
        }
        return items;
    }

    auto hud_runtime::RefreshControllerConfigFocus(void) -> void
    {
        auto& focus = this->controller_config_;
        auto items = this->ControllerConfigItems(focus.region);
        focus.Resolve(items);
        for (size_t attempt = 0; items.empty() && attempt < static_cast<size_t>(region::count); ++attempt)
        {
            focus.region = static_cast<region>((static_cast<size_t>(focus.region) + 1) % static_cast<size_t>(region::count));
            items = this->ControllerConfigItems(focus.region);
            focus.Resolve(items);
        }
        if (items.empty()) return;
        if (focus.region == region::movement)
        {
            const controller_movement_layout bounds{this->config_bounds_, focus.viewport, items.size()};
            if (focus.item < static_cast<size_t>(focus.movement_scroll)) focus.movement_scroll = static_cast<int>(focus.item);
            if (focus.item >= static_cast<size_t>(focus.movement_scroll + bounds.rows)) focus.movement_scroll = static_cast<int>(focus.item) - bounds.rows + 1;
            focus.movement_scroll = std::clamp(focus.movement_scroll, 0, std::max(0, static_cast<int>(items.size()) - bounds.rows));
            return;
        }
        if (focus.region != region::options) return;
        const auto& item = items[focus.item];
        if (this->IsExperienceDisplay() && this->experience_display_.View() == experience_display_view::readouts)
        {
            this->experience_display_.RevealControllerTarget(static_cast<uint32_t>(item.index), this->experience_, this->ConfigLayout());
            return;
        }
        if (this->IsXIUIContributors())
        {
            const auto area = this->ConfigLayout().ContentArea();
            if (item.bounds.y < area.y) this->option_scroll_ -= static_cast<int>(std::ceil((area.y - item.bounds.y) / 22));
            if (item.bounds.y + item.bounds.height > area.y + area.height)
                this->option_scroll_ += static_cast<int>(std::ceil((item.bounds.y + item.bounds.height - area.y - area.height) / 22));
        }
        else
        {
            const auto slots = this->GroupOptionSlots();
            const auto filtered = static_cast<size_t>(std::find(slots.begin(), slots.end(), item.index) - slots.begin());
            auto layout = this->ConfigLayout();
            if (static_cast<int>(filtered) < this->option_scroll_) this->option_scroll_ = layout.FirstSlotForRow(layout.RowForSlot(static_cast<int>(filtered)));
            while (static_cast<int>(filtered) >= this->option_scroll_ + this->ConfigLayout().VisibleSlots())
                this->option_scroll_ += layout.Columns();
        }
        this->ClampOptionScroll();
    }

    auto hud_runtime::ActivateControllerConfigItem(const config_controller_item& item, const controller_action value, const bool repeated) -> bool
    {
        const auto area = this->controller_config_.region;
        const bool confirm = value == action::confirm && !repeated;
        if (area == region::options)
        {
            if (this->IsExperienceDisplay() && this->experience_display_.View() == experience_display_view::readouts)
            {
                if (confirm && this->experience_display_.Activate(static_cast<uint32_t>(item.index), this->experience_, this->ConfigLayout()) == ui::experience_display_editor::result::changed)
                    this->SaveSettings();
                return true;
            }
            if (this->active_module_ == nullptr || item.index >= this->active_module_->OptionCount() || !this->MatchesConfigOption(item.index)) return true;
            const auto option = this->active_module_->Option(item.index);
            if (!option.available) return true;
            this->active_option_index_ = item.index;
            if (option.kind == module_option_kind::slider && option.slider != nullptr)
            {
                if (value == action::actions && !repeated)
                {
                    this->slider_input_.Open(option); this->slider_input_module_ = this->active_module_;
                    this->slider_input_option_index_ = item.index; this->slider_input_bounds_ = ui::config_layout::SliderValue(item.bounds);
                    this->controller_config_.adjusting = false;
                }
                else if (confirm) this->controller_config_.adjusting = !this->controller_config_.adjusting;
                else if (this->controller_config_.adjusting && (value == action::left || value == action::right))
                {
                    const float amount = option.step > 0 ? option.step : (option.maximum - option.minimum) / 100;
                    const float next = ui::slider_value_input::Normalize(*option.slider + Direction(value) * amount, option.minimum, option.maximum, option.step);
                    if (next != *option.slider) { *option.slider = next; this->active_module_->OnOptionChanged(item.index); this->SaveSettings(); }
                }
                return true;
            }
            if (!confirm && !(value == action::actions && option.kind == module_option_kind::color_override)) return true;
            if (repeated) return true;
            if (option.kind == module_option_kind::toggle && option.toggle != nullptr) *option.toggle = !*option.toggle;
            else if (option.kind == module_option_kind::color_override && option.toggle != nullptr && option.color != nullptr)
            {
                if (value == action::actions) *option.toggle = !*option.toggle;
                else { *option.toggle = true; this->active_color_ = option.color; this->color_picker_.Open(*option.color); this->controller_config_.modal_item = 0; }
            }
            else if (option.kind == module_option_kind::color && option.color != nullptr)
            { this->active_color_ = option.color; this->color_picker_.Open(*option.color); this->controller_config_.modal_item = 0; return true; }
            else if ((option.kind == module_option_kind::choice || option.kind == module_option_kind::dropdown) && option.choice != nullptr && option.choice_count != 0)
            {
                this->choice_input_.Open(option, ui::config_layout::ChoiceButton(item.bounds));
                this->choice_input_module_ = this->active_module_; this->choice_input_option_index_ = item.index; return true;
            }
            else if (option.kind == module_option_kind::text_input && option.text != nullptr)
            {
                this->option_text_input_.Open(*option.text, option.text_limit); this->text_input_module_ = this->active_module_;
                this->text_input_option_index_ = item.index; this->text_input_value_ = option.text;
                this->text_input_bounds_ = ui::config_layout::TextValue(item.bounds); return true;
            }
            else if (option.kind == module_option_kind::action) return this->ActivateConfigAction(*this->active_module_, item.index);
            else return true;
            this->active_module_->OnOptionChanged(item.index); this->SaveSettings(); return true;
        }
        if (!confirm) return true;
        this->FinishConfigInput(true, false);
        this->controller_config_.adjusting = false;
        if (area == region::categories)
        {
            const auto pages = this->VisibleConfigPages();
            if (item.index >= pages.size()) return true;
            this->SelectConfigModule(pages[item.index].SectionModule(0), 0);
        }
        else if (area == region::sections)
        {
            const auto page = this->ActiveConfigPage();
            if (item.index < page.SectionCount()) this->SelectConfigModule(page.SectionModule(item.index), page.SectionView(item.index));
        }
        else if (area == region::topics)
        {
            if (this->HasConfigSubmenu()) this->active_config_section_ = item.index;
            else
            {
                const auto categories = this->ConfigCategories();
                if (item.index < categories.count) this->active_option_group_ = categories.groups[item.index];
            }
            this->option_scroll_ = 0; this->active_color_ = nullptr;
        }
        else if (area == region::content_tabs)
        {
            if (this->IsInventorySettings()) this->SelectConfigModule(this->inventory_pages_[item.index]);
            else if (this->IsGeneralAppearance()) this->general_.SelectAppearanceTab(item.index);
            else if (this->IsAttribution()) this->general_.SelectAttributionView(item.index);
            else if (this->IsExperienceDisplay()) this->experience_display_.SelectView(static_cast<experience_display_view>(item.index));
            this->option_scroll_ = 0; this->active_color_ = nullptr;
        }
        else if (area == region::elements)
        {
            const auto selector = this->ElementSelector();
            if (!selector.Enabled() || item.index >= selector.count) return true;
            if (selector.combined)
            {
                *selector.scope.choice = item.index == 0 ? 0 : 1;
                this->active_module_->OnOptionChanged(selector.scope_index);
            }
            if (!selector.combined || item.index != 0)
            {
                *selector.element.choice = static_cast<int>(item.index - (selector.combined ? 1 : 0));
                this->active_module_->OnOptionChanged(selector.option_index);
            }
            this->option_scroll_ = 0; this->SaveSettings();
        }
        else if (area == region::profiles)
        {
            if (item.index == 0)
            {
                this->CloseProfileControls(); this->profile_dropdown_ = true; this->profile_keyboard_selection_ = this->profiles_.Selected();
                this->profile_scroll_ = std::clamp(static_cast<int>(this->profile_keyboard_selection_) - 3, 0, std::max(0, static_cast<int>(this->profiles_.Entries().size()) - 7));
            }
            else if (item.index == 1) { this->OpenProfileDialog(profile_dialog::create); this->controller_config_.modal_item = 0; }
            else if (item.index == 2)
            {
                this->CloseProfileControls(); this->profile_actions_ = true; this->profile_keyboard_selection_ = 0;
                if (!this->ProfileActionAvailable(0)) this->MoveProfileAction(1);
            }
        }
        else if (area == region::movement)
        {
            const auto modules = this->ControllerMovableModules();
            const auto found = std::find_if(modules.begin(), modules.end(), [&](const auto* module) { return item.id == module->Id(); });
            if (found == modules.end()) return true;
            this->FocusModuleConfig(**found);
            auto& focus = this->controller_config_;
            focus.moving_id = (*found)->Id(); focus.move_original = focus.move_desired = (*found)->State().bounds;
            focus.move_collapsed = this->config_collapsed_; focus.moving = true; focus.fine = false;
            this->config_collapsed_ = true;
        }
        else if (area == region::utilities)
        {
            if (item.index == 0) { this->input_mode_menu_ = true; this->input_mode_selection_ = this->general_.InputMode(); }
            else if (item.index == 1)
            { this->config_collapsed_ = false; this->SelectConfigModule(&this->general_); this->active_config_section_ = general_module::attribution_section; }
            else if (item.index == 2) { this->config_collapsed_ = !this->config_collapsed_; this->CloseProfileControls(); }
            else if (item.index == 4) this->BeginConfigResize();
            else if (item.index == 5) this->ToggleConfigSidebar(this->controller_config_.viewport);
            else if (item.index == 6) this->BeginSettingsSearch();
            else this->ExitEdit();
        }
        return true;
    }

    auto hud_runtime::HandleControllerConfig(const controller_action value, const bool repeated) -> bool
    {
        if (!this->visible_ || !this->editing_) return false;
        if (repeated && !Directional(value)) return true;
        if (this->settings_search_open_) return this->HandleSettingsSearchController(value, repeated);
        auto& focus = this->controller_config_;
        focus.active = true;
        if (focus.moving) return this->HandleControllerMove(value, repeated);
        if (focus.resizing) return this->HandleConfigResize(value, repeated);
        this->NormalizeConfigSelection();
        if (value == action::confirm) this->ui_sounds_.Select(::GetTickCount64());
        const auto modal = this->profile_name_.Focused() ? "profile/" + std::to_string(static_cast<int>(this->profile_dialog_)) :
            this->active_color_ != nullptr ? std::string{"color"} : std::string{};
        if (focus.modal_id != modal) { focus.modal_id = modal; focus.modal_item = 0; }
        if (this->input_mode_menu_)
        {
            if (value == action::cancel) this->input_mode_menu_ = false;
            else if (value == action::up || value == action::down)
            {
                const auto previous = this->input_mode_selection_;
                this->input_mode_selection_ = static_cast<size_t>(std::clamp(static_cast<int>(this->input_mode_selection_) + Direction(value), 0, 2));
                if (previous != this->input_mode_selection_) this->ui_sounds_.Play(ui::native_ui_sound::hover);
            }
            else if (value == action::confirm) { this->general_.SetInputMode(static_cast<int>(this->input_mode_selection_)); this->SaveSettings(); this->input_mode_menu_ = false; }
            return true;
        }
        if (this->choice_input_.Opened())
        {
            if (value == action::cancel) this->CloseChoiceInput();
            else if (Directional(value))
            {
                const auto previous = this->choice_input_.Highlighted(); this->choice_input_.Move(Direction(value));
                if (previous != this->choice_input_.Highlighted()) this->ui_sounds_.Play(ui::native_ui_sound::hover);
            }
            else if (value == action::confirm) this->SelectChoiceInput(this->choice_input_.Highlighted());
            else if (value == action::actions) this->PreviewChoiceInput(this->choice_input_.Highlighted());
            return true;
        }
        if (this->slider_input_.Focused() || this->option_text_input_.Focused())
        {
            if (value == action::cancel) this->FinishConfigInput(false);
            else if (value == action::confirm) this->FinishConfigInput(true);
            return true;
        }
        if (this->profile_name_.Focused())
        {
            const bool share = this->profile_dialog_ == profile_dialog::share;
            const bool importing = this->profile_dialog_ == profile_dialog::import_profile;
            const bool deleting = this->profile_dialog_ == profile_dialog::delete_profile;
            const bool confirming = this->ConfirmingAction();
            if (value == action::cancel) this->CloseProfileControls();
            else if (Directional(value))
            {
                if (share && (value == action::up || value == action::down)) this->profile_key_view_.Scroll(Direction(value));
                else
                {
                    const auto previous = focus.modal_item;
                    focus.modal_item = static_cast<uint8_t>(std::clamp(static_cast<int>(focus.modal_item) + Direction(value), 0, importing ? 3 : share || deleting || confirming ? 1 : 2));
                    if (previous != focus.modal_item) this->ui_sounds_.Play(ui::native_ui_sound::hover);
                }
            }
            else if (value == action::confirm)
            {
                if (focus.modal_item == 0) this->CloseProfileControls();
                else if (share) this->CopyProfileKey();
                else if (deleting) { if (focus.modal_item == 1) this->DeleteProfile(); }
                else if (confirming) { if (focus.modal_item == 1) this->ApplyConfirmedAction(); }
                else if (importing && focus.modal_item == 2) this->PasteProfileKey();
                else if (focus.modal_item == 1)
                {
                    if (this->profile_dialog_ == profile_dialog::rename) this->RenameProfile();
                    else this->CreateProfile();
                }
                else this->profile_name_.SelectAll();
            }
            if (deleting || confirming) this->profile_keyboard_selection_ = focus.modal_item;
            return true;
        }
        if (this->profile_actions_ || this->profile_dropdown_)
        {
            const bool actions = this->profile_actions_;
            const auto count = actions ? std::size(ui::profile_actions) : this->profiles_.Entries().size();
            if (value == action::cancel) this->CloseProfileControls();
            else if (Directional(value) && count != 0)
            {
                const auto previous = this->profile_keyboard_selection_;
                if (actions) this->MoveProfileAction(Direction(value));
                else this->profile_keyboard_selection_ = static_cast<size_t>(std::clamp(static_cast<int>(this->profile_keyboard_selection_) + Direction(value), 0, static_cast<int>(count) - 1));
                if (previous != this->profile_keyboard_selection_) this->ui_sounds_.Play(ui::native_ui_sound::hover);
                if (!actions)
                {
                    if (this->profile_keyboard_selection_ < static_cast<size_t>(this->profile_scroll_)) this->profile_scroll_ = static_cast<int>(this->profile_keyboard_selection_);
                    if (this->profile_keyboard_selection_ >= static_cast<size_t>(this->profile_scroll_ + 7)) this->profile_scroll_ = static_cast<int>(this->profile_keyboard_selection_) - 6;
                }
            }
            else if (value == action::confirm)
            {
                if (actions) { this->OpenProfileAction(this->profile_keyboard_selection_); focus.modal_item = 0; }
                else this->SelectProfile(this->profile_keyboard_selection_);
            }
            return true;
        }
        if (this->active_color_ != nullptr)
        {
            if (value == action::cancel) { this->active_color_ = nullptr; return true; }
            if (value == action::confirm)
            {
                if (focus.modal_item == 4) { this->color_picker_.Restore(); this->ApplyPickedColor(); this->SaveSettings(); }
                else if (focus.modal_item == 5)
                {
                    const auto* properties = this->core_ != nullptr ? this->core_->GetProperties() : nullptr;
                    this->color_picker_.CopyHex(properties != nullptr ? properties->GetFinalFantasyHwnd() : nullptr);
                }
                else this->active_color_ = nullptr;
                return true;
            }
            if (value == action::up || value == action::down)
            {
                const auto previous = focus.modal_item;
                focus.modal_item = static_cast<uint8_t>(std::clamp(static_cast<int>(focus.modal_item) + Direction(value), 0, 6));
                if (previous != focus.modal_item) this->ui_sounds_.Play(ui::native_ui_sound::hover);
            }
            if ((value == action::left || value == action::right) && focus.modal_item < 4)
            {
                const auto color = this->color_picker_.Color();
                const auto shift = static_cast<uint8_t>(focus.modal_item == 3 ? 24 : 16 - focus.modal_item * 8);
                const int component = static_cast<int>((color >> shift) & 255);
                const auto next = (color & ~(0xFFu << shift)) | (static_cast<uint32_t>(std::clamp(component + Direction(value), 0, 255)) << shift);
                this->color_picker_.SetColor(next); this->ApplyPickedColor(); this->SaveSettings();
            }
            else if (value == action::actions) { this->color_picker_.Restore(); this->ApplyPickedColor(); this->SaveSettings(); }
            return true;
        }
        this->RefreshControllerConfigFocus();
        if (value == action::cancel)
        {
            if (focus.adjusting) focus.adjusting = false;
            else this->ExitEdit();
            return true;
        }
        if (value == action::next_tab || value == action::previous_tab || value == action::cycle_window || value == action::focus)
        {
            focus.adjusting = false;
            focus.remembered[static_cast<size_t>(focus.region)] = focus.id;
            for (size_t attempt = 0; attempt < static_cast<size_t>(region::count); ++attempt)
            {
                focus.region = static_cast<region>((static_cast<int>(focus.region) + Direction(value) + static_cast<int>(region::count)) % static_cast<int>(region::count));
                const auto items = this->ControllerConfigItems(focus.region);
                focus.id = focus.remembered[static_cast<size_t>(focus.region)]; focus.item = 0; focus.Resolve(items);
                if (!items.empty()) break;
            }
            this->RefreshControllerConfigFocus();
            this->ui_sounds_.Play(ui::native_ui_sound::hover); return true;
        }
        auto items = this->ControllerConfigItems(focus.region);
        if (items.empty()) return true;
        if (Directional(value) && !focus.adjusting)
        {
            const bool readouts = this->IsExperienceDisplay() && this->experience_display_.View() == experience_display_view::readouts;
            const bool grid = focus.region == region::options && !this->IsXIUIContributors() && !readouts;
            const bool moved = grid ? focus.MoveGrid(items, value == action::left ? -1 : value == action::right ? 1 : 0,
                value == action::up ? -1 : value == action::down ? 1 : 0) : focus.Move(items, Direction(value));
            if (moved)
                this->ui_sounds_.Play(ui::native_ui_sound::hover);
            this->RefreshControllerConfigFocus();
            return true;
        }
        const auto item = items[focus.item];
        this->ActivateControllerConfigItem(item, value, repeated);
        this->RefreshControllerConfigFocus();
        return true;
    }

    auto hud_runtime::InputModeControlTarget(const D3DVIEWPORT8& viewport) const -> uint64_t
    {
        UNREFERENCED_PARAMETER(viewport);
        if (!this->visible_ || !this->editing_) return 0;
        const auto layout = this->ConfigLayout();
        if (Contains(layout.InputMode(), static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_))) return 0xD100000000000001ULL;
        if (this->input_mode_menu_)
            for (size_t index = 0; index < 3; ++index)
                if (Contains(layout.InputModeRow(index), static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_))) return 0xD100000000000010ULL + index;
        return 0;
    }

    auto hud_runtime::ProcessInputModeClick(const D3DVIEWPORT8& viewport) -> bool
    {
        const auto target = this->InputModeControlTarget(viewport);
        if (target == 0xD100000000000001ULL)
        {
            this->FinishConfigInput(true, false); this->CloseProfileControls(); this->CloseChoiceInput();
            this->active_color_ = nullptr; this->input_mode_menu_ = !this->input_mode_menu_;
            this->input_mode_selection_ = this->general_.InputMode(); return true;
        }
        if (!this->input_mode_menu_) return false;
        if (target >= 0xD100000000000010ULL && target <= 0xD100000000000012ULL)
        { this->general_.SetInputMode(static_cast<int>(target - 0xD100000000000010ULL)); this->SaveSettings(); }
        this->input_mode_menu_ = false;
        return true;
    }

    auto hud_runtime::DrawInputModeControl(const D3DVIEWPORT8& viewport) -> void
    {
        const auto layout = this->ConfigLayout();
        const auto target = this->InputModeControlTarget(viewport);
        const int mode = this->general_.InputMode();
        const char* label = mode == 0 ? (this->ControllerPresented() ? "Auto: Controller" : "Auto: Keyboard") : general_module::input_modes[mode];
        this->config_controls_.IconButton(layout.InputMode(), this->ControllerPresented() ? ui::icon_glyph::controller : ui::icon_glyph::keyboard,
            this->input_mode_menu_ || target == 0xD100000000000001ULL);
        if (target == 0xD100000000000001ULL && !this->input_mode_menu_)
        {
            this->config_tooltip_ = std::string{"Input mode: "} + label;
            this->config_tooltip_anchor_ = layout.InputMode();
        }
        if (!this->input_mode_menu_) return;
        this->config_controls_.Panel(layout.InputModeMenu());
        for (size_t index = 0; index < 3; ++index)
            this->config_controls_.NavigationItem(layout.InputModeRow(index), general_module::input_modes[index], static_cast<size_t>(mode) == index,
                target == 0xD100000000000010ULL + index || (this->controller_config_.active && this->input_mode_selection_ == index));
    }

    auto hud_runtime::DrawControllerInputStatus(void) -> void
    {
        if (this->active_module_ != &this->general_ || this->active_config_section_ != general_module::input_section) return;
        auto area = this->ConfigLayout().ContentArea();
        auto style = this->design_system_.GetTheme().caption_text;
        style.scale = 0.7f; style.alignment = ui::text_alignment::left;
        const float y = std::max(area.y + 224, area.y + area.height - 86);
        const auto& status = this->general_.ControllerStatus();
        const char* readiness = status.enumeration_disabled ? "Runtime ready. Ashita controller enumeration is disabled; check your launch configuration." :
            status.configuration_known && !status.gamepad_enabled ? "Runtime ready. The gamepad is disabled in FFXI Config." :
            status.connection_known && !status.connected ? "Runtime ready. Connect a controller to test navigation." :
            !status.bindings_known ? "Runtime ready. Waiting for a verified controller mapping; use /xiui focus config to enter." :
            "XIUI controller runtime ready. Enter with /xiui focus config.";
        this->config_controls_.Text(readiness, {area.x + 4, y, area.width - 8, 20}, style);
        this->config_controls_.Text("Change region: Next / Previous tab. Navigate: D-pad. Activate: Confirm. Back: Cancel.", {area.x + 4, y + 22, area.width - 8, 20}, style);
        this->config_controls_.Text("Text and profile names accept keyboard input. Controller-only typing is still pending.", {area.x + 4, y + 44, area.width - 8, 20}, style);
    }

    auto hud_runtime::DrawControllerConfig(const D3DVIEWPORT8& viewport) -> void
    {
        auto& focus = this->controller_config_;
        focus.viewport = viewport;
        if (!focus.active || !this->visible_ || !this->editing_) return;
        if (focus.moving) { this->DrawControllerMovement(viewport); return; }
        const auto modal = this->profile_name_.Focused() ? "profile/" + std::to_string(static_cast<int>(this->profile_dialog_)) :
            this->active_color_ != nullptr ? std::string{"color"} : std::string{};
        if (focus.modal_id != modal) { focus.modal_id = modal; focus.modal_item = 0; }
        this->RefreshControllerConfigFocus();
        if (focus.region == region::movement) this->DrawControllerMovement(viewport);
        this->config_tooltip_.clear();
        rect bounds{};
        std::string help;
        const char* legend = "D-pad: navigate   Confirm: select   Next/Previous: region   Cancel: back";
        if (focus.resizing)
        {
            bounds = this->ConfigLayout().ResizeGrip();
            legend = this->controller_presented_ ? "D-pad: resize   Actions: fine step   Confirm: save   Cancel: restore" :
                "Arrows: resize   Space: fine step   Enter: save   Escape: restore";
        }
        else if (this->profile_name_.Focused())
        {
            const bool share = this->profile_dialog_ == profile_dialog::share, importing = this->profile_dialog_ == profile_dialog::import_profile;
            const ui::profile_dialog_layout dialog{this->config_bounds_, viewport, share || importing};
            bounds = focus.modal_item == 0 ? dialog.Cancel() : focus.modal_item == 1 ? dialog.Create() : importing && focus.modal_item == 2 ? dialog.Paste() : dialog.Field();
            legend = this->ConfirmingAction() || this->profile_dialog_ == profile_dialog::delete_profile ?
                "D-pad: choose action   Confirm: activate   Cancel: close" : "D-pad: choose action   Confirm: activate   Cancel: close   Keyboard: edit name";
        }
        else if (this->choice_input_.Opened())
        {
            bounds = this->choice_input_.Row(this->choice_input_.Highlighted(), viewport);
            legend = "D-pad: choose   Confirm: apply   Actions: preview sound   Cancel: back";
        }
        else if (this->slider_input_.Focused() || this->option_text_input_.Focused())
        {
            bounds = this->slider_input_.Focused() ? this->slider_input_bounds_ : this->text_input_bounds_;
            legend = "Keyboard: edit value   Confirm: apply   Cancel: discard edit";
        }
        else if (this->active_color_ != nullptr)
        {
            const auto panel = this->ConfigLayout().ColorEditor(viewport);
            const ui::color_picker_layout picker{panel};
            bounds = focus.modal_item == 4 ? picker.Original() : focus.modal_item == 6 ? picker.Close() : picker.Hex();
            static constexpr const char* channels[]{"Red", "Green", "Blue", "Alpha"};
            help = focus.modal_item == 4 ? "Restore the original color." : focus.modal_item == 5 ? "Copy the hexadecimal color." : focus.modal_item == 6 ? "Close the color picker." :
                std::string{channels[focus.modal_item]} + ": Left/Right adjusts one step; Up/Down changes channel or action.";
            legend = "Up/Down: channel/action   Left/Right: adjust   Confirm: activate/close   Cancel: back";
        }
        else if (this->profile_dropdown_)
            bounds = this->ConfigLayout().ProfileRow(this->profile_keyboard_selection_ - this->profile_scroll_, this->profiles_.Entries().size());
        else if (this->profile_actions_) bounds = this->ConfigLayout().ProfileActionRow(this->profile_keyboard_selection_);
        else if (this->settings_search_open_)
        {
            const ui::settings_search_layout search{this->ConfigLayout(), this->config_bounds_};
            bounds = this->settings_search_matches_.empty() || this->settings_search_expansion_ < 1 ? this->SettingsSearchBounds() : search.Row(this->settings_search_selection_ - this->settings_search_scroll_);
            legend = "Keyboard: type search   Directions: results   Confirm: open   Actions: clear   Cancel: close";
        }
        else if (this->input_mode_menu_) bounds = this->ConfigLayout().InputModeRow(this->input_mode_selection_);
        else
        {
            const auto items = this->ControllerConfigItems(focus.region);
            if (!items.empty()) { bounds = items[focus.item].bounds; help = items[focus.item].help; }
            if (focus.adjusting) legend = "Left/Right: adjust   Confirm: finish   Cancel: back   Actions: type value";
            else if (focus.region == region::options) legend = "D-pad: navigate   Confirm: edit   Actions: type value / color source   Next/Previous: region";
        }
        if (!help.empty()) { this->config_tooltip_ = std::move(help); this->config_tooltip_anchor_ = bounds; }
        if (!this->config_collapsed_)
        {
            auto style = this->design_system_.GetTheme().caption_text; style.scale = 0.64f; style.alignment = ui::text_alignment::center;
            const auto footer = this->ConfigLayout().Footer();
            this->FillRect(footer, this->design_system_.GetTheme().colors.surface);
            this->config_controls_.Text(legend, footer, style);
        }
        if (bounds.width > 0 && bounds.height > 0) this->design_system_.DrawFocusFrame(bounds, this->general_.FocusStyle());
    }
}
