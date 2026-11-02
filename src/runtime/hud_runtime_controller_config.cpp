#include "hud_runtime.hpp"
#include "config/lua_import_layout.hpp"

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
        this->FinishScale(false);
        this->FinishControllerMove(false);
        this->FinishConfigResize(false);
        this->config_.focus = {};
        this->config_.input_mode_menu = false;
    }

    auto hud_runtime::BeginConfigResize(const bool keyboard) -> void
    {
        this->FinishConfigInput(true, false);
        this->CloseProfileControls(); this->CloseChoiceInput(); this->config_.input_mode_menu = false;
        this->config_.active_color = nullptr; this->config_.active_slider = nullptr;
        this->config_.collapsed = false;
        auto& focus = this->config_.focus;
        if (this->device_ != nullptr) this->device_->GetViewport(&focus.viewport);
        focus.active = true; focus.adjusting = false; focus.resizing = true; focus.fine = false;
        focus.resize_keyboard = keyboard;
        focus.resize_original = this->config_.bounds;
        focus.region = region::utilities; focus.id = "resize";
        this->controller_surface_ = controller_surface::config;
    }

    auto hud_runtime::FinishConfigResize(const bool apply) -> void
    {
        auto& focus = this->config_.focus;
        if (!focus.resizing) return;
        if (!apply) this->config_.bounds = focus.resize_original;
        focus.resizing = false; focus.resize_keyboard = false; focus.fine = false;
        this->ClampOptionScroll();
        if (apply) this->SaveSettings();
    }

    auto hud_runtime::HandleConfigResize(const controller_action value, const bool repeated) -> bool
    {
        auto& focus = this->config_.focus;
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
        this->ApplyConfigResize(this->config_.bounds, ui::resize_right | ui::resize_bottom, dx, dy, focus.viewport);
        return true;
    }

    auto hud_runtime::ControllerConfigItems(const config_controller_region area) -> std::vector<config_controller_item>
    {
        std::vector<config_controller_item> items;
        const auto layout = this->ConfigLayout();
        const auto add = [&](std::string id, const rect& bounds, const size_t index, const char* help, const size_t part = 0) {
            items.push_back({std::move(id), bounds, index, help != nullptr ? help : "", part});
        };
        if (area == region::utilities)
        {
            if (!this->config_.collapsed)
                add("settings-mode", this->SettingsModeBounds(), 8, this->config_.basic ? "Advanced Settings" : "Basic Settings");
            if (!this->config_.collapsed)
                add("edit-mode", layout.EditModeButton(), 9, this->config_.edit_mode ? "Edit Mode ON: show all enabled modules. Confirm turns previews off." : "Edit Mode OFF: show live modules and the selected preview. Confirm previews all enabled modules.");
            add("input", layout.InputMode(), 0, "Input mode: Auto, Keyboard & Mouse, or Controller. Both devices remain available.");
            add("attribution", layout.HeaderAction(), 1, "Attribution");
            add("collapse", layout.HeaderAction(1), 2, this->config_.collapsed ? "Expand configuration" : "Collapse to title bar");
            add("close", layout.CloseButton(), 3, "Close configuration and return to the game");
            if (!this->config_.collapsed)
                add("resize", layout.ResizeGrip(), 4, "Resize configuration. D-pad changes width and height; Actions switches fine steps. Confirm saves; Cancel restores.");
            if (!this->config_.basic) add("sidebar", layout.SidebarToggle(), 5, this->config_.sidebar_collapsed ? "Expand sidebar" : "Collapse sidebar to icons");
            if (!this->config_.basic && this->MovementSelectionModule() != nullptr && this->MovementSelectionModule()->HasEditableBounds() && this->ModuleDisplayed(*this->MovementSelectionModule()) && this->MovementSelectionModule()->EditGrip() != edit_grip_kind::none)
                add("scale", layout.HeaderTitle(), 7, this->MovementSelectionModule()->EditGrip() == edit_grip_kind::resize ?
                    "Resize chat viewport. Directions adjust width/height without changing fonts; Actions changes step; Confirm saves and Cancel restores." :
                    "Scale selected HUD element. Directions adjust X/Y; Actions changes step; Confirm saves and Cancel restores.");
            if (!this->config_.basic) add("search", this->SettingsSearchBounds(), 6, "Search settings (Ctrl+F). Confirm opens; use a keyboard to type, directions for results and Actions to clear.");
            return items;
        }
        if (this->config_.basic && area != region::profiles && area != region::movement) return this->BasicControllerItems(area);
        if (area == region::movement)
        {
            const auto modules = this->ControllerMovableModules();
            const controller_movement_layout bounds{this->config_.bounds, this->config_.focus.viewport, modules.size()};
            for (size_t index = 0; index < modules.size(); ++index)
                add(modules[index]->Id(), bounds.Row(static_cast<int>(index) - this->config_.focus.movement_scroll), index,
                    "Confirm enters Move; Actions opens reset options. While moving: D-pad nudges, Actions changes step, Confirm saves, Cancel restores.");
            return items;
        }
        if (this->config_.collapsed) return items;
        if (area == region::categories)
        {
            for (const auto& row : this->ConfigSidebarRows())
                if (row.available) add(row.id, row.bounds, row.item, row.help != nullptr ? row.help : row.label);
        }
        else if (area == region::topics)
        {
            if (this->config_.module == nullptr) return items;
            const auto topics = this->ConfigTopicRows();
            for (size_t index = 0; index < topics.size(); ++index)
                add(topics[index].id.c_str(), ui::SettingsTopicBounds(layout.Submenu(index, topics.size()), topics[index]), index, topics[index].description);
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
            if (this->IsAttribution())
                for (size_t index = 0; index < std::size(ui::attribution_views); ++index)
                    add(ui::attribution_views[index], ui::config_controls::Segment(layout.ContentTabs(), index, std::size(ui::attribution_views)), index, ui::attribution_views[index]);
            if (this->IsInventorySettings())
                for (size_t index = 0; index < this->inventory_pages_.size(); ++index)
                    add(this->inventory_pages_[index]->Id(), ui::config_controls::Segment(layout.ContentTabs(), index, this->inventory_pages_.size()), index, this->inventory_pages_[index]->DisplayName());
            if (this->IsExperienceDisplay())
            {
                static constexpr const char* names[]{"Bar info", "Readouts"};
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
        else if (area == region::options && this->config_.module != nullptr)
        {
            if (this->IsExperienceDisplay() && this->experience_display_.View() == experience_display_view::readouts)
                return this->experience_display_.ControllerItems(this->experience_, layout);
            if (this->IsXIUIContributors())
            {
                const ui::contributor_list_layout bounds{layout, this->config_.scroll};
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
                if (index >= this->config_.module->OptionCount()) continue;
                const auto option = this->config_.module->Option(index);
                if (!option.available) continue;
                const auto row = layout.OptionRow(static_cast<int>(filtered) - this->config_.scroll);
                const auto id = std::string{this->config_.module->Id()} + "/" + std::to_string(this->config_.module->ConfigView()) + "/" + std::to_string(index);
                if (option.kind == module_option_kind::toggle_buttons)
                {
                    for (size_t part = 0; part < option.choice_count; ++part)
                    {
                        const std::string help = std::string{option.choices[part]} + (option.button_toggles[part] ? ": ON. " : ": OFF. ") + option.description;
                        add(part == 0 ? id : id + "/" + std::to_string(part), ui::config_layout::ToggleButton(row, part, option.choice_count), index, help.c_str(), part);
                    }
                }
                else add(id, row, index, option.description);
            }
        }
        else if (area == region::profiles)
        {
            if (this->profiles_.Ready())
            {
                add("profile", layout.ProfileSelector(), 0, "Select a profile");
                add("add", layout.ProfileAdd(), 1, "Create a profile. Use a keyboard to type its name.");
            }
            add("actions", layout.ProfileActionsButton(), 2, "Profile actions and Reset to defaults for the entire XIUI+ configuration");
        }
        return items;
    }

    auto hud_runtime::RefreshControllerConfigFocus(void) -> void
    {
        auto& focus = this->config_.focus;
        auto items = this->ControllerConfigItems(focus.region);
        focus.Resolve(items);
        for (size_t attempt = 0; items.empty() && attempt < static_cast<size_t>(region::count); ++attempt)
        {
            focus.region = static_cast<region>((static_cast<size_t>(focus.region) + 1) % static_cast<size_t>(region::count));
            items = this->ControllerConfigItems(focus.region);
            focus.Resolve(items);
        }
        if (items.empty()) return;
        if (focus.region == region::categories)
        {
            const auto bounds = this->ConfigSidebarBounds();
            const auto& item = items[focus.item];
            if (item.bounds.y < bounds.y) this->config_.sidebar_animation.scroll -= bounds.y - item.bounds.y;
            else if (item.bounds.y + item.bounds.height > bounds.y + bounds.height)
                this->config_.sidebar_animation.scroll += item.bounds.y + item.bounds.height - bounds.y - bounds.height;
            return;
        }
        if (focus.region == region::movement)
        {
            const controller_movement_layout bounds{this->config_.bounds, focus.viewport, items.size()};
            if (focus.item < static_cast<size_t>(focus.movement_scroll)) focus.movement_scroll = static_cast<int>(focus.item);
            if (focus.item >= static_cast<size_t>(focus.movement_scroll + bounds.rows)) focus.movement_scroll = static_cast<int>(focus.item) - bounds.rows + 1;
            focus.movement_scroll = std::clamp(focus.movement_scroll, 0, std::max(0, static_cast<int>(items.size()) - bounds.rows));
            return;
        }
        if (focus.region != region::options || this->config_.basic) return;
        const auto& item = items[focus.item];
        if (this->IsExperienceDisplay() && this->experience_display_.View() == experience_display_view::readouts)
        {
            this->experience_display_.RevealControllerTarget(static_cast<uint32_t>(item.index), this->experience_, this->ConfigLayout());
            return;
        }
        if (this->IsXIUIContributors())
        {
            const auto area = this->ConfigLayout().ContentArea();
            if (item.bounds.y < area.y) this->config_.scroll -= static_cast<int>(std::ceil((area.y - item.bounds.y) / 22));
            if (item.bounds.y + item.bounds.height > area.y + area.height)
                this->config_.scroll += static_cast<int>(std::ceil((item.bounds.y + item.bounds.height - area.y - area.height) / 22));
        }
        else
        {
            const auto slots = this->GroupOptionSlots();
            const auto filtered = static_cast<size_t>(std::find(slots.begin(), slots.end(), item.index) - slots.begin());
            auto layout = this->ConfigLayout();
            if (static_cast<int>(filtered) < this->config_.scroll) this->config_.scroll = layout.FirstSlotForRow(layout.RowForSlot(static_cast<int>(filtered)));
            while (static_cast<int>(filtered) >= this->config_.scroll + this->ConfigLayout().VisibleSlots())
                this->config_.scroll += layout.Columns();
        }
        this->ClampOptionScroll();
    }

    auto hud_runtime::ActivateControllerConfigItem(const config_controller_item& item, const controller_action value, const bool repeated) -> bool
    {
        const auto area = this->config_.focus.region;
        const bool confirm = value == action::confirm && !repeated;
        if (area == region::movement && value == action::actions && !repeated)
        {
            for (auto* module : this->ControllerMovableModules())
                if (item.id == module->Id()) return this->OpenModuleMenu(*module, this->ScaleGrip(*module));
            return true;
        }
        if (area == region::options)
        {
            if (this->config_.basic) return this->ActivateBasicItem(item.index, value, repeated);
            if (this->IsExperienceDisplay() && this->experience_display_.View() == experience_display_view::readouts)
            {
                if (confirm && this->experience_display_.Activate(static_cast<uint32_t>(item.index), this->experience_, this->ConfigLayout()) == ui::experience_display_editor::result::changed)
                    this->SaveSettings();
                return true;
            }
            if (this->config_.module == nullptr || item.index >= this->config_.module->OptionCount() || !this->MatchesConfigOption(item.index)) return true;
            const auto option = this->config_.module->Option(item.index);
            if (!option.available) return true;
            this->config_.option = item.index;
            if (option.kind == module_option_kind::slider && option.slider != nullptr)
            {
                if (value == action::actions && !repeated)
                {
                    this->config_.slider_input.Open(option); this->config_.slider_module = this->config_.module;
                    this->config_.slider_option = item.index; this->config_.slider_bounds = ui::config_layout::SliderValue(item.bounds);
                    this->config_.focus.adjusting = false;
                }
                else if (confirm) this->config_.focus.adjusting = !this->config_.focus.adjusting;
                else if (this->config_.focus.adjusting && (value == action::left || value == action::right))
                {
                    const float amount = option.step > 0 ? option.step : (option.maximum - option.minimum) / 100;
                    const float next = ui::slider_value_input::Normalize(*option.slider + Direction(value) * amount, option.minimum, option.maximum, option.step);
                    if (next != *option.slider) { *option.slider = next; this->config_.module->OnOptionChanged(item.index); this->SaveSettings(); }
                }
                return true;
            }
            if (!confirm && !(value == action::actions && option.kind == module_option_kind::color_override)) return true;
            if (repeated) return true;
            if (option.kind == module_option_kind::toggle_buttons)
            {
                if (!this->config_.ActivateToggle(option, item.part)) return true;
            }
            else if (option.kind == module_option_kind::toggle && option.toggle != nullptr) this->config_.ActivateToggle(option);
            else if (option.kind == module_option_kind::color_override && option.toggle != nullptr && option.color != nullptr)
            {
                if (value == action::actions) *option.toggle = !*option.toggle;
                else { *option.toggle = true; this->config_.active_color = option.color; this->config_.color_picker.Open(*option.color); this->config_.focus.modal_item = 0; }
            }
            else if (option.kind == module_option_kind::color && option.color != nullptr)
            { this->config_.active_color = option.color; this->config_.color_picker.Open(*option.color); this->config_.focus.modal_item = 0; return true; }
            else if ((option.kind == module_option_kind::choice || option.kind == module_option_kind::dropdown) && option.choice != nullptr && option.choice_count != 0)
            {
                this->config_.choice_input.Open(option, ui::config_layout::ChoiceButton(item.bounds));
                this->config_.choice_module = this->config_.module; this->config_.choice_option = item.index; return true;
            }
            else if (option.kind == module_option_kind::text_input && option.text != nullptr)
            {
                this->config_.text_input.Open(*option.text, option.text_limit); this->config_.text_module = this->config_.module;
                this->config_.text_option = item.index; this->config_.text_value = option.text;
                this->config_.text_bounds = ui::config_layout::TextValue(item.bounds); return true;
            }
            else if (option.kind == module_option_kind::action) return this->ActivateConfigAction(*this->config_.module, item.index);
            else return true;
            this->config_.module->OnOptionChanged(item.index); this->SaveSettings(); return true;
        }
        if (!confirm) return true;
        this->FinishConfigInput(true, false);
        this->config_.focus.adjusting = false;
        if (area == region::categories)
        {
            this->SelectConfigSidebarItem(item.index);
        }
        else if (area == region::topics)
        {
            if (this->HasConfigSubmenu()) this->SelectConfigTopicRow(item.index);
            else
            {
                const auto categories = this->ConfigCategories();
                if (item.index < categories.count) this->config_.group = categories.groups[item.index];
            }
            this->config_.scroll = 0; this->config_.active_color = nullptr;
        }
        else if (area == region::content_tabs)
        {
            if (this->IsInventorySettings()) this->SelectConfigModule(this->inventory_pages_[item.index]);
            else if (this->IsExperienceDisplay()) this->experience_display_.SelectView(static_cast<experience_display_view>(item.index));
            else if (this->IsAttribution()) this->general_.SelectAttributionView(item.index);
            this->config_.scroll = 0; this->config_.active_color = nullptr;
        }
        else if (area == region::elements)
        {
            const auto selector = this->ElementSelector();
            if (!selector.Enabled() || item.index >= selector.count) return true;
            if (selector.combined)
            {
                *selector.scope.choice = item.index == 0 ? 0 : 1;
                this->config_.module->OnOptionChanged(selector.scope_index);
            }
            if (!selector.combined || item.index != 0)
            {
                *selector.element.choice = static_cast<int>(item.index - (selector.combined ? 1 : 0));
                this->config_.module->OnOptionChanged(selector.option_index);
            }
            this->config_.scroll = 0; this->SaveSettings();
        }
        else if (area == region::profiles)
        {
            if (item.index == 0)
            {
                this->CloseProfileControls(); this->profile_dropdown_ = true; this->profile_keyboard_selection_ = this->profiles_.Selected();
                this->profile_scroll_ = std::clamp(static_cast<int>(this->profile_keyboard_selection_) - 3, 0, std::max(0, static_cast<int>(this->profiles_.Entries().size()) - 7));
            }
            else if (item.index == 1) { this->OpenProfileDialog(profile_dialog::create); this->config_.focus.modal_item = 0; }
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
            if (!(*found)->CanMove()) { this->BeginScale(**found, true); return true; }
            auto& focus = this->config_.focus;
            focus.moving_id = (*found)->Id(); focus.move_original = focus.move_desired = (*found)->State().bounds;
            focus.move_collapsed = this->config_.collapsed; focus.moving = true; focus.fine = false;
            this->config_.collapsed = true;
        }
        else if (area == region::utilities)
        {
            if (item.index == 0) { this->config_.input_mode_menu = true; this->config_.input_mode_selection = this->general_.InputMode(); }
            else if (item.index == 1)
            { this->SetBasicSettings(false); this->config_.collapsed = false; this->SelectConfigModule(&this->general_); this->config_.section = general_module::attribution_section; }
            else if (item.index == 2) { this->config_.collapsed = !this->config_.collapsed; this->CloseProfileControls(); }
            else if (item.index == 4) this->BeginConfigResize();
            else if (item.index == 5) this->ToggleConfigSidebar(this->config_.focus.viewport);
            else if (item.index == 6) this->BeginSettingsSearch();
            else if (item.index == 8) this->SetBasicSettings(!this->config_.basic);
            else if (item.index == 9) this->ToggleLayoutEditMode();
            else if (item.index == 7 && this->MovementSelectionModule() != nullptr) this->BeginScale(*this->MovementSelectionModule(), true);
            else this->ExitEdit();
        }
        return true;
    }

    auto hud_runtime::HandleControllerConfig(const controller_action value, const bool repeated) -> bool
    {
        if (!this->visible_ || !this->editing_) return false;
        if (repeated && !Directional(value)) return true;
        if (this->config_.search_open) return this->HandleSettingsSearchController(value, repeated);
        auto& focus = this->config_.focus;
        focus.active = true;
        if (this->scale_gesture_.module != nullptr) return this->HandleScaleAction(value, repeated);
        if (focus.moving) return this->HandleControllerMove(value, repeated);
        if (focus.resizing) return this->HandleConfigResize(value, repeated);
        this->NormalizeConfigSelection();
        if (value == action::confirm) this->ui_sounds_.Select(::GetTickCount64());
        const auto modal = this->profile_name_.Focused() ? "profile/" + std::to_string(static_cast<int>(this->profile_dialog_)) :
            this->config_.active_color != nullptr ? std::string{"color"} : std::string{};
        if (focus.modal_id != modal) { focus.modal_id = modal; focus.modal_item = 0; }
        if (this->config_.input_mode_menu)
        {
            if (value == action::cancel) this->config_.input_mode_menu = false;
            else if (value == action::up || value == action::down)
            {
                const auto previous = this->config_.input_mode_selection;
                this->config_.input_mode_selection = static_cast<size_t>(std::clamp(static_cast<int>(this->config_.input_mode_selection) + Direction(value), 0, 2));
                if (previous != this->config_.input_mode_selection) this->ui_sounds_.Play(ui::native_ui_sound::hover);
            }
            else if (value == action::confirm) { this->general_.SetInputMode(static_cast<int>(this->config_.input_mode_selection)); this->SaveSettings(); this->config_.input_mode_menu = false; }
            return true;
        }
        if (this->config_.choice_input.Opened())
        {
            if (value == action::cancel) this->CloseChoiceInput();
            else if (Directional(value))
            {
                const auto previous = this->config_.choice_input.Highlighted(); this->config_.choice_input.Move(Direction(value));
                if (previous != this->config_.choice_input.Highlighted()) this->ui_sounds_.Play(ui::native_ui_sound::hover);
            }
            else if (value == action::confirm) this->SelectChoiceInput(this->config_.choice_input.Highlighted());
            else if (value == action::actions) this->PreviewChoiceInput(this->config_.choice_input.Highlighted());
            return true;
        }
        if (this->config_.slider_input.Focused() || this->config_.text_input.Focused())
        {
            if (value == action::cancel) this->FinishConfigInput(false);
            else if (value == action::confirm) this->FinishConfigInput(true);
            return true;
        }
        if (this->profile_name_.Focused())
        {
            const bool share = this->profile_dialog_ == profile_dialog::share;
            const bool importing = this->profile_dialog_ == profile_dialog::import_profile;
            if (importing && this->profile_import_lua_) return this->HandleLuaImportController(value);
            const bool deleting = this->profile_dialog_ == profile_dialog::delete_profile;
            const bool confirming = this->ConfirmingAction();
            if (value == action::cancel) this->CloseProfileControls();
            else if (this->profile_transfer_.Busy())
            {
                focus.modal_item = 0;
                if (value == action::confirm) this->CloseProfileControls();
            }
            else if (Directional(value))
            {
                const auto previous = focus.modal_item;
                const int direction = Direction(value);
                int next = std::clamp(static_cast<int>(focus.modal_item) + direction, 0, importing ? 6 : share || deleting || confirming ? 1 : 2);
                if (importing && next == 1 && !this->profile_import_) next += direction;
                focus.modal_item = static_cast<uint8_t>(next);
                if (previous != focus.modal_item) this->ui_sounds_.Play(ui::native_ui_sound::hover);
            }
            else if (value == action::confirm)
            {
                if (focus.modal_item == 0) this->CloseProfileControls();
                else if (share) this->BrowseProfileFile(true);
                else if (deleting) { if (focus.modal_item == 1) this->DeleteProfile(); }
                else if (confirming) { if (focus.modal_item == 1) this->ApplyConfirmedAction(); }
                else if (importing && focus.modal_item == 2) this->BrowseProfileFile(false);
                else if (importing && focus.modal_item == 6) this->PasteProfileKey();
                else if (importing && (focus.modal_item == 4 || focus.modal_item == 5)) this->SetProfileImportSource(focus.modal_item == 5);
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
        if (this->config_.active_color != nullptr)
        {
            if (this->config_.color_picker.EditingHex())
            {
                if (value == action::cancel) this->config_.color_picker.CancelHex();
                else if (value == action::confirm && this->config_.color_picker.CommitHex()) { this->ApplyPickedColor(); this->SaveSettings(); }
                return true;
            }
            if (value == action::cancel) { this->config_.active_color = nullptr; return true; }
            if (value == action::confirm)
            {
                if (focus.modal_item == 4) { this->config_.color_picker.Restore(); this->ApplyPickedColor(); this->SaveSettings(); }
                else if (focus.modal_item == 5)
                    this->config_.color_picker.BeginHex();
                else if (focus.modal_item == 6)
                {
                    const auto* properties = this->core_ != nullptr ? this->core_->GetProperties() : nullptr;
                    this->config_.color_picker.CopyHex(properties != nullptr ? properties->GetFinalFantasyHwnd() : nullptr);
                }
                else this->config_.active_color = nullptr;
                return true;
            }
            if (value == action::up || value == action::down)
            {
                const auto previous = focus.modal_item;
                focus.modal_item = static_cast<uint8_t>(std::clamp(static_cast<int>(focus.modal_item) + Direction(value), 0, 7));
                if (previous != focus.modal_item) this->ui_sounds_.Play(ui::native_ui_sound::hover);
            }
            if ((value == action::left || value == action::right) && focus.modal_item < 4)
            {
                const auto color = this->config_.color_picker.Color();
                const auto shift = static_cast<uint8_t>(focus.modal_item == 3 ? 24 : 16 - focus.modal_item * 8);
                const int component = static_cast<int>((color >> shift) & 255);
                const auto next = (color & ~(0xFFu << shift)) | (static_cast<uint32_t>(std::clamp(component + Direction(value), 0, 255)) << shift);
                this->config_.color_picker.SetColor(next); this->ApplyPickedColor(); this->SaveSettings();
            }
            else if (value == action::actions) { this->config_.color_picker.Restore(); this->ApplyPickedColor(); this->SaveSettings(); }
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
        if (this->config_.input_mode_menu)
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
            this->config_.active_color = nullptr; this->config_.input_mode_menu = !this->config_.input_mode_menu;
            this->config_.input_mode_selection = this->general_.InputMode(); return true;
        }
        if (!this->config_.input_mode_menu) return false;
        if (target >= 0xD100000000000010ULL && target <= 0xD100000000000012ULL)
        { this->general_.SetInputMode(static_cast<int>(target - 0xD100000000000010ULL)); this->SaveSettings(); }
        this->config_.input_mode_menu = false;
        return true;
    }

    auto hud_runtime::DrawInputModeControl(const D3DVIEWPORT8& viewport) -> void
    {
        const auto layout = this->ConfigLayout();
        const auto target = this->InputModeControlTarget(viewport);
        const int mode = this->general_.InputMode();
        const char* label = mode == 0 ? (this->ControllerPresented() ? "Auto: Controller" : "Auto: Keyboard") : general_module::input_modes[mode];
        this->config_controls_.IconButton(layout.InputMode(), this->ControllerPresented() ? ui::icon_glyph::controller : ui::icon_glyph::keyboard,
            this->config_.input_mode_menu || target == 0xD100000000000001ULL);
        if (target == 0xD100000000000001ULL && !this->config_.input_mode_menu)
        {
            this->config_.tooltip = std::string{"Input mode: "} + label;
            this->config_.tooltip_anchor = layout.InputMode();
        }
        if (!this->config_.input_mode_menu) return;
        this->config_controls_.Panel(layout.InputModeMenu());
        for (size_t index = 0; index < 3; ++index)
            this->config_controls_.NavigationItem(layout.InputModeRow(index), general_module::input_modes[index], static_cast<size_t>(mode) == index,
                target == 0xD100000000000010ULL + index || (this->config_.focus.active && this->config_.input_mode_selection == index));
    }

    auto hud_runtime::DrawControllerInputStatus(void) -> void
    {
        if (this->config_.module != &this->general_ || this->config_.section != general_module::input_section) return;
        auto area = this->ConfigLayout().ContentArea();
        auto style = this->design_system_.GetTheme().caption_text;
        style.scale = 0.7f; style.alignment = ui::text_alignment::left;
        const float y = std::max(area.y + 224, area.y + area.height - 86);
        const auto clip = this->design_system_.Clip(area);
        const auto& status = this->general_.ControllerStatus();
        const char* readiness = status.enumeration_disabled ? "Runtime ready. Ashita controller enumeration is disabled; check your launch configuration." :
            status.configuration_known && !status.gamepad_enabled ? "Runtime ready. The gamepad is disabled in FFXI Config." :
            status.connection_known && !status.connected ? "Runtime ready. Connect a controller to test navigation." :
            !status.bindings_known ? "Runtime ready. Waiting for a verified controller mapping; use /xiuiplus focus config to enter." :
            "XIUI+ controller runtime ready. Enter with /xiuiplus focus config.";
        this->config_controls_.Text(readiness, {area.x + 4, y, area.width - 8, 20}, style);
        this->config_controls_.Text("Change region: Next / Previous tab. Navigate: D-pad. Activate: Confirm. Back: Cancel.", {area.x + 4, y + 22, area.width - 8, 20}, style);
        this->config_controls_.Text("Text and profile names accept keyboard input. Controller-only typing is still pending.", {area.x + 4, y + 44, area.width - 8, 20}, style);
    }

    auto hud_runtime::DrawControllerConfig(const D3DVIEWPORT8& viewport) -> void
    {
        auto& focus = this->config_.focus;
        focus.viewport = viewport;
        if (!focus.active || !this->visible_ || !this->editing_) return;
        if (this->scale_gesture_.module != nullptr) return;
        if (focus.moving) { this->DrawControllerMovement(viewport); return; }
        const auto modal = this->profile_name_.Focused() ? "profile/" + std::to_string(static_cast<int>(this->profile_dialog_)) :
            this->config_.active_color != nullptr ? std::string{"color"} : std::string{};
        if (focus.modal_id != modal) { focus.modal_id = modal; focus.modal_item = 0; }
        this->RefreshControllerConfigFocus();
        if (focus.region == region::movement) this->DrawControllerMovement(viewport);
        this->config_.tooltip.clear();
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
            const ui::profile_dialog_layout dialog{this->config_.bounds, viewport, share || importing};
            bounds = focus.modal_item == 0 ? dialog.Cancel() : focus.modal_item == 1 ? dialog.Create() : importing && focus.modal_item == 2 ? dialog.Browse() : dialog.Field();
            if (importing && (focus.modal_item == 4 || focus.modal_item == 5)) bounds = dialog.ImportSource(focus.modal_item - 4);
            if (importing && focus.modal_item == 6) bounds = dialog.Paste();
            if (importing && this->profile_import_lua_)
            { const ui::lua_import_layout luaLayout{this->config_.bounds, viewport}; bounds = luaLayout.Control(focus.modal_item); }
            legend = this->ConfirmingAction() || this->profile_dialog_ == profile_dialog::delete_profile ?
                "D-pad: choose action   Confirm: activate   Cancel: close" : "D-pad: choose action   Confirm: activate   Cancel: close   Keyboard: edit name";
        }
        else if (this->config_.choice_input.Opened())
        {
            bounds = this->config_.choice_input.Row(this->config_.choice_input.Highlighted(), viewport);
            legend = "D-pad: choose   Confirm: apply   Actions: preview sound   Cancel: back";
        }
        else if (this->config_.slider_input.Focused() || this->config_.text_input.Focused())
        {
            bounds = this->config_.slider_input.Focused() ? this->config_.slider_bounds : this->config_.text_bounds;
            legend = "Keyboard: edit value   Confirm: apply   Cancel: discard edit";
        }
        else if (this->config_.active_color != nullptr)
        {
            const auto panel = this->ConfigLayout().ColorEditor(viewport);
            const ui::color_picker_layout picker{panel};
            bounds = focus.modal_item == 4 ? picker.Original() : focus.modal_item == 6 ? picker.Copy() : focus.modal_item == 7 ? picker.Close() : picker.Hex();
            static constexpr const char* channels[]{"Red", "Green", "Blue", "Alpha"};
            help = this->config_.color_picker.EditingHex() ? "Type or paste #RRGGBB or #RRGGBBAA. Confirm applies; Cancel discards." :
                focus.modal_item == 4 ? "Restore the original color." : focus.modal_item == 5 ? "Edit the hexadecimal color with a keyboard." : focus.modal_item == 6 ? "Copy the hexadecimal color." : focus.modal_item == 7 ? "Close the color picker." :
                std::string{channels[focus.modal_item]} + ": Left/Right adjusts one step; Up/Down changes channel or action.";
            legend = "Up/Down: channel/action   Left/Right: adjust   Confirm: activate/close   Cancel: back";
        }
        else if (this->profile_dropdown_)
            bounds = this->ConfigLayout().ProfileRow(this->profile_keyboard_selection_ - this->profile_scroll_, this->profiles_.Entries().size());
        else if (this->profile_actions_) bounds = this->ConfigLayout().ProfileActionRow(this->profile_keyboard_selection_);
        else if (this->config_.search_open)
        {
            const ui::settings_search_layout search{this->ConfigLayout(), this->config_.bounds};
            bounds = this->config_.search_matches.empty() || this->config_.search_expansion < 1 ? this->SettingsSearchBounds() : search.Row(this->config_.search_selection - this->config_.search_scroll);
            legend = "Keyboard: type search   Directions: results   Confirm: open   Actions: clear   Cancel: close";
        }
        else if (this->config_.input_mode_menu) bounds = this->ConfigLayout().InputModeRow(this->config_.input_mode_selection);
        else
        {
            const auto items = this->ControllerConfigItems(focus.region);
            if (!items.empty()) { bounds = items[focus.item].bounds; help = items[focus.item].help; }
            if (focus.adjusting) legend = "Left/Right: adjust   Confirm: finish   Cancel: back   Actions: type value";
            else if (focus.region == region::options) legend = "D-pad: navigate   Confirm: edit   Actions: type value / color source   Next/Previous: region";
            if (this->config_.basic) legend = focus.adjusting ? "Left/Right: adjust   Confirm: done   Actions: type value" :
                "Confirm: select   Next/Previous: region";
        }
        if (!help.empty()) { this->config_.tooltip = std::move(help); this->config_.tooltip_anchor = bounds; }
        if (!this->config_.collapsed)
        {
            auto style = this->design_system_.GetTheme().caption_text; style.scale = 0.64f; style.alignment = ui::text_alignment::center;
            const auto footer = this->ConfigLayout().Footer();
            if (footer.width < 500) legend = focus.adjusting ? "Left/Right: adjust   Confirm: done" : "Confirm: select   Cancel: back";
            this->FillRect(footer, this->design_system_.GetTheme().colors.surface);
            const auto clip = this->design_system_.Clip(footer);
            this->config_controls_.Text(legend, footer, style);
        }
        if (bounds.width > 0 && bounds.height > 0) this->design_system_.DrawFocusFrame(bounds, this->general_.FocusStyle());
    }
}
