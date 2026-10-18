#include "hud_runtime.hpp"

#include <functional>
#include <cstring>
#include <memory>
#include <set>

namespace
{
    constexpr uint64_t search_input = 0xD200000000000001ULL, search_panel = 0xD200000000000002ULL;
    constexpr uint64_t search_clear = 0xD200000000000003ULL;
    constexpr uint64_t search_result = 0xD200000000000010ULL;

    auto NavigationSelector(const xiui::module_option& option) -> bool
    {
        return option.choice != nullptr && option.choice_count != 0 &&
            (option.selector != xiui::module_selector::none ||
                (option.label != nullptr && std::strcmp(option.label, "Native filter group") == 0));
    }
}

namespace xiui
{
    auto hud_runtime::BuildSettingsSearch(void) -> void
    {
        auto catalog = std::make_unique<hud_runtime>();
        catalog->LoadSettings(this->CaptureSettings());
        catalog->chat_filters_ = this->chat_filters_;
        this->settings_search_entries_.clear();
        std::set<std::string> modules;
        static constexpr const char* groups[]{"General", "Appearance", "Effects", "Colors", "Text"};
        static constexpr const char* displays[]{"Bar info", "Readouts", "Layout"};
        for (const auto& page : catalog->ConfigPages())
        for (size_t member = 0; member < page.Count(); ++member)
        {
            auto* module = page.Module(member);
            if (module == nullptr || !modules.insert(module->Id()).second) continue;
            catalog->active_module_ = module;
            for (size_t view = 0; view < std::max<size_t>(1, module->ConfigViews().size()); ++view)
            {
                module->SelectConfigView(view);
                const auto sections = module->ConfigSections();
                for (size_t section = 0; section < (sections.empty() ? std::size(groups) : sections.size()); ++section)
                {
                    catalog->active_config_section_ = sections.empty() ? 0 : section;
                    catalog->active_option_group_ = sections.empty() ? static_cast<module_option_group>(section) : module_option_group::general;
                    const size_t tabs = catalog->IsGeneralAppearance() ? std::size(general_module::appearance_tabs) : catalog->IsExperienceDisplay() ? 3 : 1;
                    for (size_t tab = 0; tab < tabs; ++tab)
                    {
                        if (catalog->IsGeneralAppearance()) catalog->general_.SelectAppearanceTab(tab);
                        if (catalog->IsExperienceDisplay()) catalog->experience_display_.SelectView(static_cast<experience_display_view>(tab));
                        std::string location = page.name;
                        if (std::strcmp(module->DisplayName(), page.name) != 0) location += std::string{" / "} + module->DisplayName();
                        if (!module->ConfigViews().empty()) location += std::string{" / "} + module->ConfigViews()[view];
                        location += std::string{" / "} + (sections.empty() ? groups[section] : sections[section].name);
                        if (catalog->IsGeneralAppearance()) location += std::string{" / "} + general_module::appearance_tabs[tab];
                        if (catalog->IsExperienceDisplay()) location += std::string{" / "} + displays[tab];
                        std::vector<std::pair<size_t, int>> selectors;
                        std::vector<std::string> labels;
                        std::set<size_t> visiting;
                        std::function<void()> collect = [&] {
                            for (size_t index = 0; index < module->OptionCount(); ++index)
                            {
                                if (!catalog->MatchesConfigOption(index) || visiting.contains(index)) continue;
                                const auto option = module->Option(index);
                                if (!NavigationSelector(option) || !option.available) continue;
                                const auto original = *option.choice;
                                visiting.insert(index);
                                for (size_t choice = 0; choice < option.choice_count; ++choice)
                                {
                                    *option.choice = static_cast<int>(choice);
                                    selectors.emplace_back(index, static_cast<int>(choice)); labels.emplace_back(option.choices[choice]);
                                    collect();
                                    labels.pop_back(); selectors.pop_back();
                                }
                                *option.choice = original; visiting.erase(index);
                                return;
                            }
                            std::string path = location;
                            for (const auto& label : labels) path += " / " + label;
                            for (const auto index : catalog->GroupOptionSlots())
                            {
                                if (index >= module->OptionCount()) continue;
                                const auto option = module->Option(index);
                                if (NavigationSelector(option) || option.label == nullptr || *option.label == '\0') continue;
                                ui::settings_search_entry entry;
                                entry.module = module->Id(); entry.label = option.label; entry.path = path;
                                entry.description = option.description != nullptr ? option.description : "";
                                entry.view = view; entry.section = catalog->active_config_section_; entry.option = index;
                                entry.group = catalog->active_option_group_; entry.selectors = selectors;
                                entry.appearance_tab = tab; entry.display_view = static_cast<int>(tab);
                                entry.label_terms = ui::SettingsSearchText(entry.label);
                                entry.path_terms = ui::SettingsSearchText(entry.path);
                                entry.description_terms = ui::SettingsSearchText(entry.description);
                                this->settings_search_entries_.push_back(std::move(entry));
                            }
                        };
                        collect();
                    }
                }
            }
        }
    }

    auto hud_runtime::BeginSettingsSearch(void) -> void
    {
        if (this->settings_search_open_) { this->settings_search_input_.SelectAll(); return; }
        this->FinishConfigInput(true, false); this->CloseProfileControls(); this->ReleaseChatInput();
        this->FinishConfigResize(false); this->FinishControllerMove(false);
        this->active_color_ = nullptr; this->active_slider_ = nullptr; this->input_mode_menu_ = false;
        this->config_collapsed_ = false;
        this->BuildSettingsSearch();
        this->settings_search_input_.Open(this->settings_search_input_.Editor().Text(), 128);
        this->settings_search_open_ = true;
        this->settings_search_animation_at_ = ::GetTickCount64();
        this->UpdateSettingsSearch();
    }

    auto hud_runtime::CloseSettingsSearch(void) -> void
    { this->settings_search_open_ = false; this->settings_search_input_.Cancel(); }

    auto hud_runtime::ClearSettingsSearch(void) -> void
    {
        this->settings_search_input_.Open({}, 128);
        this->UpdateSettingsSearch();
    }

    auto hud_runtime::AnimateSettingsSearch(const uint64_t now) -> void
    {
        const float step = this->settings_search_animation_at_ != 0 ?
            static_cast<float>(std::min<uint64_t>(now - this->settings_search_animation_at_, 180)) / 180 : 0;
        this->settings_search_animation_at_ = now;
        this->settings_search_expansion_ = std::clamp(this->settings_search_expansion_ + (this->settings_search_open_ ? step : -step), 0.0f, 1.0f);
    }

    auto hud_runtime::SettingsSearchBounds(void) const -> rect
    {
        const auto progress = this->settings_search_expansion_;
        return this->ConfigLayout().SearchInput(progress * progress * (3 - 2 * progress));
    }

    auto hud_runtime::UpdateSettingsSearch(void) -> void
    {
        this->settings_search_matches_ = ui::FindSettings(this->settings_search_entries_, this->settings_search_input_.Editor().Text());
        this->settings_search_selection_ = this->settings_search_scroll_ = 0;
    }

    auto hud_runtime::MoveSettingsSearch(const int steps) -> void
    {
        if (this->settings_search_matches_.empty()) return;
        this->settings_search_selection_ = static_cast<size_t>(std::clamp(static_cast<int>(this->settings_search_selection_) + steps, 0,
            static_cast<int>(this->settings_search_matches_.size()) - 1));
        const auto rows = ui::settings_search_layout{this->ConfigLayout(), this->config_bounds_}.rows;
        if (this->settings_search_selection_ < this->settings_search_scroll_) this->settings_search_scroll_ = this->settings_search_selection_;
        if (this->settings_search_selection_ >= this->settings_search_scroll_ + rows) this->settings_search_scroll_ = this->settings_search_selection_ - rows + 1;
    }

    auto hud_runtime::SelectSettingsSearch(const size_t index) -> bool
    {
        if (index >= this->settings_search_matches_.size()) return false;
        const auto entry = this->settings_search_entries_[this->settings_search_matches_[index]];
        hud_module* module{};
        for (auto* candidate : this->modules_.All()) if (entry.module == candidate->Id()) { module = candidate; break; }
        if (module == nullptr) return false;
        this->CloseSettingsSearch();
        this->settings_search_revealed_module_ = module;
        this->SelectConfigModule(module, entry.view);
        this->active_config_section_ = entry.section; this->active_option_group_ = entry.group;
        if (this->IsGeneralAppearance()) this->general_.SelectAppearanceTab(entry.appearance_tab);
        if (this->IsExperienceDisplay()) this->experience_display_.SelectView(static_cast<experience_display_view>(entry.display_view));
        for (const auto& [optionIndex, choice] : entry.selectors)
        {
            if (optionIndex >= module->OptionCount()) continue;
            const auto option = module->Option(optionIndex);
            if (!NavigationSelector(option) || choice < 0 || static_cast<size_t>(choice) >= option.choice_count || *option.choice == choice) continue;
            *option.choice = choice;
            if (option.selector != module_selector::none) module->OnOptionChanged(optionIndex);
        }
        if (entry.option >= module->OptionCount() || !this->MatchesConfigOption(entry.option)) return false;
        const auto slots = this->GroupOptionSlots();
        const auto found = std::find(slots.begin(), slots.end(), entry.option);
        if (found == slots.end()) return false;
        this->option_scroll_ = this->ConfigLayout().FirstSlotForRow(this->ConfigLayout().RowForSlot(static_cast<int>(found - slots.begin())));
        this->ClampOptionScroll();
        this->settings_search_highlight_module_ = module; this->settings_search_highlight_option_ = entry.option;
        this->settings_search_highlight_until_ = ::GetTickCount64() + 3500;
        this->controller_config_.adjusting = false;
        this->controller_config_.active = module->Option(entry.option).available;
        this->controller_config_.region = config_controller_region::options;
        this->controller_config_.id = entry.module + "/" + std::to_string(entry.view) + "/" + std::to_string(entry.option);
        if (this->controller_config_.active) this->RefreshControllerConfigFocus();
        this->SaveSettings();
        return true;
    }

    auto hud_runtime::HandleSettingsSearchKey(const WPARAM key, const LPARAM flags, const HWND window) -> bool
    {
        if (!this->settings_search_open_) return false;
        const bool repeated = (static_cast<uint32_t>(flags) & 0x40000000u) != 0;
        if ((static_cast<uint32_t>(flags) & 0x80000000u) != 0) return true;
        if (key == VK_TAB && (static_cast<uint32_t>(flags) & 0x20000000u) != 0) { this->CloseSettingsSearch(); return false; }
        if (key == VK_ESCAPE) { if (!repeated) this->CloseSettingsSearch(); return true; }
        if (key == VK_RETURN) { if (!repeated) this->SelectSettingsSearch(this->settings_search_selection_); return true; }
        if (key == VK_UP || key == VK_DOWN || key == VK_PRIOR || key == VK_NEXT || key == VK_TAB)
        {
            const int page = static_cast<int>(ui::settings_search_layout{this->ConfigLayout(), this->config_bounds_}.rows);
            this->MoveSettingsSearch(key == VK_UP ? -1 : key == VK_PRIOR ? -page : key == VK_NEXT ? page :
                key == VK_TAB && (::GetKeyState(VK_SHIFT) & 0x8000) != 0 ? -1 : 1);
            return true;
        }
        const auto before = this->settings_search_input_.Editor().Text();
        this->settings_search_input_.HandleKey(key, flags, window);
        if (before != this->settings_search_input_.Editor().Text()) this->UpdateSettingsSearch();
        return true;
    }

    auto hud_runtime::HandleSettingsSearchController(const controller_action action, const bool repeated) -> bool
    {
        if (!this->settings_search_open_) return false;
        if (action == controller_action::cancel) { if (!repeated) this->CloseSettingsSearch(); }
        else if (action == controller_action::confirm) { if (!repeated) this->SelectSettingsSearch(this->settings_search_selection_); }
        else if (action == controller_action::up || action == controller_action::down) this->MoveSettingsSearch(action == controller_action::up ? -1 : 1);
        else if (action == controller_action::left || action == controller_action::right)
            this->MoveSettingsSearch((action == controller_action::left ? -1 : 1) * static_cast<int>(ui::settings_search_layout{this->ConfigLayout(), this->config_bounds_}.rows));
        else if (action == controller_action::actions && !repeated) this->ClearSettingsSearch();
        return true;
    }

    auto hud_runtime::SettingsSearchTarget(const D3DVIEWPORT8&) const -> uint64_t
    {
        if (!this->editing_ || this->config_collapsed_) return 0;
        const ui::settings_search_layout layout{this->ConfigLayout(), this->config_bounds_};
        const auto x = static_cast<float>(this->mouse_x_), y = static_cast<float>(this->mouse_y_);
        const auto input = this->SettingsSearchBounds();
        if (this->settings_search_open_ && !this->settings_search_input_.Editor().Text().empty() && input.width >= 70 &&
            Contains({input.x + input.width - 24, input.y, 24, input.height}, x, y)) return search_clear;
        if (Contains(input, x, y)) return search_input;
        if (!this->settings_search_open_ || !Contains(layout.panel, x, y)) return 0;
        for (size_t row = 0; row < layout.rows && row + this->settings_search_scroll_ < this->settings_search_matches_.size(); ++row)
            if (Contains(layout.Row(row), x, y)) return search_result + row + this->settings_search_scroll_;
        return search_panel;
    }

    auto hud_runtime::ProcessSettingsSearchClick(const D3DVIEWPORT8& viewport) -> bool
    {
        const auto target = this->SettingsSearchTarget(viewport);
        if (target == search_clear) { this->ClearSettingsSearch(); return true; }
        if (target == search_input) { this->BeginSettingsSearch(); return true; }
        if (!this->settings_search_open_) return false;
        if (target >= search_result) this->SelectSettingsSearch(static_cast<size_t>(target - search_result));
        else if (target != search_panel) this->CloseSettingsSearch();
        return true;
    }

    auto hud_runtime::DrawSettingsSearch(const D3DVIEWPORT8&) -> void
    {
        if (this->config_collapsed_) return;
        this->AnimateSettingsSearch(::GetTickCount64());
        const ui::settings_search_layout layout{this->ConfigLayout(), this->config_bounds_};
        const auto& theme = this->design_system_.GetTheme();
        auto text = theme.body_text; text.scale = 0.76f; text.alignment = ui::text_alignment::left;
        const auto input = this->SettingsSearchBounds();
        const bool hovered = Contains(input, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
        if (this->settings_search_expansion_ == 0)
        {
            this->config_controls_.IconButton(input, ui::icon_glyph::search, hovered);
            if (hovered) { this->config_tooltip_ = "Search settings (Ctrl+F)"; this->config_tooltip_anchor_ = input; }
        }
        else
        {
            const auto clip = this->design_system_.Clip(input);
            this->design_system_.FillRoundedRect(input, theme.colors.chrome, 3);
            this->design_system_.FillRoundedRect(input, 0x38000000, 3);
            this->design_system_.DrawIcon({input.x + 6, input.y + 5, 12, 12}, ui::icon_glyph::search, theme.colors.muted);
            const bool clear = this->settings_search_open_ && !this->settings_search_input_.Editor().Text().empty() && input.width >= 70;
            const ui::rect field{input.x + 22, input.y, input.width - 22 - (clear ? 24 : 0), input.height};
            if (field.width >= 24)
            {
                const auto fieldClip = this->design_system_.Clip(field);
                this->config_controls_.TextInput(field, this->settings_search_input_, false, "Search settings...", "Ctrl+F");
            }
            if (clear)
            {
                const ui::rect button{input.x + input.width - 24, input.y, 24, input.height};
                this->config_controls_.IconButton(button, ui::icon_glyph::close,
                    Contains(button, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_)));
            }
        }
        if (!this->settings_search_open_)
        {
            if (this->settings_search_highlight_module_ != this->active_module_ || ::GetTickCount64() >= this->settings_search_highlight_until_) return;
            const auto slots = this->GroupOptionSlots();
            const auto found = std::find(slots.begin(), slots.end(), this->settings_search_highlight_option_);
            if (found != slots.end())
            {
                const auto row = this->ConfigLayout().OptionRow(static_cast<int>(found - slots.begin()) - this->option_scroll_);
                const auto area = this->ConfigLayout().ContentArea();
                if (row.y >= area.y && row.y + row.height <= area.y + area.height)
                    this->design_system_.BorderRect(row, theme.colors.control_selected);
            }
            return;
        }
        this->config_tooltip_.clear();
        if (this->settings_search_expansion_ < 1) return;
        const ui::container_fill shade{layout.panel, 0x50000000};
        this->config_controls_.Panel(layout.panel, {&shade, 1});
        auto caption = theme.caption_text; caption.scale = 0.64f; caption.alignment = ui::text_alignment::left;
        if (this->settings_search_matches_.empty())
            this->config_controls_.Text(this->settings_search_input_.Editor().Text().empty() ? "Search by setting, category or feature. Try: party hp" : "No matching settings.", layout.Row(0), caption);
        for (size_t row = 0; row < layout.rows && row + this->settings_search_scroll_ < this->settings_search_matches_.size(); ++row)
        {
            const auto index = row + this->settings_search_scroll_;
            const auto& entry = this->settings_search_entries_[this->settings_search_matches_[index]];
            const auto bounds = layout.Row(row);
            this->config_controls_.NavigationItem(bounds, "", index == this->settings_search_selection_,
                Contains(bounds, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_)));
            this->config_controls_.Text(entry.label.c_str(), {bounds.x + 8, bounds.y + 1, bounds.width - 16, 21}, text);
            this->config_controls_.Text(entry.path.c_str(), {bounds.x + 8, bounds.y + 23, bounds.width - 16, 17}, caption);
        }
        const auto total = this->settings_search_matches_.size();
        const auto range = total == 0 ? std::string{"0 results"} : std::to_string(this->settings_search_scroll_ + 1) + "-" +
            std::to_string(std::min(total, this->settings_search_scroll_ + layout.rows)) + " of " + std::to_string(total);
        const auto footer = range + (this->ControllerPresented() ? " | Directions: select | Confirm: open | Actions: clear | Cancel: close" :
            " | Up/Down: select | Enter: open | Esc: close");
        this->config_controls_.Text(footer.c_str(), layout.footer, caption);
    }
}
