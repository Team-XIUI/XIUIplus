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
                (option.label != nullptr && (std::strcmp(option.label, "Native filter group") == 0 || std::strcmp(option.label, "Native window") == 0)));
    }
}

namespace xiui
{
    auto hud_runtime::BuildSettingsSearch(void) -> void
    {
        auto catalog = std::make_unique<hud_runtime>();
        catalog->LoadSettings(this->CaptureSettings());
        catalog->chat_filters_ = this->chat_filters_;
        catalog->native_ui_ = this->native_ui_;
        catalog->UpdateNativeWindows(this->hud_viewport_);
        this->config_.search_entries.clear();
        std::set<std::string> modules;
        static constexpr const char* groups[]{"General", "Appearance", "Effects", "Colors", "Text"};
        static constexpr const char* displays[]{"Bar info", "Readouts"};
        for (const auto& page : catalog->ConfigPages())
        for (size_t member = 0; member < page.Count(); ++member)
        {
            auto* module = page.Module(member);
            if (module == nullptr || !modules.insert(module->Id()).second) continue;
            catalog->config_.module = module;
            for (size_t view = 0; view < std::max<size_t>(1, module->ConfigViews().size()); ++view)
            {
                module->SelectConfigView(view);
                const auto sections = module->ConfigSections();
                for (size_t section = 0; section < (sections.empty() ? std::size(groups) : sections.size()); ++section)
                {
                    if (module == &catalog->general_ && section == general_module::game_hud_section) continue;
                    catalog->config_.section = sections.empty() ? 0 : section;
                    catalog->config_.group = sections.empty() ? static_cast<module_option_group>(section) : module_option_group::general;
                    const size_t tabs = catalog->IsGeneralAppearance() ? std::size(general_module::appearance_tabs) : catalog->IsExperienceDisplay() ? std::size(displays) : 1;
                    for (size_t tab = 0; tab < tabs; ++tab)
                    {
                        if (catalog->IsGeneralAppearance()) catalog->general_.SelectAppearanceTab(tab);
                        if (catalog->IsExperienceDisplay()) catalog->experience_display_.SelectView(static_cast<experience_display_view>(tab));
                        std::string location = i18n::Text(page.name);
                        if (std::strcmp(module->DisplayName(), page.name) != 0) location += std::string{" / "} + i18n::Text(module->DisplayName());
                        if (!module->ConfigViews().empty()) location += std::string{" / "} + i18n::Text(module->ConfigViews()[view]);
                        if (catalog->IsGeneralAppearance()) location += std::string{" / "} + i18n::Text(tab == 0 ? "Appearance" : general_module::appearance_tabs[tab]);
                        else location += std::string{" / "} + (sections.empty() ? i18n::Text(groups[section]) : ui::SettingsTopicPath(sections, section, module != &catalog->general_));
                        if (catalog->IsGeneralAppearance() && tab >= 3) location = std::string{i18n::Text("Extras")} + " / " + i18n::Text(general_module::appearance_tabs[tab]);
                        if (catalog->IsExperienceDisplay()) location += std::string{" / "} + i18n::Text(displays[tab]);
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
                                    selectors.emplace_back(index, static_cast<int>(choice)); labels.emplace_back(option.ChoiceText(choice));
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
                                entry.module = module->Id(); entry.label = i18n::Text(option.label); entry.path = path;
                                entry.description = option.description != nullptr ? i18n::Text(option.description) : "";
                                entry.view = view; entry.section = catalog->config_.section; entry.option = index;
                                entry.group = catalog->config_.group; entry.selectors = selectors;
                                entry.appearance_tab = tab; entry.display_view = static_cast<int>(tab);
                                entry.label_terms = ui::SettingsSearchText(entry.label) + " " + ui::SettingsSearchText(option.label);
                                entry.path_terms = ui::SettingsSearchText(entry.path);
                                entry.description_terms = ui::SettingsSearchText(entry.description) + " " + ui::SettingsSearchText(option.description != nullptr ? option.description : "");
                                this->config_.search_entries.push_back(std::move(entry));
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
        this->SetBasicSettings(false);
        if (this->config_.search_open) { this->config_.search_input.SelectAll(); return; }
        this->FinishConfigInput(true, false); this->CloseProfileControls(); this->ReleaseChatInput();
        this->FinishConfigResize(false); this->FinishControllerMove(false);
        this->config_.active_color = nullptr; this->config_.active_slider = nullptr; this->config_.header_menu_open = false;
        this->config_.collapsed = false;
        this->BuildSettingsSearch();
        this->config_.search_input.Open(this->config_.search_input.Editor().Text(), 128);
        this->config_.search_open = true;
        this->config_.search_animation_at = ::GetTickCount64();
        this->UpdateSettingsSearch();
    }

    auto hud_runtime::CloseSettingsSearch(void) -> void
    { this->config_.search_open = false; this->config_.search_input.Cancel(); }

    auto hud_runtime::ClearSettingsSearch(void) -> void
    {
        this->config_.search_input.Open({}, 128);
        this->UpdateSettingsSearch();
    }

    auto hud_runtime::AnimateSettingsSearch(const uint64_t now) -> void
    {
        const float step = this->config_.search_animation_at != 0 ?
            static_cast<float>(std::min<uint64_t>(now - this->config_.search_animation_at, 180)) / 180 : 0;
        this->config_.search_animation_at = now;
        this->config_.search_expansion = std::clamp(this->config_.search_expansion + (this->config_.search_open ? step : -step), 0.0f, 1.0f);
    }

    auto hud_runtime::SettingsSearchBounds(void) const -> rect
    {
        const auto progress = this->config_.search_expansion;
        return this->ConfigLayout().SearchInput(progress * progress * (3 - 2 * progress));
    }

    auto hud_runtime::UpdateSettingsSearch(void) -> void
    {
        this->config_.search_matches = ui::FindSettings(this->config_.search_entries, this->config_.search_input.Editor().Text());
        this->config_.search_selection = this->config_.search_scroll = 0;
    }

    auto hud_runtime::MoveSettingsSearch(const int steps) -> void
    {
        if (this->config_.search_matches.empty()) return;
        this->config_.search_selection = static_cast<size_t>(std::clamp(static_cast<int>(this->config_.search_selection) + steps, 0,
            static_cast<int>(this->config_.search_matches.size()) - 1));
        const auto rows = ui::settings_search_layout{this->ConfigLayout(), this->config_.bounds}.rows;
        if (this->config_.search_selection < this->config_.search_scroll) this->config_.search_scroll = this->config_.search_selection;
        if (this->config_.search_selection >= this->config_.search_scroll + rows) this->config_.search_scroll = this->config_.search_selection - rows + 1;
    }

    auto hud_runtime::SelectSettingsSearch(const size_t index) -> bool
    {
        if (index >= this->config_.search_matches.size()) return false;
        const auto entry = this->config_.search_entries[this->config_.search_matches[index]];
        hud_module* module{};
        for (auto* candidate : this->modules_.All()) if (entry.module == candidate->Id()) { module = candidate; break; }
        if (module == nullptr) return false;
        this->CloseSettingsSearch();
        this->config_.search_revealed_module = module;
        this->SelectConfigModule(module, entry.view);
        this->config_.section = entry.section; this->config_.group = entry.group;
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
        this->config_.scroll = this->ConfigLayout().FirstSlotForRow(this->ConfigLayout().RowForSlot(static_cast<int>(found - slots.begin())));
        this->ClampOptionScroll();
        this->config_.search_highlight_module = module; this->config_.search_highlight_option = entry.option;
        this->config_.search_highlight_until = ::GetTickCount64() + 3500;
        this->config_.focus.adjusting = false;
        this->config_.focus.active = module->Option(entry.option).available;
        this->config_.focus.region = config_controller_region::options;
        this->config_.focus.id = entry.module + "/" + std::to_string(entry.view) + "/" + std::to_string(entry.option);
        if (this->config_.focus.active) this->RefreshControllerConfigFocus();
        this->SaveSettings();
        return true;
    }

    auto hud_runtime::HandleSettingsSearchKey(const WPARAM key, const LPARAM flags, const HWND window) -> bool
    {
        if (!this->config_.search_open) return false;
        const bool repeated = (static_cast<uint32_t>(flags) & 0x40000000u) != 0;
        if ((static_cast<uint32_t>(flags) & 0x80000000u) != 0) return true;
        if (key == VK_TAB && (static_cast<uint32_t>(flags) & 0x20000000u) != 0) { this->CloseSettingsSearch(); return false; }
        if (key == VK_ESCAPE) { if (!repeated) this->CloseSettingsSearch(); return true; }
        if (key == VK_RETURN) { if (!repeated) this->SelectSettingsSearch(this->config_.search_selection); return true; }
        if (key == VK_UP || key == VK_DOWN || key == VK_PRIOR || key == VK_NEXT || key == VK_TAB)
        {
            const int page = static_cast<int>(ui::settings_search_layout{this->ConfigLayout(), this->config_.bounds}.rows);
            this->MoveSettingsSearch(key == VK_UP ? -1 : key == VK_PRIOR ? -page : key == VK_NEXT ? page :
                key == VK_TAB && (::GetKeyState(VK_SHIFT) & 0x8000) != 0 ? -1 : 1);
            return true;
        }
        const auto before = this->config_.search_input.Editor().Text();
        this->config_.search_input.HandleKey(key, flags, window);
        if (before != this->config_.search_input.Editor().Text()) this->UpdateSettingsSearch();
        return true;
    }

    auto hud_runtime::HandleSettingsSearchController(const controller_action action, const bool repeated) -> bool
    {
        if (!this->config_.search_open) return false;
        if (action == controller_action::cancel) { if (!repeated) this->CloseSettingsSearch(); }
        else if (action == controller_action::confirm) { if (!repeated) this->SelectSettingsSearch(this->config_.search_selection); }
        else if (action == controller_action::up || action == controller_action::down) this->MoveSettingsSearch(action == controller_action::up ? -1 : 1);
        else if (action == controller_action::left || action == controller_action::right)
            this->MoveSettingsSearch((action == controller_action::left ? -1 : 1) * static_cast<int>(ui::settings_search_layout{this->ConfigLayout(), this->config_.bounds}.rows));
        else if (action == controller_action::actions && !repeated) this->ClearSettingsSearch();
        return true;
    }

    auto hud_runtime::SettingsSearchTarget(const D3DVIEWPORT8&) const -> uint64_t
    {
        if (!this->editing_ || this->config_.basic || this->config_.collapsed) return 0;
        const ui::settings_search_layout layout{this->ConfigLayout(), this->config_.bounds};
        const auto x = this->ConfigMouseX(), y = this->ConfigMouseY();
        const auto input = this->SettingsSearchBounds();
        if (this->config_.search_open && !this->config_.search_input.Editor().Text().empty() && input.width >= 70 &&
            Contains({input.x + input.width - 24, input.y, 24, input.height}, x, y)) return search_clear;
        if (Contains(input, x, y)) return search_input;
        if (!this->config_.search_open || !Contains(layout.panel, x, y)) return 0;
        for (size_t row = 0; row < layout.rows && row + this->config_.search_scroll < this->config_.search_matches.size(); ++row)
            if (Contains(layout.Row(row), x, y)) return search_result + row + this->config_.search_scroll;
        return search_panel;
    }

    auto hud_runtime::ProcessSettingsSearchClick(const D3DVIEWPORT8& viewport) -> bool
    {
        const auto target = this->SettingsSearchTarget(viewport);
        if (target == search_clear) { this->ClearSettingsSearch(); return true; }
        if (target == search_input) { this->BeginSettingsSearch(); return true; }
        if (!this->config_.search_open) return false;
        if (target >= search_result) this->SelectSettingsSearch(static_cast<size_t>(target - search_result));
        else if (target != search_panel) this->CloseSettingsSearch();
        return true;
    }

    auto hud_runtime::DrawSettingsSearch(const D3DVIEWPORT8&) -> void
    {
        const auto transform = this->design_system_.Transform(this->ConfigTransform());
        if (this->config_.basic || this->config_.collapsed) return;
        this->AnimateSettingsSearch(::GetTickCount64());
        const ui::settings_search_layout layout{this->ConfigLayout(), this->config_.bounds};
        const auto& theme = this->design_system_.GetTheme();
        auto text = theme.body_text; text.scale = 0.76f; text.alignment = ui::text_alignment::left;
        const auto input = this->SettingsSearchBounds();
        const bool hovered = Contains(input, this->ConfigMouseX(), this->ConfigMouseY());
        if (this->config_.search_expansion == 0)
        {
            this->config_controls_.IconButton(input, ui::icon_glyph::search, hovered);
            if (hovered) { this->config_.tooltip = "Search settings (Ctrl+F)"; this->config_.tooltip_anchor = input; }
        }
        else
        {
            const auto clip = this->design_system_.Clip(input);
            this->design_system_.FillRoundedRect(input, theme.colors.chrome, 3);
            this->design_system_.FillRoundedRect(input, 0x38000000, 3);
            this->design_system_.DrawIcon({input.x + 6, input.y + 5, 12, 12}, ui::icon_glyph::search, theme.colors.muted);
            const bool clear = this->config_.search_open && !this->config_.search_input.Editor().Text().empty() && input.width >= 70;
            const ui::rect field{input.x + 22, input.y, input.width - 22 - (clear ? 24 : 0), input.height};
            if (field.width >= 24)
            {
                const auto fieldClip = this->design_system_.Clip(field);
                this->config_controls_.TextInput(field, this->config_.search_input, false, i18n::Text("Search settings..."), i18n::Text("Ctrl+F"));
            }
            if (clear)
            {
                const ui::rect button{input.x + input.width - 24, input.y, 24, input.height};
                this->config_controls_.IconButton(button, ui::icon_glyph::close,
                    Contains(button, this->ConfigMouseX(), this->ConfigMouseY()));
            }
        }
        if (!this->config_.search_open)
        {
            if (this->config_.search_highlight_module != this->config_.module || ::GetTickCount64() >= this->config_.search_highlight_until) return;
            const auto slots = this->GroupOptionSlots();
            const auto found = std::find(slots.begin(), slots.end(), this->config_.search_highlight_option);
            if (found != slots.end())
            {
                const auto row = this->ConfigLayout().OptionRow(static_cast<int>(found - slots.begin()) - this->config_.scroll);
                const auto area = this->ConfigLayout().ContentArea();
                if (row.y >= area.y && row.y + row.height <= area.y + area.height)
                    this->design_system_.BorderRect(row, theme.colors.control_selected);
            }
            return;
        }
        this->config_.tooltip.clear();
        if (this->config_.search_expansion < 1) return;
        const ui::container_fill shade{layout.panel, 0x50000000};
        this->config_controls_.Panel(layout.panel, {&shade, 1});
        auto caption = theme.caption_text; caption.scale = 0.64f; caption.alignment = ui::text_alignment::left;
        if (this->config_.search_matches.empty())
            this->config_controls_.Text(this->config_.search_input.Editor().Text().empty() ? i18n::Text("Search by setting, category or feature. Try: party hp") : i18n::Text("No matching settings."), layout.Row(0), caption);
        for (size_t row = 0; row < layout.rows && row + this->config_.search_scroll < this->config_.search_matches.size(); ++row)
        {
            const auto index = row + this->config_.search_scroll;
            const auto& entry = this->config_.search_entries[this->config_.search_matches[index]];
            const auto bounds = layout.Row(row);
            this->config_controls_.NavigationItem(bounds, "", index == this->config_.search_selection,
                Contains(bounds, this->ConfigMouseX(), this->ConfigMouseY()));
            this->config_controls_.Text(entry.label.c_str(), {bounds.x + 8, bounds.y + 1, bounds.width - 16, 21}, text);
            this->config_controls_.Text(entry.path.c_str(), {bounds.x + 8, bounds.y + 23, bounds.width - 16, 17}, caption);
        }
        const auto total = this->config_.search_matches.size();
        const auto range = total == 0 ? std::string{i18n::Text("0 results")} : i18n::Format("%zu-%zu of %zu", this->config_.search_scroll + 1, std::min(total, this->config_.search_scroll + layout.rows), total);
        const auto footer = range + (this->ControllerPresented() ? i18n::Text(" | Directions: select | Confirm: open | Actions: clear | Cancel: close") :
            i18n::Text(" | Up/Down: select | Enter: open | Esc: close"));
        this->config_controls_.Text(footer.c_str(), layout.footer, caption);
    }
}
