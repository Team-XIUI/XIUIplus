#include "hud/hud_runtime.hpp"
#include "config/contributor_list.hpp"
#include "config/option_layout.hpp"
#include "config/config_window_resize.hpp"
#include "config/config_window_scale.hpp"
#include "input/keyboard_policy.hpp"
#include "ui/sound_choices.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>

namespace xiui
{
    auto hud_runtime::ConfigScale(void) const -> float
    { return ui::ConfigWindowScale(this->general_.MenuScale(), this->hud_viewport_); }

    auto hud_runtime::ConfigViewport(const D3DVIEWPORT8& viewport) const -> D3DVIEWPORT8
    { return ui::ConfigWindowViewport(viewport, this->ConfigTransform()); }

    auto hud_runtime::ChoiceViewport(const D3DVIEWPORT8& viewport) const -> D3DVIEWPORT8
    { return ui::ConfigWindowViewport(viewport, this->ChoiceTransform()); }

    auto hud_runtime::ToggleConfigSidebar(const D3DVIEWPORT8& viewport) -> void
    {
        this->FinishConfigInput(true, false);
        this->FinishConfigResize(false);
        this->CloseProfileControls();
        this->config_.active_color = nullptr; this->config_.active_slider = nullptr;
        const auto firstOption = this->GroupOptionIndex(static_cast<size_t>(std::max(0, this->config_.scroll)));
        this->config_.sidebar_collapsed = !this->config_.sidebar_collapsed;
        const auto layout = this->ConfigLayout();
        if (!this->config_.collapsed && viewport.Width != 0 && viewport.Height != 0)
            this->ApplyConfigResize(this->config_.bounds, ui::resize_bottom, 0, 0, viewport);
        if (!this->IsXIUIContributors())
        {
            const auto slots = this->GroupOptionSlots();
            const auto found = std::find(slots.begin(), slots.end(), firstOption);
            if (found != slots.end()) this->config_.scroll = static_cast<int>(found - slots.begin());
        }
        this->ClampOptionScroll();
        if (this->config_.focus.active) this->RefreshControllerConfigFocus();
        this->config_.tooltip.clear();
        this->SaveSettings();
    }

    auto hud_runtime::ConfigHeaderDragArea(void) const -> rect
    {
        const float searchWidth = this->config_.basic || this->config_.collapsed ? 0 : this->SettingsSearchBounds().width;
        return this->ConfigTransform().Map(this->ConfigLayout().HeaderDragArea(searchWidth));
    }

    auto hud_runtime::ConfigResizeEdges(const float screenX, const float screenY) const -> uint8_t
    {
        const auto transform = this->ConfigTransform();
        const float x = transform.InverseX(screenX), y = transform.InverseY(screenY);
        if (!this->editing_ || this->config_.collapsed || this->profile_name_.Focused() || this->profile_dropdown_ ||
            this->profile_actions_ || this->config_.choice_input.Opened() || this->config_.header_menu_open) return 0;
        const auto& bounds = this->config_.bounds;
        if (!Contains(bounds, x, y)) return 0;
        if (this->config_.active_color != nullptr && Contains(this->ConfigLayout().ColorEditor(this->ConfigViewport(this->config_.focus.viewport)), x, y)) return 0;
        if (Contains(this->ConfigLayout().ResizeGrip(), x, y)) return ui::resize_right | ui::resize_bottom;
        constexpr float edge = 5;
        uint8_t result{};
        if (x < bounds.x + edge) result |= ui::resize_left;
        else if (x >= bounds.x + bounds.width - edge) result |= ui::resize_right;
        if (y < bounds.y + edge) result |= ui::resize_top;
        else if (y >= bounds.y + bounds.height - edge) result |= ui::resize_bottom;
        return result;
    }

    auto hud_runtime::ApplyConfigResize(const rect& original, const uint8_t edges, const float dx, const float dy,
        const D3DVIEWPORT8& viewport) -> void
    {
        const auto layout = this->ConfigLayout();
        const auto before = this->config_.bounds;
        const auto firstOption = this->GroupOptionIndex(static_cast<size_t>(std::max(0, this->config_.scroll)));
        const auto scale = this->ConfigScale();
        const auto transform = ui::hud_transform::At(original.x, original.y, scale, scale);
        auto resized = ui::ResizeConfigWindow(original, edges, dx / scale, dy / scale, layout.MinimumWidth(), layout.MinimumHeight(), ui::ConfigWindowViewport(viewport, transform));
        resized.x = transform.X(resized.x); resized.y = transform.Y(resized.y);
        this->config_.bounds = resized;
        if (before.x == this->config_.bounds.x && before.y == this->config_.bounds.y &&
            before.width == this->config_.bounds.width && before.height == this->config_.bounds.height) return;
        if (layout.Columns() != this->ConfigLayout().Columns() && !this->IsXIUIContributors())
        {
            const auto slots = this->GroupOptionSlots();
            const auto found = std::find(slots.begin(), slots.end(), firstOption);
            if (found != slots.end()) this->config_.scroll = static_cast<int>(found - slots.begin());
        }
        this->ClampOptionScroll();
        if (this->config_.focus.active) this->RefreshControllerConfigFocus();
        this->config_.tooltip.clear();
    }

    auto hud_runtime::FocusModuleConfig(hud_module& module) -> void
    {
        if (const auto index = this->NativeWindowIndex(module); index < native_window_count)
        {
            this->FinishConfigInput(false);
            this->config_.module = index < native_hud_window_count ? static_cast<hud_module*>(&this->game_hud_) : static_cast<hud_module*>(&this->commands_);
            this->config_.section = index < native_hud_window_count ? 1 : static_cast<size_t>(native_menu_windows[index - native_hud_window_count].category);
            if (index < native_hud_window_count) this->game_hud_.SelectWindow(index);
            else this->commands_.SelectWindow(index - native_hud_window_count);
            this->config_.scroll = 0;
            this->config_.group = module_option_group::general;
            return;
        }
        this->config_.preview_selected = true;
        if (this->config_.basic) { this->config_.layout_selection = &module; return; }
        if (&module == &this->world_target_scale_) { this->FocusModuleConfig(this->target_); this->target_.SelectConfigView(3); this->config_.section = 1; return; }
        for (size_t i = 0; i < this->actionbar_children_.size(); ++i)
            if (&module == &this->actionbar_children_[i])
            {
                this->FocusModuleConfig(this->actionbars_);
                this->actionbars_.SelectConfigView(i + 1);
                return;
            }
        for (size_t i = 0; i < this->world_parts_.size(); ++i)
            if (&module == &this->world_parts_[i])
            {
                this->FocusModuleConfig(this->world_info_);
                this->world_info_.SelectPart(i);
                this->config_.section = 1;
                return;
            }
        if (&module == &this->notifications_) this->notifications_.SelectGroup(0);
        for (auto& group : this->notification_groups_)
            if (&module == &group)
            {
                this->FocusModuleConfig(this->notifications_);
                this->notifications_.SelectGroup(group.Index());
                this->config_.section = 2;
                return;
            }
        if (&module == &this->target_cast_)
        {
            this->FocusModuleConfig(this->target_);
            this->target_.SelectConfigView(0);
            this->config_.section = 9;
            return;
        }
        for (size_t i = 0; i < this->mob_info_groups_.size(); ++i)
            if (&module == &this->mob_info_groups_[i])
            {
                this->FocusModuleConfig(this->target_);
                this->target_.SelectConfigView(0);
                this->target_.Settings().mob_info.selected_group = static_cast<int>(i + 1);
                this->config_.section = 11;
                return;
            }
        if (!this->editing_ || (this->config_.module == &module && module.ConfigView() == 0)) return;
        this->FinishConfigInput(false);
        module.SelectConfigView(0);
        this->config_.module = &module;
        this->config_.section = 0;
        this->config_.group = module_option_group::general;
        this->config_.scroll = 0;
        this->config_.active_color = nullptr;
        this->config_.active_slider = nullptr;
    }

    auto hud_runtime::ConfigPages(void) -> std::array<config_page, config_page_count>
    {
        return {{{"general", "Global", &this->general_},
            {"hotbar", "Action Bars", &this->actionbars_, nullptr, nullptr, this->actionbar_members_, 1, "Hotbars", 0, action_bar_count},
            {"casting", "Casting", &this->cast_bar_, &this->cast_cost_, &this->recast_},
            {"chat", "Chat", &this->chat_, &this->chat_second_, &this->chat_bubbles_},
            {"enemylist", this->enemy_list_.DisplayName(), &this->enemy_list_},
            {"experience", this->experience_.DisplayName(), &this->experience_},
            {"inventory", "Inventory", &this->inventory_[0], &this->satchel_, &this->equipment_, this->inventory_config_members_, this->inventory_.size(), "Dots", 1},
            {"minimap", "Minimap", &this->minimap_},
            {"native-ui", "Native UI", &this->game_hud_, &this->commands_},
            {"notifications", this->notifications_.DisplayName(), &this->notifications_},
            {"party", "Party", &this->party_, &this->alliance_first_, &this->alliance_second_},
            {"pet", "Pet", &this->pet_, &this->pet_target_, nullptr, this->pet_config_members_},
            {"player", this->player_.DisplayName(), &this->player_},
            {"statuses", "Status Effects", &this->buffs_, &this->debuffs_},
            {"target", this->target_.DisplayName(), &this->target_, &this->target_recipient_, &this->subtarget_},
            {"extras", "Extras", &this->compass_, &this->world_info_, nullptr, this->extras_pages_}}};
    }

    auto hud_runtime::ConfigPageVisible(const config_page& page) const -> bool
    {
        if (this->config_.search_revealed_module == this->config_.module && page.Contains(this->config_.search_revealed_module)) return true;
        if (page.primary == &this->general_ || page.primary == &this->compass_ || page.primary == &this->game_hud_) return true;
        if (page.primary == &this->actionbars_)
        {
            for (size_t index = 0; index <= action_bar_count; ++index)
                if (this->actionbars_.BarState(index).enabled) return true;
            return false;
        }
        if (page.primary == &this->party_) return this->general_.PartyEnabled();
        if (page.primary == &this->target_) return general_module::Enabled(*page.primary) ||
            general_module::Enabled(*page.secondary) || general_module::Enabled(*page.tertiary);
        for (size_t index = 0; index < page.SectionCount(); ++index)
            if (auto* module = page.SectionModule(index); module != nullptr &&
                (module == &this->inventory_[0] ? this->general_.InventoryEnabled() : general_module::Enabled(*module))) return true;
        return false;
    }

    auto hud_runtime::VisibleConfigPages(void) const -> std::vector<config_page>
    {
        std::vector<config_page> visible;
        visible.reserve(config_page_count);
        for (const auto& page : const_cast<hud_runtime*>(this)->ConfigPages())
            if (this->ConfigPageVisible(page)) visible.push_back(page);
        return visible;
    }

    auto hud_runtime::ActiveConfigPage(void) -> config_page
    {
        for (const auto& page : this->ConfigPages())
            if (this->ConfigPageActive(page)) return page;
        return {};
    }

    auto hud_runtime::ConfigPageActive(const config_page& page) const -> bool
    {
        if (this->IsGeneralAppearance() && this->general_.AppearanceTab() >= 3)
            return page.primary == &this->compass_;
        return page.Contains(this->config_.module);
    }

    auto hud_runtime::SaveConfigSelection(const settings_store& store) const -> void
    {
        const auto* module = this->config_.basic && this->config_.advanced_module != nullptr ? this->config_.advanced_module : this->config_.module;
        const auto section = this->config_.basic ? this->config_.advanced_section : this->config_.section;
        store.WriteBool("session.config", "basic", this->config_.basic);
        store.WriteInt("session.config", "scroll", this->config_.basic ? static_cast<int>(this->config_.advanced_scroll) : this->config_.scroll);
        store.WriteString("session.config", "module", module != nullptr ? module->Id() : "general");
        store.WriteInt("session.config", "view", module != nullptr ? static_cast<int>(module->ConfigView()) : 0);
        store.WriteInt("session.config", "section", static_cast<int>(section));
        const auto sections = module != nullptr ? module->ConfigSections() : std::span<const module_config_section>{};
        store.WriteString("session.config", "topic_id", section < sections.size() ? std::string{sections[section].Id()} : "");
        store.WriteInt("session.config", "party_presets_general", 1);
        store.WriteInt("session.config", "party_presets_location", 2);
        store.WriteInt("session.config", "actionbar_keybinds_topic", 1);
        store.WriteInt("session.config", "appearance_tab", static_cast<int>(this->general_.AppearanceTab()));
    }

    auto hud_runtime::LoadConfigSelection(const settings_store& store) -> void
    {
        const bool basic = store.ReadBool("session.config", "basic", this->config_.basic);
        this->config_.basic = false;
        this->config_.module = &this->general_;
        const auto id = store.ReadString("session.config", "module", "general");
        for (auto* module : this->modules_.All())
            if (id == module->Id()) { this->config_.module = module; break; }
        const auto views = this->config_.module->ConfigViews();
        const auto view = static_cast<size_t>(std::max(0, store.ReadInt("session.config", "view", 0)));
        this->config_.module->SelectConfigView(views.empty() ? 0 : std::min(view, views.size() - 1));
        auto section = static_cast<size_t>(std::max(0, store.ReadInt("session.config", "section", 0)));
        auto topic = store.ReadString("session.config", "topic_id");
        if (topic.empty())
        {
            if (store.ReadInt("session.config", "party_presets_general", 0) == 0 &&
                (this->config_.module == &this->party_ || this->config_.module == &this->alliance_first_ || this->config_.module == &this->alliance_second_))
            {
                if (section == 12) { this->config_.module = &this->party_; section = 0; }
                else if (section == 13) section = 12;
            }
            if (this->config_.module == &this->general_ && section == 6 && store.ReadInt("session.config", "party_presets_location", 0) < 2)
            { this->config_.module = &this->party_; section = 0; }
            if (this->config_.module == &this->actionbars_ && store.ReadInt("session.config", "actionbar_keybinds_topic", 0) == 0)
            {
                const size_t firstMoved = this->actionbars_.ConfigView() == action_bar_count ? 2 : 1;
                if (section >= firstMoved) ++section;
            }
        }
        if (this->config_.module == &this->general_ && (topic == "game-hud" || (topic.empty() && section == general_module::game_hud_section)))
        { this->config_.module = &this->game_hud_; section = 0; topic = "general"; }
        const auto sections = this->config_.module->ConfigSections();
        if (!topic.empty())
        {
            const auto found = std::find_if(sections.begin(), sections.end(), [&](const auto& item) { return item.Id() == topic; });
            section = found != sections.end() ? static_cast<size_t>(found - sections.begin()) : 0;
        }
        this->config_.section = section < sections.size() ? section : 0;
        this->config_.scroll = std::max(0, store.ReadInt("session.config", "scroll", 0));
        this->general_.SelectAppearanceTab(static_cast<size_t>(std::max(0, store.ReadInt("session.config", "appearance_tab", 0))));
        this->NormalizeConfigSelection();
        this->ClampOptionScroll();
        this->SetBasicSettings(basic, false);
    }

    auto hud_runtime::SelectConfigTopic(const size_t index) -> void
    {
        if (this->config_.module == nullptr || index >= this->config_.module->ConfigSections().size()) return;
        const auto& section = this->config_.module->ConfigSections()[index];
        if (this->config_.module != &this->general_ && ui::AppearanceCategory(section) != nullptr)
            this->config_.appearance_topics[std::string{this->config_.module->Id()} + ":" + std::to_string(this->config_.module->ConfigView())] = section.Id();
        this->config_.preview_selected = true;
        this->config_.section = index;
        this->config_.active_color = nullptr;
        this->config_.scroll = 0;
        if (this->config_.module == &this->actionbars_ && this->actionbars_.OpenKeybindsSection(index))
            this->FinishActionBarsLaunch();
    }

    auto hud_runtime::ConfigTopicRows(void) const -> std::vector<ui::settings_topic_row>
    {
        if (this->config_.module == nullptr) return {};
        if (this->config_.module == &this->general_)
        {
            if (this->IsAttribution() || (this->IsGeneralAppearance() && this->general_.AppearanceTab() >= 3)) return {};
            std::vector<ui::settings_topic_row> rows;
            const auto sections = this->general_.ConfigSections();
            for (size_t section = 0; section < sections.size(); ++section)
            {
                if (section == general_module::attribution_section || section == general_module::game_hud_section || section == general_module::extra_settings_section) continue;
                const auto& topic = sections[section];
                const auto id = std::string{"general/"} + std::string{topic.Id()};
                const bool selected = this->config_.section == section;
                if (section == general_module::appearance_section)
                    for (size_t tab = 0; tab < 3; ++tab)
                        rows.push_back({id + "/" + std::to_string(tab), tab == 0 ? "Appearance" : general_module::appearance_tabs[tab],
                            tab == 2 ? "Configuration window scale, text styling and editing behavior." : topic.description,
                            section, false, false, selected && this->general_.AppearanceTab() == tab, static_cast<int>(tab)});
                else rows.push_back({id, topic.name, topic.description, section, false, false, selected});
            }
            return rows;
        }
        const auto key = std::string{this->config_.module->Id()} + ":" + std::to_string(this->config_.module->ConfigView());
        const auto remembered = this->config_.appearance_topics.find(key);
        return ui::SettingsTopics(this->config_.module->ConfigSections(), this->config_.section,
            remembered != this->config_.appearance_topics.end() ? remembered->second : std::string_view{}, this->config_.module != &this->general_);
    }

    auto hud_runtime::SelectConfigTopicRow(const size_t row) -> void
    {
        const auto topics = this->ConfigTopicRows();
        if (row >= topics.size()) return;
        this->SelectConfigTopic(topics[row].section);
        if (this->config_.module == &this->general_ && topics[row].subview >= 0)
            this->general_.SelectAppearanceTab(static_cast<size_t>(topics[row].subview));
    }

    auto hud_runtime::ConfigSidebarBounds(void) const -> rect
    {
        const auto layout = this->ConfigLayout();
        auto bounds = layout.ModuleTab(0);
        bounds.height = std::max(1.0f, layout.FooterBackground().y - bounds.y - 8);
        return bounds;
    }

    auto hud_runtime::ConfigSidebarRows(void) const -> std::vector<ui::settings_sidebar_row>
    {
        const auto pages = this->VisibleConfigPages();
        auto& animation = this->config_.sidebar_animation;
        const auto now = ::GetTickCount64();
        for (const auto& page : pages)
            if (this->ConfigPageActive(page)) { animation.Select(page.id, now); break; }
        const auto globalTopics = this->general_.ConfigSections();
        struct destination
        {
            std::string id;
            const char* label{};
            const char* help{};
            size_t section{};
            int subview{-1}, depth{1};
            float progress{1};
            bool selected{}, available{true};
        };
        const auto destinations = [&](const config_page& page) {
            std::vector<destination> result;
            if (page.SectionCount() > 1)
            {
                constexpr const char* chatNames[]{"Window 1", "Window 2", "Bubbles"};
                for (size_t section = 0; section < page.SectionCount(); ++section)
                {
                    const bool selected = page.primary == &this->actionbars_ && this->actionbars_.FloatingWindowOpen() && this->actionbars_.WindowFocused() ?
                        page.SectionView(section) == this->actionbars_.WindowConfigView() : page.SectionContains(section, this->config_.module);
                    const auto* label = std::strcmp(page.id, "chat") == 0 ? chatNames[section] : page.SectionName(section);
                    result.push_back({std::string{page.id} + "/" + std::to_string(section), label, label, section, -1, 1, 1, selected});
                }
                if (page.primary == &this->compass_)
                    for (size_t tab = 3; tab < std::size(general_module::appearance_tabs); ++tab)
                        result.push_back({std::string{page.id} + "/appearance/" + std::to_string(tab), general_module::appearance_tabs[tab],
                            globalTopics[general_module::appearance_section].description, general_module::appearance_section, static_cast<int>(tab), 1, 1,
                            this->IsGeneralAppearance() && this->general_.AppearanceTab() == tab});
            }
            return result;
        };
        const auto bounds = this->ConfigSidebarBounds();
        float total = static_cast<float>(pages.size()) * 30;
        for (const auto& page : pages)
            if (!this->config_.sidebar_collapsed)
                for (const auto& target : destinations(page)) total += 24 * target.progress * animation.Expansion(page.id, now);
        animation.content_height = total;
        animation.scroll = std::clamp(animation.scroll, 0.0f, std::max(0.0f, total - bounds.height));
        const float width = bounds.width - (total > bounds.height ? 10 : 0);
        float y = bounds.y - animation.scroll;
        std::vector<ui::settings_sidebar_row> rows;
        for (size_t at = 0; at < pages.size(); ++at)
        {
            const auto& page = pages[at];
            const bool active = this->ConfigPageActive(page);
            rows.push_back({page.id, page.name, {bounds.x, y, width, 28}, at, 0, at * 256, false, active});
            y += 30;
            const float expansion = this->config_.sidebar_collapsed ? 0 : animation.Expansion(page.id, now);
            if (expansion <= 0) continue;
            for (const auto& target : destinations(page))
            {
                const float progress = expansion * target.progress;
                const float indent = 12 * target.depth + 8 * (1 - progress);
                const size_t item = at * 256 + (target.subview < 0 ? target.section + 1 : 128 + target.section * 16 + static_cast<size_t>(target.subview));
                rows.push_back({target.id, target.label, {bounds.x + indent, y, width - indent, 22 * progress}, at, target.section, item,
                    true, active && target.selected, active && expansion >= .99f && target.available, target.subview, target.help});
                y += 24 * progress;
            }
        }
        if (animation.reveal)
        {
            const auto first = std::find_if(rows.begin(), rows.end(), [](const auto& row) { return !row.child && row.selected; });
            if (first != rows.end())
            {
                auto last = first;
                while (last + 1 != rows.end() && (last + 1)->child) ++last;
                const float delta = first->bounds.y < bounds.y ? first->bounds.y - bounds.y :
                    std::min(std::max(0.0f, last->bounds.y + last->bounds.height - bounds.y - bounds.height), std::max(0.0f, first->bounds.y - bounds.y));
                const float before = animation.scroll;
                animation.scroll = std::clamp(before + delta, 0.0f, std::max(0.0f, total - bounds.height));
                for (auto& row : rows) row.bounds.y -= animation.scroll - before;
                if (this->config_.sidebar_collapsed || animation.Expansion(animation.active, now) >= .9999f)
                    animation.reveal = false;
            }
        }
        return rows;
    }

    auto hud_runtime::SelectConfigSidebarItem(const size_t item) -> void
    {
        const auto rows = this->ConfigSidebarRows();
        const auto row = std::find_if(rows.begin(), rows.end(), [&](const auto& value) { return value.item == item && value.available; });
        if (row == rows.end()) return;
        const auto pages = this->VisibleConfigPages();
        const auto& page = pages[row->page];
        this->FinishConfigInput(true, false);
        if ((page.primary == &this->general_ || row->subview >= 0) && row->child)
        {
            this->SelectConfigModule(&this->general_);
            this->SelectConfigTopic(row->section);
            if (row->subview >= 0)
            {
                if (row->section == general_module::appearance_section) this->general_.SelectAppearanceTab(static_cast<size_t>(row->subview));
                else if (row->section == general_module::attribution_section) this->general_.SelectAttributionView(static_cast<size_t>(row->subview));
            }
        }
        else
        {
            this->SelectConfigModule(page.SectionModule(row->section), page.SectionView(row->section));
            if (page.primary == &this->general_ && this->IsGeneralAppearance() && this->general_.AppearanceTab() >= 3)
                this->general_.SelectAppearanceTab(0);
        }
        if (this->config_.sidebar_collapsed && page.SectionCount() > 1)
            this->ToggleConfigSidebar(this->config_.focus.viewport.Width != 0 ? this->config_.focus.viewport : this->hud_viewport_);
    }

    auto hud_runtime::SelectConfigModule(hud_module* module, const std::optional<size_t> view) -> void
    {
        this->config_.preview_selected = true;
        const auto previousTopics = this->config_.module != nullptr ? this->config_.module->ConfigSections() : std::span<const module_config_section>{};
        const auto previousTopic = this->config_.section < previousTopics.size() ? std::string{previousTopics[this->config_.section].Id()} : std::string{};
        if (view && !module->ConfigViews().empty()) module->SelectConfigView(*view);
        this->config_.module = module;
        const auto nextTopics = module->ConfigSections();
        this->config_.section = 0;
        for (size_t topic = 0; !previousTopic.empty() && topic < nextTopics.size(); ++topic)
            if (previousTopic == nextTopics[topic].Id()) { this->config_.section = topic; break; }
        this->config_.active_color = nullptr; this->config_.active_slider = nullptr;
        this->config_.group = module_option_group::general;
        this->config_.scroll = 0;
        if (module == &this->actionbars_ && view && *view > action_bar_count && *view <= action_bar_count + 3)
            this->FinishActionBarsLaunch();
    }

    auto hud_runtime::ConfigCategories(void) const -> config_categories
    {
        config_categories result{};
        if (this->config_.module == nullptr || this->HasConfigSubmenu()) return result;
        std::array<bool, 5> populated{};
        for (size_t index = 0; index < this->config_.module->OptionCount(); ++index)
        {
            const auto option = this->config_.module->Option(index);
            if (!option.visible) continue;
            const auto group = static_cast<size_t>(option.group);
            if (group < populated.size()) populated[group] = true;
        }
        for (size_t group = 0; group < populated.size(); ++group)
            if (populated[group]) result.groups[result.count++] = static_cast<module_option_group>(group);
        return result;
    }

    auto hud_runtime::NormalizeConfigSelection(void) -> void
    {
        if (this->config_.module == &this->general_ && this->config_.section == general_module::extra_settings_section)
        { this->config_.section = general_module::appearance_section; this->general_.SelectAppearanceTab(2); }
        if (this->config_.module == &this->general_ && this->config_.section == general_module::game_hud_section)
        { this->config_.module = &this->game_hud_; this->config_.section = 0; }
        if (this->module_menu_target_ != nullptr && !this->ModuleMenuAvailable(*this->module_menu_target_)) this->CloseChoiceInput();
        if (this->config_.module != nullptr)
        {
            auto* target = this->MovementSelectionModule();
            this->config_.module->BindConfigScale(target != nullptr ? &target->State() : nullptr, target != nullptr && target->HasEditableBounds());
        }
        if (this->config_.basic) return;
        if ((this->config_.active_color != nullptr || this->config_.active_slider != nullptr) && this->config_.module != nullptr && this->config_.option < this->config_.module->OptionCount() &&
            !this->config_.module->Option(this->config_.option).visible)
        {
            this->config_.active_color = nullptr;
            this->config_.color_picker.EndDrag();
            this->config_.active_slider = nullptr;
            if (this->interaction_ == interaction::option_slider || this->interaction_ == interaction::color_pick) this->FinishInteraction();
        }
        if (!this->ConfigPageVisible(this->ActiveConfigPage()))
        {
            this->FinishConfigInput(false);
            this->config_.module = &this->general_;
            this->config_.section = general_module::enabled_modules_section;
            this->config_.group = module_option_group::general;
            this->config_.active_color = nullptr;
            this->config_.active_slider = nullptr;
            this->config_.scroll = 0;
        }
        if (this->module_menu_target_ == nullptr && this->config_.choice_input.Opened() && (this->config_.module == nullptr || this->config_.choice_module != this->config_.module ||
            this->config_.choice_option >= this->config_.module->OptionCount() || !this->MatchesConfigOption(this->config_.choice_option) ||
            !this->config_.module->Option(this->config_.choice_option).available ||
            this->config_.module->Option(this->config_.choice_option).choice != this->config_.choice_input.Option().choice)) this->CloseChoiceInput();
        if (this->config_.text_input.Focused() && (this->config_.module == nullptr || this->config_.text_module != this->config_.module ||
            this->config_.text_option >= this->config_.module->OptionCount() || !this->MatchesConfigOption(this->config_.text_option) ||
            !this->config_.module->Option(this->config_.text_option).available ||
            this->config_.module->Option(this->config_.text_option).text != this->config_.text_value)) this->FinishConfigInput(false);
        if (this->config_.slider_input.Focused() && (this->config_.module == nullptr || this->config_.slider_module != this->config_.module ||
            this->config_.slider_option >= this->config_.module->OptionCount() ||
            !this->MatchesConfigOption(this->config_.slider_option) ||
            !this->config_.module->Option(this->config_.slider_option).available ||
            this->config_.module->Option(this->config_.slider_option).slider != this->config_.slider_input.Option().slider)) this->FinishConfigInput(false);
        if (this->HasConfigSubmenu())
        {
            if (this->config_.section < this->config_.module->ConfigSections().size()) return;
            this->config_.section = 0;
        }
        else
        {
            const auto categories = this->ConfigCategories();
            if (categories.count == 0 || std::find(categories.groups.begin(), categories.groups.begin() + categories.count, this->config_.group) != categories.groups.begin() + categories.count)
                return;
            this->config_.group = categories.groups[0];
        }
        this->config_.active_color = nullptr;
        this->config_.active_slider = nullptr;
        this->config_.scroll = 0;
    }

    auto hud_runtime::MatchesConfigOption(const size_t index) const -> bool
    {
        if (this->config_.basic) return this->config_.module == &this->general_ && index >= this->general_.HudScaleStart() && index < this->general_.OptionCount();
        if (this->config_.module == nullptr || index >= this->config_.module->OptionCount() || !this->config_.module->Option(index).visible) return false;
        if (index >= this->config_.module->ContentOptionCount())
        {
            if (this->config_.module == &this->target_ && this->target_.ConfigView() == 4) return false;
            auto* target = this->MovementSelectionModule();
            if (target == nullptr || !target->SupportsScale()) return false;
            this->config_.module->BindConfigScale(target != nullptr ? &target->State() : nullptr, target != nullptr && target->HasEditableBounds());
            if (target != nullptr && target != this->config_.module && this->config_.module == &this->target_ && this->target_.ConfigView() == 0)
                return this->config_.section == 9 || this->config_.section == 11;
        }
        if (index >= this->config_.module->ContentOptionCount() && this->IsExperienceDisplay()) return false;
        if (this->IsExperienceDisplay()) return this->experience_.DisplayOption(this->experience_display_.View(), index);
        if (this->IsAttribution()) return this->general_.AttributionOption(index);
        if (this->IsGeneralAppearance()) return this->general_.AppearanceOption(index);
        return this->HasConfigSubmenu() ? this->config_.module->ConfigSectionContains(this->config_.section, index) :
            this->config_.module->Option(index).group == this->config_.group;
    }

    auto hud_runtime::GroupOptionSlots(void) const -> std::vector<size_t>
    {
        if (this->config_.module == nullptr) return {};
        std::vector<ui::config_option_entry> entries;
        for (size_t index = 0; index < this->config_.module->OptionCount(); ++index)
        {
            if (!this->MatchesConfigOption(index)) continue;
            const auto option = this->config_.module->Option(index);
            if (option.selector == module_selector::none) entries.push_back({index, option.display_order, option.pair_next});
        }
        std::vector<ui::config_option_entry> cards;
        std::erase_if(entries, [&](const auto& entry) {
            if (!this->config_.module->Option(entry.index).card) return false;
            cards.push_back(entry); return true;
        });
        const auto empty = this->config_.module->OptionCount();
        const int columns = ui::config_layout{this->config_.bounds, 0, false, !this->ConfigTopicRows().empty(), 0, 0, false, -1, 0, this->config_.sidebar_collapsed}.Columns();
        auto slots = ui::ArrangeConfigOptions(std::move(entries), empty, columns);
        if (this->config_.module == &this->general_)
        {
            const auto follow = std::find(slots.begin(), slots.end(), this->general_.FollowGameWindowTypeOption());
            if (follow != slots.end() && std::find(slots.begin(), slots.end(), general_module::window_theme_option) != slots.end())
            {
                slots.erase(follow);
                const auto below = static_cast<size_t>(std::find(slots.begin(), slots.end(), general_module::window_theme_option) - slots.begin()) + columns;
                if (below > slots.size()) slots.resize(below, empty);
                slots.insert(slots.begin() + below, this->general_.FollowGameWindowTypeOption());
            }
        }
        if (!cards.empty() && slots.size() % columns != 0) slots.push_back(empty);
        for (const auto& card : cards) slots.push_back(card.index);
        return slots;
    }

    auto hud_runtime::GroupOptionCount(void) const -> size_t
    {
        return this->GroupOptionSlots().size();
    }

    auto hud_runtime::GroupOptionIndex(const size_t filteredIndex) const -> size_t
    {
        if (this->config_.module == nullptr)
            return 0;
        const auto slots = this->GroupOptionSlots();
        return filteredIndex < slots.size() ? slots[filteredIndex] : this->config_.module->OptionCount();
    }

    auto hud_runtime::ClampOptionScroll(void) -> void
    {
        if (this->IsXIUIContributors())
        {
            this->config_.scroll = std::clamp(this->config_.scroll, 0, ui::contributor_list_layout{this->ConfigLayout()}.MaximumScroll());
            return;
        }
        this->config_.scroll = this->ConfigLayout().AlignScroll(this->config_.scroll, static_cast<int>(this->GroupOptionCount()));
    }
}
