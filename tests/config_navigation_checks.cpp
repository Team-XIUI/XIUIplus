#include "config_input_fixture.hpp"
#include "config/config_window_resize.hpp"

namespace xiui
{
    using namespace config_checks;

    auto config_input_test::NativeUi(void) -> bool
    {
        hud_runtime runtime;
        const D3DVIEWPORT8 viewport{0, 0, 1920, 1080, 0, 1};
        runtime.hud_viewport_ = runtime.config_.focus.viewport = viewport;
        runtime.editing_ = runtime.config_.edit_mode = true;
        runtime.config_.bounds = {600, 50, 1100, 600};
        runtime.general_.grid_.snap_elements = false;
        runtime.game_hud_.SetAvailable((uint32_t{1} << native_hud_window_count) - 1);
        runtime.native_ui_.menu_available = true;
        runtime.native_ui_.reference_width = 1920; runtime.native_ui_.reference_height = 1080;
        runtime.native_ui_.windows[0] = {true, true, 50, 300, 200, 80, true};
        runtime.game_hud_.SetWindowShown(1, false);
        for (size_t index = native_hud_window_count; index < native_window_count; ++index) runtime.native_ui_.windows[index].movable = true;
        runtime.UpdateNativeWindows(viewport);
        const auto pages = runtime.ConfigPages();
        const auto native = std::find_if(pages.begin(), pages.end(), [](const auto& page) { return std::strcmp(page.id, "native-ui") == 0; });
        const auto extras = std::find_if(pages.begin(), pages.end(), [](const auto& page) { return std::strcmp(page.id, "extras") == 0; });
        if (!check(native != pages.end() && native->Contains(&runtime.game_hud_) && native->Contains(&runtime.commands_) &&
            extras != pages.end() && !extras->Contains(&runtime.game_hud_) && !extras->Contains(&runtime.commands_),
            "native HUD and dialog controls have one main Native UI category")) return false;
        const auto player = runtime.player_.State().bounds;
        auto& window = runtime.native_windows_[0];
        const auto click = [&](const ui::rect& bounds) {
            runtime.mouse_x_ = static_cast<int>(bounds.x + bounds.width * .5f); runtime.mouse_y_ = static_cast<int>(bounds.y + bounds.height * .5f);
            const bool down = runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
            return runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport) && down;
        };
        const auto switchBounds = runtime.EditModeSwitchBounds(viewport);
        if (!check(switchBounds.width == 28 && switchBounds.height == 60 && switchBounds.x + switchBounds.width == viewport.Width &&
            !runtime.ModuleDisplayed(window) && click(runtime.EditModeSwitchItemBounds(viewport, true)) && runtime.NativeEditMode() &&
            runtime.ModuleDisplayed(window) && !runtime.ModuleDisplayed(runtime.player_) && runtime.config_.module == &runtime.game_hud_,
            "native edit switch owns its pointer gesture and isolates the two layout surfaces")) return false;
        if (!check((runtime.NativeHudHiddenMask() & NativeHudBit(native_hud_element::party)) == 0 && !runtime.game_hud_.Shown(1),
            "native editing reveals the real party renderer without changing its saved visibility")) return false;
        const auto selectedAt = runtime.config_.native_edit_slide.started;
        if (!check(click(runtime.EditModeSwitchItemBounds(viewport, true)) && runtime.config_.native_edit_slide.started == selectedAt &&
            click(runtime.EditModeSwitchItemBounds(viewport, false)) && !runtime.NativeEditMode() &&
            click(runtime.EditModeSwitchItemBounds(viewport, true)) && runtime.NativeEditMode() &&
            runtime.config_.native_edit_slide.Value(runtime.config_.native_edit_slide.started + 160) == 1,
            "the two edge icons select their own mode and the highlight slides to the selected icon")) return false;
        runtime.UpdateNativeWindows(viewport);
        runtime.native_windows_.back().State().bounds = {0, 0, 1920, 1080};
        if (!check(runtime.NativeWindowAt(60, 340) == &window, "smaller native outlines stay selectable over large overlapping dialog groups")) return false;
        runtime.mouse_x_ = 60; runtime.mouse_y_ = 340;
        runtime.BeginPanelInteraction(viewport); runtime.mouse_down_ = true;
        runtime.mouse_x_ += 70; runtime.mouse_y_ += 20; runtime.UpdateInteraction(viewport);
        runtime.UpdateNativeWindows(viewport);
        if (!check(runtime.interaction_module_ == &window && window.State().bounds.x == 120 && window.State().bounds.y == 320 &&
            runtime.MenuLayout().windows[0].enabled && runtime.MenuLayout().windows[0].x == 120,
            "native drags survive fresh game snapshots and publish their own position")) return false;
        const auto pendingNative = runtime.CaptureSettings();
        if (!check(pendingNative.ReadString((std::string{"layout."} + window.Id()).c_str(), "x").empty() &&
            pendingNative.ReadFloat(window.Id(), "x", 0) == 50 && pendingNative.ReadFloat(window.Id(), "y", 0) == 300,
            "autosaving a native drag keeps its committed position without creating a layout section")) return false;
        runtime.mouse_down_ = false; runtime.FinishInteraction();
        if (!check(window.Placement().enabled && runtime.player_.State().bounds.x == player.x && runtime.player_.State().bounds.y == player.y,
            "native placement commits without moving the XIUI replacement")) return false;
        const bool shown = runtime.game_hud_.Shown(0);
        if (!check(click(runtime.NativeVisibilityButton(window)) && runtime.game_hud_.Shown(0) != shown && runtime.ModuleDisplayed(window),
            "hiding a native HUD window keeps its editor outline available for restoring it")) return false;
        const auto saved = runtime.CaptureSettings(); hud_runtime restored; restored.LoadSettings(settings_store::FromIniText(saved.ToIniText()));
        if (!check(restored.native_windows_[0].Placement().enabled && restored.native_windows_[0].Placement().x == 120 &&
            restored.game_hud_.Shown(0) == runtime.game_hud_.Shown(0) && !restored.config_.native_edit,
            "native positions and hides persist while editor mode stays transient")) return false;
        window.OnOptionChanged(4);
        if (!check(!window.Placement().enabled && runtime.game_hud_.Shown(0) != shown,
            "resetting native placement keeps visibility choices")) return false;
        runtime.native_ui_.windows[0].live = false; runtime.UpdateNativeWindows(viewport);
        if (!check(window.PositionKnown() && window.State().bounds.x == 50 && window.State().bounds.y == 300 && window.State().bounds.width == 200 &&
            restored.native_windows_[0].HasGeometry(), "closed native windows retain captured geometry and persist it for future previews")) return false;
        const auto merchant = native_hud_window_count + static_cast<size_t>(native_dialog::merchant);
        if (!check(!runtime.native_windows_[merchant].PositionKnown() && !runtime.native_windows_[merchant].CanMove() &&
            !runtime.ModuleDisplayed(runtime.native_windows_[merchant]) && runtime.native_windows_[merchant].State().bounds.width == 0 &&
            !runtime.MenuLayout().windows[merchant].enabled, "unopened menus have no fabricated editor frame or position")) return false;
        runtime.native_ui_.windows[merchant] = {true, true, 420, 460, 300, 200, true}; runtime.UpdateNativeWindows(viewport);
        if (!check(runtime.native_windows_[merchant].PositionKnown() && runtime.native_windows_[merchant].State().bounds.x == 420 &&
            runtime.native_windows_[merchant].State().bounds.y == 460 && runtime.ModuleDisplayed(runtime.native_windows_[merchant]),
            "only client-rendered menus receive an editor outline at their real position")) return false;
        const auto emotes = native_hud_window_count + NativeMenuWindowIndex("evitem  ", 2);
        const bool playerHidden = runtime.NativeWindowHidden(0);
        runtime.native_ui_.windows[emotes] = {true, true, 1400, 300, 260, 200, true}; runtime.UpdateNativeWindows(viewport);
        runtime.ToggleNativeWindowVisibility(emotes); runtime.UpdateNativeWindows(viewport);
        if (!check(emotes >= 64 && runtime.NativeWindowHidden(emotes) && runtime.MenuLayout().windows[emotes].hidden &&
            runtime.NativeWindowHidden(0) == playerHidden, "submenu visibility beyond 64 windows stays independent of HUD visibility")) return false;
        runtime.ToggleNativeWindowVisibility(emotes);
        runtime.commands_.SelectSection(static_cast<size_t>(native_dialog::status));
        auto selector = runtime.commands_.ContentOption(0);
        const auto selectNative = [&](const char* label) {
            for (size_t choice = 0; choice < selector.choice_count; ++choice)
                if (std::strcmp(selector.ChoiceText(choice), label) == 0) { *selector.choice = static_cast<int>(choice); return runtime.commands_.SelectedWindow(); }
            return static_cast<native_window_module*>(nullptr);
        };
        auto* statusSelector = selectNative("Status Menu");
        auto* statusResult = selectNative("Character Status");
        auto* profileResult = selectNative("Profile");
        auto* combatResult = selectNative("Combat Skills");
        auto* currencyResult = selectNative("Currencies 2");
        if (!check(statusSelector && statusResult && profileResult && combatResult && currencyResult && statusSelector != statusResult &&
            statusResult != profileResult && profileResult != combatResult && currencyResult != combatResult,
            "Status navigation selects the submenu and distinct result windows instead of a category-sized target")) return false;
        runtime.FocusModuleConfig(*combatResult);
        if (!check(runtime.config_.section == static_cast<size_t>(native_dialog::status) && runtime.commands_.SelectedWindow() == combatResult,
            "clicking an individual native result focuses its exact settings and shared movement controls")) return false;
        restored.LoadSettings(runtime.CaptureSettings()); restored.commands_.SelectSection(static_cast<size_t>(native_dialog::status));
        if (!check(std::strcmp(restored.commands_.SelectedWindow()->Id(), combatResult->Id()) == 0,
            "the selected individual window survives settings persistence")) return false;
        runtime.SelectConfigModule(&runtime.game_hud_); runtime.game_hud_.SelectWindow(0);

        const auto obsolete = settings_store::FromIniText("[commands]\nenabled=1\nposition_initialized=1\nwindow_anchor=1\n[layout.commands]\nx=300\ny=500\nwidth=434\nheight=200\n");
        for (const char* resource : {"playermo", "magic   ", "iteminfo", "query   ", "guildsho"})
        {
            native_window_module independent; independent.Initialize(native_hud_window_count + NativeMenuWindowIndex(resource)); independent.Load(obsolete);
            independent.Update({true, true, 16, 48, 320, 200, true}, 1, 1, 1920, 1080);
            if (!check(!independent.Placement().enabled && independent.Placement().x == 16 && independent.Placement().y == 48,
                "obsolete shared-anchor settings never create a native window placement")) return false;
        }
        runtime.SetNativeEditMode(false);
        if (!check((runtime.NativeHudHiddenMask() & NativeHudBit(native_hud_element::party)) != 0, "leaving native editing restores saved native visibility")) return false;
        runtime.native_ui_.windows[0].live = true; runtime.native_ui_.windows[0].visible = true;
        runtime.config_.focus.active = true; runtime.config_.focus.region = config_controller_region::utilities; runtime.config_.focus.id = "edit-native";
        runtime.HandleControllerConfig(controller_action::confirm, true);
        if (!check(!runtime.NativeEditMode(), "held controller Confirm cannot switch editors")) return false;
        runtime.HandleControllerConfig(controller_action::confirm, false);
        if (!check(runtime.NativeEditMode(), "fresh controller Confirm uses the same editor switch")) return false;
        runtime.ToggleNativeWindowVisibility(0); runtime.UpdateNativeWindows(viewport);
        runtime.config_.focus.id = "native-visibility";
        runtime.HandleControllerConfig(controller_action::confirm, false);
        if (!check(runtime.game_hud_.Shown(0) == shown, "controller can show the selected native window through the shared action")) return false;
        runtime.SelectConfigModule(&runtime.commands_); runtime.SelectConfigTopic(static_cast<size_t>(native_dialog::merchant));
        const auto commandTopics = runtime.ConfigTopicRows();
        runtime.GroupOptionSlots();
        if (!check(commandTopics.size() == native_dialog_labels.size() && runtime.commands_.SelectedWindow() == &runtime.native_windows_[native_hud_window_count + 3] &&
            runtime.game_hud_.ConfigSectionContains(2, game_hud_module::instant_macro_option) &&
            !runtime.general_.ConfigSectionContains(general_module::visibility_section, runtime.general_.NameplatesOption()),
            "dialog groups, native macros and overhead names belong to the Native UI settings")) return false;
        runtime.BuildSettingsSearch();
        const auto result = std::find_if(runtime.config_.search_entries.begin(), runtime.config_.search_entries.end(), [](const auto& entry) {
            return entry.module == "gamehud" && entry.label == "Position X" && entry.path.find("Party List") != std::string::npos;
        });
        if (!check(result != runtime.config_.search_entries.end() && !result->selectors.empty() && result->path.starts_with("Native UI"),
            "native placement search preserves the selected window and its main category")) return false;
        restored.LoadSettings(runtime.CaptureSettings());
        const auto unopened = native_hud_window_count + NativeMenuWindowIndex("joblevel");
        for (size_t index = 0; index < native_window_count; ++index)
            if (index != unopened) restored.native_windows_[index].OnPositionChanged();
        restored.game_hud_.SetAvailable((uint32_t{1} << native_hud_window_count) - 1);
        restored.game_hud_.SetWindowShown(1, false); restored.native_windows_[emotes].SetHidden(true);
        const auto xiuiPosition = restored.player_.State().bounds;
        const auto reset = game_hud_module::reset_positions_option;
        if (!check(restored.game_hud_.ConfigSectionContains(1, reset) && !restored.game_hud_.ConfigSectionContains(0, reset) &&
            restored.game_hud_.Option(reset).kind == module_option_kind::action && restored.HandleCommand("nativeui reset"),
            "Window Placement and nativeui reset share the reset action covering unopened windows")) return false;
        if (!check(std::none_of(restored.native_windows_.begin(), restored.native_windows_.end(), [](const auto& item) { return item.Placement().enabled; }) &&
            !restored.game_hud_.Shown(1) && restored.native_windows_[emotes].Hidden() &&
            restored.player_.State().bounds.x == xiuiPosition.x && restored.player_.State().bounds.y == xiuiPosition.y,
            "reset all clears every native HUD and dialog override while retaining visibility and XIUI positions")) return false;
        const auto resetSaved = restored.CaptureSettings(); restored.LoadSettings(resetSaved);
        restored.native_windows_[unopened].Update({true, true, 130, 48, 252, 200, true}, 1, 1, 1920, 1080);
        if (!check(std::none_of(restored.native_windows_.begin(), restored.native_windows_.end(), [](const auto& item) { return item.Placement().enabled; }) &&
            restored.native_windows_[unopened].Placement().x == 130 && restored.native_windows_[unopened].Placement().y == 48 &&
            !restored.game_hud_.Shown(1) && restored.native_windows_[emotes].Hidden(),
            "bulk reset persists and opening a previously unseen window keeps its native position")) return false;
        runtime.editing_ = false;
        return true;
    }

    auto config_input_test::ConfigMemory(void) -> bool
    {
        class mouse final : public IMouse
        {
            bool blocked_{};
        public:
            void AddCallback(const char*, mousecallback_f) override {}
            void RemoveCallback(const char*) override {}
            bool GetBlockInput(void) const override { return this->blocked_; }
            void SetBlockInput(bool value) override { this->blocked_ = value; }
        } input;
        hud_runtime runtime;
        runtime.mouse_ = &input; runtime.SetEditing(true, false);
        runtime.config_.bounds = {320, 180, 1100, 600};
        const auto section = static_cast<size_t>(bar_config_section::text);
        runtime.SelectConfigModule(&runtime.player_); runtime.config_.section = section;
        runtime.config_.scroll = 2; runtime.ClampOptionScroll();
        const int scroll = runtime.config_.scroll;
        if (!check(scroll > 0, "memory fixture has a scrollable settings page")) return false;
        for (const bool basic : {false, true})
        {
            runtime.SetBasicSettings(basic, false);
            runtime.SetEditing(false, false); runtime.SetEditing(true, false);
            if (!check(runtime.config_.basic == basic && runtime.config_.bounds.x == 320 && runtime.config_.bounds.y == 180,
                "close and reopen retain the chosen settings mode and window placement")) return false;
            const auto saved = runtime.CaptureSettings();
            hud_runtime restored;
            restored.LoadSettings(settings_store::FromIniText(saved.ToIniText()));
            if (!check(restored.config_.basic == basic && restored.config_.bounds.x == 320 && restored.config_.bounds.y == 180,
                "saved mode and placement survive a fresh runtime")) return false;
            restored.SetBasicSettings(false, false);
            if (!check(restored.config_.module == &restored.player_ && restored.config_.section == section && restored.config_.scroll == scroll,
                "the last Advanced page and scroll survive both modes and a fresh runtime")) return false;
            const auto selection = settings_store::FromIniText(""); runtime.SaveConfigSelection(selection);
            runtime.SetBasicSettings(!basic, false); runtime.LoadConfigSelection(selection);
            if (!check(runtime.config_.basic == basic, "navigation-only restoration retains the mode with its page")) return false;
        }
        runtime.SetEditing(false, false);
        return true;
    }

    auto config_input_test::SettingsPresentation(const char* reportPath) -> bool
    {
        ui::settings_sidebar_animation animation;
        animation.Select("extras", 100);
        const auto halfway = animation.Expansion("extras", 180);
        if (!check(animation.Expansion("extras", 100) == 0 && halfway > 0 && halfway < 1 && animation.Expansion("extras", 260) == 1,
            "sidebar submenus expand over time")) return false;
        animation.Select("inventory", 180);
        if (!check(animation.Expansion("extras", 180) == halfway && animation.Expansion("extras", 340) == 0 && animation.Expansion("inventory", 340) == 1,
            "changing categories reverses an in-progress expansion without jumping")) return false;
        hud_runtime runtime;
        runtime.editing_ = true;
        runtime.config_.bounds = {20, 20, 1100, 600};
        for (auto* module : runtime.modules_.All()) module->Option(0);
        const auto originalSettings = runtime.CaptureSettings().Preferences("").ToIniText();
        std::ostringstream report;
        SYSTEMTIME now{}; ::GetLocalTime(&now);
        char date[16]{};
        std::snprintf(date, sizeof(date), "%04u-%02u-%02u", now.wYear, now.wMonth, now.wDay);
        report << "# Settings inventory\n\nGenerated " << date << ". Source: runtime option metadata and shared layout.\n"
            << "Regenerate with `./scripts/update-docs.ps1`. Each bullet is one visual row; [empty] preserves paired controls.\n\n";
        size_t pages{}, controls{}, variants{};
        const auto audit = [&](const std::string& title, std::set<std::string>& seen) {
            const auto slots = runtime.GroupOptionSlots();
            std::set<size_t> covered;
            std::ostringstream rows;
            for (size_t at = 0; at < slots.size(); ++at)
            {
                const auto index = slots[at];
                if (at % 2 == 0) rows << "- "; else rows << " | ";
                if (index >= runtime.config_.module->OptionCount()) rows << "[empty]";
                else
                {
                    if (!check(covered.insert(index).second, "presentation never duplicates an option")) return false;
                    const auto option = runtime.config_.module->Option(index);
                    if (!check(runtime.MatchesConfigOption(index) && option.selector == module_selector::none,
                        "presentation retains original IDs and current section membership")) return false;
                    rows << option.label;
                    if (option.pair_next && at + 1 < slots.size() && slots[at + 1] < runtime.config_.module->OptionCount() &&
                        runtime.config_.module->Option(slots[at + 1]).display_order == option.display_order + 1)
                        if (!check(at % 2 == 0, "related pairs share the left and right cells of one row")) return false;
                }
                if (at % 2 == 1 || at + 1 == slots.size()) rows << '\n';
            }
            for (size_t index = 0; index < runtime.config_.module->OptionCount(); ++index)
                if (runtime.MatchesConfigOption(index) && runtime.config_.module->Option(index).selector == module_selector::none)
                    if (!check(covered.contains(index), "presentation does not hide existing settings")) return false;
            if (seen.insert(rows.str()).second)
            {
                report << "### " << title << "\n\n" << rows.str() << '\n';
                ++pages; controls += covered.size();
            }
            ++variants;
            return true;
        };
        for (auto* module : runtime.modules_.All())
        {
            if (runtime.NativeWindowIndex(*module) < native_window_count) continue;
            runtime.config_.module = module;
            const auto originalView = module->ConfigView();
            for (size_t view = 0; view < std::max<size_t>(1, module->ConfigViews().size()); ++view)
            {
                module->SelectConfigView(view);
                const auto sections = module->ConfigSections();
                for (size_t selected = 0; module != &runtime.general_ && module != &runtime.game_hud_ && selected < sections.size(); ++selected)
                {
                    runtime.config_.section = selected;
                    const auto topics = runtime.ConfigTopicRows();
                    std::set<std::string> ids;
                    bool reachable{};
                    size_t appearanceParents{};
                    for (const auto& topic : topics)
                    {
                        if (!check(ids.insert(topic.id).second, "settings navigation has stable unique topic identities")) return false;
                        reachable |= !topic.parent && topic.section == selected;
                        appearanceParents += topic.id == "appearance";
                        if (module != &runtime.general_ && ui::AppearanceCategory(sections[topic.section]) != nullptr)
                            if (!check(topic.child || topic.id == "appearance", "visual categories appear only inside Appearance")) return false;
                    }
                    if (!check(reachable && appearanceParents <= 1, "every original topic remains reachable under one Appearance parent")) return false;
                }
                report << "## " << module->DisplayName();
                if (!module->ConfigViews().empty()) report << " / " << module->ConfigViews()[view];
                report << "\n\n";
                std::vector<size_t> sectionOrder;
                size_t expanded{};
                for (size_t index = 0; index < sections.size(); ++index)
                    if (ui::AppearanceCategory(sections[index]) != nullptr) { expanded = index; break; }
                for (const auto& topic : ui::SettingsTopics(sections, expanded, {}, module != &runtime.general_))
                    if (!topic.parent) sectionOrder.push_back(topic.section);
                if (sectionOrder.empty()) sectionOrder.push_back(0);
                for (const size_t section : sectionOrder)
                {
                    if (module == &runtime.general_ && (section == general_module::game_hud_section || section == general_module::extra_settings_section)) continue;
                    runtime.config_.section = section;
                    runtime.config_.group = module_option_group::general;
                    const auto title = module->ConfigSections().empty() ? std::string{"General"} : ui::SettingsTopicPath(module->ConfigSections(), section, module != &runtime.general_);
                    std::set<std::string> seen;
                    const auto run = [&]() {
                        static constexpr const char* displayTabs[]{"Bar info", "Readouts"};
                        const auto pageTitle = module == &runtime.general_ && section == general_module::appearance_section ?
                            std::string{runtime.general_.AppearanceTab() == 0 ? "Appearance" : general_module::appearance_tabs[runtime.general_.AppearanceTab()]} : title;
                        const auto viewTitle = pageTitle + (runtime.IsExperienceDisplay() ?
                            std::string{" / "} + displayTabs[static_cast<size_t>(runtime.experience_display_.View())] : "");
                        if (!audit(viewTitle, seen)) return false;
                        for (size_t index = 0; index < module->OptionCount(); ++index)
                        {
                            if (!runtime.MatchesConfigOption(index)) continue;
                            const auto option = module->Option(index);
                            const bool selector = option.selector != module_selector::none ||
                                (option.label != nullptr && (std::strcmp(option.label, "Status order") == 0 || std::strcmp(option.label, "Native filter group") == 0 || std::strcmp(option.label, "Native window") == 0));
                            if (!selector || option.choice == nullptr) continue;
                            const auto original = *option.choice;
                            for (size_t choice = 0; choice < option.choice_count; ++choice)
                            {
                                *option.choice = static_cast<int>(choice);
                                if (!audit(pageTitle + " / " + option.choices[choice], seen)) return false;
                            }
                            *option.choice = original;
                        }
                        return true;
                    };
                    if (module == &runtime.general_ && section == general_module::appearance_section)
                    {
                        for (size_t tab = 0; tab < std::size(general_module::appearance_tabs); ++tab)
                        {
                            runtime.general_.SelectAppearanceTab(tab);
                            if (!run()) return false;
                        }
                        runtime.general_.SelectAppearanceTab(0);
                    }
                    else if (runtime.IsExperienceDisplay())
                    {
                        for (int tab = 0; tab < 2; ++tab)
                        {
                            runtime.experience_display_.SelectView(static_cast<experience_display_view>(tab));
                            if (!run()) return false;
                        }
                        runtime.experience_display_.SelectView(experience_display_view::bar_info);
                    }
                    else if (!run()) return false;
                }
            }
            module->SelectConfigView(originalView);
        }
        if (!check(runtime.CaptureSettings().Preferences("").ToIniText() == originalSettings,
            "browsing every reordered group preserves all saved settings and positions")) return false;
        runtime.actionbars_.SetWindowFocused(false);
        runtime.SelectConfigModule(&runtime.general_);
        const auto settleGlobal = [&] {
            runtime.ConfigSidebarRows();
            for (auto& [id, transition] : runtime.config_.sidebar_animation.pages) transition.started = 0;
            return runtime.ConfigSidebarRows();
        };
        const auto globalTopics = runtime.general_.ConfigSections();
        for (size_t section = 0; section < globalTopics.size(); ++section)
        {
            if (section == general_module::attribution_section || section == general_module::game_hud_section || section == general_module::extra_settings_section) continue;
            runtime.SelectConfigTopic(section);
            const auto rows = runtime.ConfigTopicRows();
            const auto id = std::string{"general/"} + std::string{globalTopics[section].Id()} + (section == general_module::appearance_section ? "/0" : "");
            const auto row = std::find_if(rows.begin(), rows.end(), [&](const auto& item) { return item.id == id; });
            if (!check(row != rows.end(), "all Global sections are reachable in the second navigation column")) return false;
            runtime.SelectConfigTopicRow(static_cast<size_t>(row - rows.begin()));
            if (!check(runtime.config_.module == &runtime.general_ && runtime.config_.section == section,
                "Global topic routes retain their original identity")) return false;
        }
        for (size_t tab = 0; tab < std::size(general_module::appearance_tabs); ++tab)
        {
            runtime.config_.module = &runtime.general_;
            runtime.SelectConfigTopic(general_module::appearance_section);
            runtime.general_.SelectAppearanceTab(tab);
            const auto sidebar = settleGlobal();
            const auto topics = runtime.ConfigTopicRows();
            if (!check(std::none_of(sidebar.begin(), sidebar.end(), [](const auto& item) { return item.id.starts_with("general/"); }),
                "Global does not expand destinations in the primary sidebar")) return false;
            if (!check(std::strcmp(runtime.ActiveConfigPage().id, tab < 3 ? "general" : "extras") == 0,
                "shared appearance destinations select their Global or Extras category")) return false;
            if (tab < 3)
            {
                const auto id = std::string{"general/appearance/"} + std::to_string(tab);
                const auto row = std::find_if(topics.begin(), topics.end(), [&](const auto& item) { return item.id == id; });
                if (!check(row != topics.end() && row->selected && !row->parent && !row->child,
                    "Global appearance pages are direct topics in the second column")) return false;
                if (!check(std::none_of(topics.begin(), topics.end(), [](const auto& item) {
                    return item.id == "general/appearance" || item.id.starts_with("general/attribution") || item.id == "general/game-hud" ||
                        item.id == "general/appearance/3" || item.id == "general/appearance/4";
                }), "Global topics omit Attribution and settings moved to Extras")) return false;
                runtime.SelectConfigTopicRow(static_cast<size_t>(row - topics.begin()));
            }
            else
            {
                const auto id = std::string{"extras/appearance/"} + std::to_string(tab);
                const auto row = std::find_if(sidebar.begin(), sidebar.end(), [&](const auto& item) { return item.id == id; });
                if (!check(row != sidebar.end() && row->available && row->selected && topics.empty(),
                    "Extras retains its direct focus and feedback destinations")) return false;
                runtime.SelectConfigSidebarItem(row->item);
            }
            if (!check(runtime.general_.AppearanceTab() == tab &&
                runtime.ControllerConfigItems(config_controller_region::content_tabs).empty(),
                "moved appearance settings keep their underlying saved identities")) return false;
            if (!check(runtime.ConfigLayout().OptionRow(0).y == runtime.config_.bounds.y + ui::config_layout::grid_y + ((tab == 1 || tab == 3) ? 56 : 0),
                "appearance pages remove unused top space and retain visible preview space")) return false;
            const auto saved = settings_store::FromIniText(""); runtime.SaveConfigSelection(saved);
            auto restored = std::make_unique<hud_runtime>(); restored->LoadConfigSelection(saved);
            if (!check(restored->general_.AppearanceTab() == tab &&
                std::strcmp(restored->ActiveConfigPage().id, tab < 3 ? "general" : "extras") == 0,
                "saved shared settings reopen under the correct sidebar category")) return false;
        }
        const D3DVIEWPORT8 globalViewport{0, 0, 1280, 720, 0, 1};
        runtime.general_.SelectAppearanceTab(0);
        runtime.SelectConfigTopic(general_module::appearance_section);
        const auto globalRows = runtime.ConfigTopicRows();
        const auto configText = std::find_if(globalRows.begin(), globalRows.end(), [](const auto& row) { return row.id == "general/appearance/2"; });
        if (!check(configText != globalRows.end() && std::strcmp(configText->label, "Config Settings") == 0, "Global Config Settings has one second-column topic")) return false;
        const auto configTextBounds = runtime.ConfigLayout().Submenu(static_cast<size_t>(configText - globalRows.begin()), globalRows.size());
        runtime.mouse_x_ = static_cast<int>(configTextBounds.x + 8); runtime.mouse_y_ = static_cast<int>(configTextBounds.y + configTextBounds.height * .5f);
        runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, globalViewport);
        runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, globalViewport);
        if (!check(runtime.IsGeneralAppearance() && runtime.general_.AppearanceTab() == 2, "pointer activates Global's second-column settings")) return false;
        runtime.config_.focus.active = true; runtime.config_.focus.viewport = globalViewport;
        runtime.config_.focus.region = config_controller_region::topics; runtime.config_.focus.id = "general/appearance/1";
        runtime.HandleControllerConfig(controller_action::confirm, false);
        if (!check(runtime.IsGeneralAppearance() && runtime.general_.AppearanceTab() == 1, "controller uses Global's second-column topics")) return false;
        runtime.config_.focus.active = false;
        runtime.general_.SelectAppearanceTab(2);
        const auto globalSelection = settings_store::FromIniText(""); runtime.SaveConfigSelection(globalSelection);
        auto globalRestored = std::make_unique<hud_runtime>(); globalRestored->LoadConfigSelection(globalSelection);
        if (!check(globalRestored->IsGeneralAppearance() && globalRestored->general_.AppearanceTab() == 2,
            "Global topic selection survives saved navigation")) return false;
        auto oldSelection = settings_store::FromIniText("[session.config]\nmodule=general\ntopic_id=extra-settings\nsection=7\n");
        globalRestored->LoadConfigSelection(oldSelection);
        if (!check(globalRestored->IsGeneralAppearance() && globalRestored->general_.AppearanceTab() == 2,
            "the retired Extra Settings destination reopens Config Settings")) return false;
        const auto attribution = runtime.ConfigLayout().HeaderAction();
        runtime.mouse_x_ = static_cast<int>(attribution.x + attribution.width * .5f);
        runtime.mouse_y_ = static_cast<int>(attribution.y + attribution.height * .5f);
        runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, globalViewport);
        runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, globalViewport);
        if (!check(runtime.IsAttribution() && runtime.ControllerConfigItems(config_controller_region::content_tabs).size() == std::size(ui::attribution_views),
            "the header Attribution button opens all credit pages")) return false;
        const auto creditTab = ui::config_controls::Segment(runtime.ConfigLayout().ContentTabs(), 1, std::size(ui::attribution_views));
        runtime.mouse_x_ = static_cast<int>(creditTab.x + creditTab.width * .5f);
        runtime.mouse_y_ = static_cast<int>(creditTab.y + creditTab.height * .5f);
        if (!check(runtime.ConfigSoundTarget(globalViewport) != 0 && runtime.ResolveCursorShape(globalViewport) == hud_runtime::cursor_shape::hand,
            "credit tabs expose shared pointer and sound targets")) return false;
        runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, globalViewport);
        runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, globalViewport);
        if (!check(runtime.general_.AttributionView() == 1, "pointer switches Attribution credit pages")) return false;
        runtime.config_.focus.active = true; runtime.config_.focus.viewport = globalViewport;
        runtime.config_.focus.region = config_controller_region::content_tabs; runtime.config_.focus.id = ui::attribution_views[2];
        runtime.HandleControllerConfig(controller_action::confirm, false);
        if (!check(runtime.IsAttribution() && runtime.general_.AttributionView() == 2, "controller switches Attribution credit pages")) return false;
        runtime.config_.focus.active = false;
        runtime.SelectConfigModule(&runtime.game_hud_);
        const auto extrasRows = settleGlobal();
        const auto nativeMenus = std::find_if(extrasRows.begin(), extrasRows.end(), [](const auto& row) { return row.id == "native-ui/0"; });
        if (!check(nativeMenus != extrasRows.end() && nativeMenus->available && nativeMenus->selected &&
            std::strcmp(nativeMenus->label, "HUD Windows") == 0 && !runtime.ConfigTopicRows().empty(),
            "native HUD controls belong to the Native UI category")) return false;
        if (!check(runtime.ConfigLayout().OptionRow(0).y == runtime.config_.bounds.y + ui::config_layout::grid_y,
            "pages without tabs or a topic column start directly below the header")) return false;
        const auto legacyMenus = settings_store::FromIniText("[session.config]\nbasic=false\nmodule=general\ntopic_id=game-hud\nsection=6\n");
        globalRestored->LoadConfigSelection(legacyMenus);
        if (!check(globalRestored->config_.module == &globalRestored->game_hud_ && globalRestored->config_.section == 0,
            "saved Global Game HUD navigation migrates to Native Menus")) return false;
        runtime.general_.SelectAppearanceTab(0); runtime.general_.SelectAttributionView(0);
        runtime.SelectConfigModule(&runtime.player_);
        runtime.SelectConfigTopic(0);
        const D3DVIEWPORT8 topicViewport{0, 0, 1280, 720, 0, 1};
        const auto clickTopic = [&](const char* id) {
            const auto topics = runtime.ConfigTopicRows();
            const auto found = std::find_if(topics.begin(), topics.end(), [&](const auto& topic) { return topic.id == id; });
            if (found == topics.end()) return false;
            const auto bounds = ui::SettingsTopicBounds(runtime.ConfigLayout().Submenu(static_cast<size_t>(found - topics.begin()), topics.size()), *found);
            runtime.mouse_x_ = static_cast<int>(bounds.x + 8); runtime.mouse_y_ = static_cast<int>(bounds.y + bounds.height * .5f);
            runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, topicViewport);
            runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, topicViewport);
            return runtime.ResolveCursorShape(topicViewport) == hud_runtime::cursor_shape::hand;
        };
        const auto appearanceClicked = clickTopic("appearance");
        const bool appearanceSelected = runtime.config_.section == static_cast<size_t>(bar_config_section::layout);
        const auto colorsClicked = clickTopic("appearance/colors");
        if (!check(appearanceClicked && appearanceSelected && colorsClicked && runtime.config_.section == static_cast<size_t>(bar_config_section::colors),
            "pointer opens Appearance and selects its indented categories")) return false;
        runtime.config_.focus.active = true; runtime.config_.focus.viewport = topicViewport;
        runtime.config_.focus.region = config_controller_region::topics; runtime.config_.focus.id = "appearance/effects";
        runtime.HandleControllerConfig(controller_action::confirm, false);
        if (!check(runtime.config_.section == static_cast<size_t>(bar_config_section::effects) && clickTopic("general") && clickTopic("appearance") &&
            runtime.config_.section == static_cast<size_t>(bar_config_section::effects), "controller shares category routing and Appearance remembers its last category")) return false;
        const auto topicSelection = settings_store::FromIniText(""); runtime.SaveConfigSelection(topicSelection);
        hud_runtime topicRestored; topicRestored.LoadConfigSelection(topicSelection);
        if (!check(topicRestored.config_.module == &topicRestored.player_ && topicRestored.config_.section == runtime.config_.section,
            "grouped settings retain existing saved topic identities")) return false;
        runtime.config_.focus.active = false;
        runtime.SelectConfigModule(&runtime.equipment_);
        runtime.ConfigSidebarRows();
        for (auto& [id, transition] : runtime.config_.sidebar_animation.pages) transition.started = 0;
        auto sidebar = runtime.ConfigSidebarRows();
        const auto equipmentRow = std::find_if(sidebar.begin(), sidebar.end(), [](const auto& row) { return row.child && std::string_view{row.label} == "Equipment"; });
        if (!check(equipmentRow != sidebar.end() && equipmentRow->available && equipmentRow->selected,
            "Inventory destinations expand under the selected left category")) return false;
        const auto sidebarItems = runtime.ControllerConfigItems(config_controller_region::categories);
        const auto equipmentItem = std::find_if(sidebarItems.begin(), sidebarItems.end(), [&](const auto& item) { return item.id == equipmentRow->id; });
        if (!check(equipmentItem != sidebarItems.end() && equipmentItem->index == equipmentRow->item,
            "controller navigation uses the same submenu destinations as pointer input")) return false;
        const auto gilRow = std::find_if(sidebar.begin(), sidebar.end(), [](const auto& row) { return row.child && std::string_view{row.label} == "Gil"; });
        if (!check(gilRow != sidebar.end(), "Inventory retains its Gil destination")) return false;
        runtime.mouse_x_ = static_cast<int>(gilRow->bounds.x + 8); runtime.mouse_y_ = static_cast<int>(gilRow->bounds.y + gilRow->bounds.height * .5f);
        runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, topicViewport);
        runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, topicViewport);
        if (!check(runtime.config_.module == &runtime.gil_, "pointer activates destinations inside the new sidebar submenu")) return false;
        runtime.config_.focus.active = true; runtime.config_.focus.viewport = topicViewport;
        runtime.config_.focus.region = config_controller_region::categories; runtime.config_.focus.id = equipmentItem->id;
        runtime.HandleControllerConfig(controller_action::confirm, false);
        if (!check(runtime.config_.module == &runtime.equipment_, "controller activates the same nested destination")) return false;
        runtime.config_.bounds.height = 350;
        runtime.SelectConfigModule(&runtime.compass_);
        runtime.ConfigSidebarRows();
        for (auto& [id, transition] : runtime.config_.sidebar_animation.pages) transition.started = 0;
        sidebar = runtime.ConfigSidebarRows();
        const auto lastChild = std::find_if(sidebar.rbegin(), sidebar.rend(), [](const auto& row) { return row.child && row.available; });
        if (!check(lastChild != sidebar.rend() && runtime.ConfigSidebarScrollbar().Visible(), "long submenus have an independent sidebar scrollbar")) return false;
        const auto expandedBounds = runtime.ConfigSidebarBounds();
        const auto extrasRoot = std::find_if(sidebar.begin(), sidebar.end(), [](const auto& row) { return row.id == "extras"; });
        if (!check(extrasRoot != sidebar.end() && extrasRoot->bounds.y >= expandedBounds.y &&
            extrasRoot->bounds.y + extrasRoot->bounds.height + 22 <= expandedBounds.y + expandedBounds.height + 1,
            "opening a category taller than the sidebar reveals its header and first destination")) return false;
        runtime.config_.focus.region = config_controller_region::categories; runtime.config_.focus.id = lastChild->id;
        runtime.config_.sidebar_animation.scroll = 0;
        runtime.RefreshControllerConfigFocus();
        sidebar = runtime.ConfigSidebarRows();
        const auto revealed = std::find_if(sidebar.begin(), sidebar.end(), [&](const auto& row) { return row.id == runtime.config_.focus.id; });
        const auto sidebarBounds = runtime.ConfigSidebarBounds();
        if (!check(revealed != sidebar.end() && revealed->bounds.y >= sidebarBounds.y &&
            revealed->bounds.y + revealed->bounds.height <= sidebarBounds.y + sidebarBounds.height + 1,
            "controller focus scrolls the selected submenu item into view")) return false;
        runtime.config_.focus.active = false;
        const auto sidebarScroll = runtime.ConfigSidebarScrollbar();
        const auto optionScroll = runtime.config_.scroll;
        runtime.mouse_x_ = static_cast<int>(sidebarScroll.Thumb().x + 4); runtime.mouse_y_ = static_cast<int>(sidebarScroll.Thumb().y + 4);
        runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, topicViewport);
        if (!check(runtime.interaction_ == hud_runtime::interaction::option_scrollbar && runtime.config_.sidebar_scrolling,
            "the sidebar scrollbar uses shared pointer capture")) return false;
        runtime.mouse_y_ = static_cast<int>(sidebarScroll.track.y - 30);
        runtime.ProcessPointerMessage(WM_MOUSEMOVE, MK_LBUTTON, topicViewport);
        runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, topicViewport);
        if (!check(runtime.config_.sidebar_animation.scroll == 0 && runtime.config_.scroll == optionScroll &&
            runtime.interaction_ == hud_runtime::interaction::none && !runtime.config_.sidebar_scrolling,
            "sidebar dragging stays independent of option scrolling and releases ownership outside its track")) return false;
        runtime.config_.bounds.height = 600;
        runtime.config_.module = &runtime.party_;
        runtime.config_.section = party_module::layout_section;
        const auto partySlots = runtime.GroupOptionSlots();
        const auto at = [&](const size_t index) { return static_cast<size_t>(std::find(partySlots.begin(), partySlots.end(), index) - partySlots.begin()); };
        if (!check(at(52) % 2 == 0 && at(party_module::base_option_count + 1) == at(52) + 1 && at(55) == at(54) + 1 && at(54) % 2 == 0,
            "Party bar width/height and X/Y offsets share rows without moving window width into the bar pair")) return false;
        runtime.config_.section = party_module::text_section;
        const auto textSlots = runtime.GroupOptionSlots();
        const auto font = std::find_if(textSlots.begin(), textSlots.end(), [&](const auto index) {
            return index < runtime.party_.OptionCount() && std::string_view{runtime.party_.Option(index).label} == "Font";
        });
        if (!check(font != textSlots.end() && font + 1 != textSlots.end() &&
            (font - textSlots.begin()) % 2 == 0 && std::string_view{runtime.party_.Option(*(font + 1)).label} == "Size (px)",
            "font and size share the first style row after text visibility")) return false;
        runtime.config_.focus.region = config_controller_region::options;
        auto items = runtime.ControllerConfigItems(config_controller_region::options);
        const auto visibleControls = std::count_if(textSlots.begin(), textSlots.end(), [&](const auto index) {
            return index < runtime.party_.OptionCount() && runtime.party_.Option(index).available;
        });
        if (!check(items.size() == static_cast<size_t>(visibleControls), "controller navigation skips layout placeholders and exposes every visible text control")) return false;
        const auto fontItem = std::find_if(items.begin(), items.end(), [&](const auto& item) { return item.index == *font; });
        const auto sizeItem = std::find_if(items.begin(), items.end(), [&](const auto& item) { return item.index == *(font + 1); });
        if (!check(fontItem != items.end() && sizeItem != items.end(), "font and size both have controller focus targets")) return false;
        runtime.config_.focus.id = fontItem->id;
        runtime.HandleControllerConfig(controller_action::right, false);
        if (!check(runtime.config_.focus.id == sizeItem->id, "controller follows the displayed font/size row")) return false;
        runtime.HandleControllerConfig(controller_action::confirm, false);
        if (!check(runtime.config_.focus.adjusting, "controller activates the original size setting after reordering")) return false;
        runtime.config_.focus.adjusting = false;
        const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
        const auto value = ui::config_layout::SliderValue(runtime.ConfigLayout().OptionRow(static_cast<int>(font - textSlots.begin() + 1)));
        runtime.mouse_x_ = static_cast<int>(value.x + 3); runtime.mouse_y_ = static_cast<int>(value.y + 3);
        runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
        runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
        if (!check(runtime.config_.slider_input.Focused() && runtime.config_.slider_input.Option().slider == runtime.party_.Option(*(font + 1)).slider,
            "pointer edits the same size setting at its new position")) return false;
        runtime.FinishConfigInput(false);
        config_controller_focus focus;
        const std::vector<config_controller_item> grid{{"a", {0, 0, 100, 30}}, {"b", {110, 0, 100, 30}},
            {"c", {0, 30, 100, 30}}, {"d", {0, 60, 100, 30}}, {"e", {110, 60, 100, 30}}};
        focus.id = "b";
        if (!check(focus.MoveGrid(grid, 0, 1) && focus.id == "c" && focus.MoveGrid(grid, 0, 1) && focus.id == "d" &&
            focus.MoveGrid(grid, 1, 0) && focus.id == "e", "controller directions follow actual rows and skip empty cells")) return false;
        report << "## Coverage\n\n" << runtime.modules_.Count() << " registered modules, " << pages << " distinct pages, " << controls << " controls across those pages, and " << variants << " selector/view combinations.\n";
        if (reportPath != nullptr) { std::ofstream output{reportPath, std::ios::binary}; output << report.str(); }
        std::printf("Settings presentation: %zu modules, %zu pages, %zu controls, %zu variants; pairing and pointer/controller routes passed.\n",
            runtime.modules_.Count(), pages, controls, variants);
        return true;
    }

    auto config_input_test::ConfigScrollbar(void) -> bool
    {
        hud_runtime runtime; runtime.editing_ = true; runtime.config_.bounds = {40, 40, 1100, 500};
        runtime.config_.module = &runtime.chat_; runtime.config_.section = 7;
        const D3DVIEWPORT8 viewport{0, 0, 1600, 900, 0, 1};
        const auto top = runtime.ConfigScrollbar();
        if (!check(top.Visible() && top.Thumb().y == top.track.y, "long config pages show a scrollbar at the current position")) return false;
        const auto lastColumn = runtime.ConfigLayout().OptionRow(1);
        if (!check(lastColumn.x + lastColumn.width < top.track.x, "scrollbar gutter keeps both option columns unobscured")) return false;
        runtime.mouse_x_ = static_cast<int>(top.Thumb().x + top.Thumb().width / 2);
        runtime.mouse_y_ = static_cast<int>(top.Thumb().y + top.Thumb().height / 2);
        if (!check(runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport) &&
            runtime.interaction_ == hud_runtime::interaction::option_scrollbar, "thumb press captures config scrolling")) return false;
        runtime.mouse_y_ = static_cast<int>(top.track.y + top.track.height + 30);
        runtime.ProcessPointerMessage(WM_MOUSEMOVE, MK_LBUTTON, viewport);
        const auto bottom = runtime.ConfigScrollbar();
        if (!check(runtime.config_.scroll == bottom.Maximum() * 2 && std::abs(bottom.Thumb().y + bottom.Thumb().height - bottom.track.y - bottom.track.height) < 0.1f,
            "thumb dragging reaches the final option row and stays bounded outside the track")) return false;
        runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
        if (!check(runtime.interaction_ == hud_runtime::interaction::none, "thumb release ends pointer capture")) return false;
        runtime.mouse_y_ = static_cast<int>(bottom.track.y + 1);
        runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
        runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
        if (!check(runtime.config_.scroll < bottom.Maximum() * 2 && runtime.config_.scroll % 2 == 0,
            "track clicks page without swapping option columns")) return false;
        runtime.config_.collapsed = true;
        if (!check(!runtime.ConfigScrollbar().Visible(), "collapsed config hides the scrollbar")) return false;
        runtime.config_.collapsed = false; runtime.config_.module = &runtime.player_; runtime.config_.section = 0;
        return check(!runtime.ConfigScrollbar().Visible(), "short config pages do not reserve a scrollbar");
    }

    auto config_input_test::HeaderInputMode(void) -> bool
    {
        hud_runtime runtime; runtime.editing_ = true; runtime.config_.bounds = {40, 40, 1100, 600};
        const D3DVIEWPORT8 viewport{0, 0, 1600, 900, 0, 1};
        const auto original = runtime.player_.State().bounds;
        const auto click = [&](const ui::rect& at) {
            runtime.mouse_x_ = static_cast<int>(at.x + at.width * 0.5f); runtime.mouse_y_ = static_cast<int>(at.y + at.height * 0.5f);
            const bool press = runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
            return runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport) && press;
        };
        for (const bool collapsed : {false, true})
        {
            runtime.config_.collapsed = collapsed;
            for (int mode = 0; mode < 3; ++mode)
            {
                if (!check(click(runtime.ConfigLayout().InputMode()) && runtime.config_.header_menu_open, "pointer opens the header input dropdown")) return false;
                controller_status unavailable{}; unavailable.enumeration_disabled = true; unavailable.connection_known = true;
                runtime.UpdateControllerStatus(unavailable); runtime.UpdateControllerFocus();
                if (!check(runtime.config_.header_menu_open, "unavailable controller updates cannot dismiss a mouse-owned mode dropdown")) return false;
                if (!check(click(runtime.ConfigLayout().HeaderMenuRow(mode, false)) && runtime.general_.InputMode() == mode && !runtime.config_.header_menu_open,
                    "pointer selects every input mode with expanded or collapsed config")) return false;
            }
        }
        if (!check(runtime.player_.State().bounds.x == original.x && runtime.player_.State().bounds.y == original.y,
            "mode selection leaves HUD positions unchanged")) return false;
        runtime.controller_surface_ = hud_runtime::controller_surface::config; runtime.config_.focus.active = true; runtime.config_.header_menu_open = true;
        controller_status disconnected{}; disconnected.connection_known = true;
        runtime.UpdateControllerStatus(disconnected);
        if (!check(runtime.controller_surface_ == hud_runtime::controller_surface::none && !runtime.config_.header_menu_open,
            "disconnect still releases a controller-owned popup")) return false;
        return check(ui::config_controls::IconId("statuses") == ui::icon_glyph::statuses && ui::config_controls::IconId("buffs") == ui::icon_glyph::statuses &&
            ui::config_controls::IconId("debuffs") == ui::icon_glyph::statuses, "status groups use their distinct effects icon");
    }

    auto config_input_test::AllBarsScope(void) -> bool
    {
        hud_runtime runtime; runtime.editing_ = true; runtime.config_.bounds = {40, 40, 1100, 600};
        const auto snapshot = [](const auto& styles) {
            auto store = settings_store::FromIniText("");
            for (size_t i = 0; i < styles.size(); ++i) SaveMeterStyle(store, ("meter." + std::to_string(i)).c_str(), styles[i]);
            return store.ToIniText();
        };
        for (auto* module : {static_cast<hud_module*>(&runtime.player_), static_cast<hud_module*>(&runtime.party_),
            static_cast<hud_module*>(&runtime.alliance_first_), static_cast<hud_module*>(&runtime.alliance_second_)})
        {
            auto* party = module == &runtime.player_ ? nullptr : static_cast<party_module*>(module);
            std::vector<ui::meter_style*> styles = party != nullptr ? std::vector<ui::meter_style*>{&party->meters_[0], &party->meters_[1], &party->meters_[2], &party->meters_[3]} :
                std::vector<ui::meter_style*>{&runtime.player_.Settings().hp_style, &runtime.player_.Settings().mp_style, &runtime.player_.Settings().tp_style};
            std::vector<ui::meter_style> expected;
            for (size_t i = 0; i < styles.size(); ++i)
            {
                styles[i]->height = 12 + static_cast<float>(i) * 5;
                styles[i]->appearance = i % 2 == 0 ? ui::meter_style::modern : ui::meter_style::window_theme;
                styles[i]->shadow_strength = 0.1f * static_cast<float>(i + 1);
                styles[i]->sheen = 0xFF102030 + static_cast<D3DCOLOR>(i);
                styles[i]->bookends = styles[i]->themed_bookends = false;
                expected.push_back(*styles[i]);
            }
            const auto actual = [&] { std::vector<ui::meter_style> values; for (const auto* style : styles) values.push_back(*style); return snapshot(values); };
            runtime.config_.module = module;
            runtime.config_.section = party != nullptr ? party_module::bar_style_section : static_cast<size_t>(bar_config_section::bar_style);
            const size_t scope = party != nullptr ? 24 : 4, selected = party != nullptr ? 25 : 5, start = party != nullptr ? party_module::base_option_count : 9;
            *module->Option(scope).choice = 1; *module->Option(selected).choice = 0;
            if (party != nullptr)
            {
                party->bar_widths_ = {170, 120, 60, 140}; party->bar_positions_ = {0, 1, 2, 3};
                party->bar_offsets_x_ = {1, 2, 3, 4}; party->bar_offsets_y_ = {-1, -2, -3, -4};
            }
            const auto selector = runtime.ElementSelectorLayout(runtime.ElementSelector(), runtime.ConfigLayout());
            runtime.mouse_x_ = static_cast<int>(selector.tabs[0].x + 5); runtime.mouse_y_ = static_cast<int>(selector.tabs[0].y + 5);
            if (!check(runtime.ProcessElementSelectorClick() && actual() == snapshot(expected), "selecting All bars by pointer preserves every existing meter field")) return false;
            *module->Option(scope).choice = 1;
            runtime.config_.focus.region = config_controller_region::elements;
            const auto items = runtime.ControllerConfigItems(config_controller_region::elements);
            if (!check(!items.empty() && runtime.ActivateControllerConfigItem(items.front(), controller_action::confirm, false) && actual() == snapshot(expected),
                "selecting All bars by controller preserves every existing meter field")) return false;
            const auto edit = [&](const size_t index) { module->OnOptionChanged(start + index); return actual() == snapshot(expected); };
            *module->Option(start + 1).slider = 19; for (auto& style : expected) style.height = 19;
            if (!check(edit(1), "editing shared height preserves each bar's appearance, effects and colors")) return false;
            *module->Option(start + 6).toggle = false; for (auto& style : expected) style.shadow_enabled = false;
            if (!check(edit(6), "editing one shared effect preserves all other properties")) return false;
            *module->Option(start + 25).color = 0xFFABCDEF; for (auto& style : expected) style.sheen = 0xFFABCDEF;
            if (!check(edit(25), "editing one shared color preserves all other properties")) return false;
            *module->Option(start + 2).toggle = true;
            for (auto& style : expected) (style.appearance == ui::meter_style::modern ? style.bookends : style.themed_bookends) = true;
            if (!check(edit(2), "shared Bookends applies to each bar's current appearance without copying styles")) return false;
            *module->Option(start).choice = ui::meter_style::window_theme; for (auto& style : expected) style.appearance = ui::meter_style::window_theme;
            if (!check(edit(0), "shared appearance changes preserve independent bar geometry and effects")) return false;
            if (party != nullptr && !check(party->bar_widths_ == std::array<float,4>{170,120,60,140} && party->bar_positions_ == std::array<int,4>{0,1,2,3} &&
                party->bar_offsets_x_ == std::array<float,4>{1,2,3,4} && party->bar_offsets_y_ == std::array<float,4>{-1,-2,-3,-4},
                "All bars selection and style edits preserve independent Party widths, positions and offsets")) return false;
            *module->Option(scope).choice = 1; *module->Option(start + 1).slider = 27; module->OnOptionChanged(start + 1); expected[0].height = 27;
            if (!check(actual() == snapshot(expected), "returning to an individual bar retains independent edits")) return false;
        }
        return true;
    }

    auto config_input_test::ElementTabs(void) -> bool
    {
        hud_runtime runtime; runtime.editing_ = true; runtime.config_.bounds = {40, 40, 1100, 600};
        const D3DVIEWPORT8 viewport{0, 0, 1600, 900, 0, 1};
        const auto click = [&](const ui::rect& at) {
            runtime.mouse_x_ = static_cast<int>(at.x + at.width * 0.5f);
            runtime.mouse_y_ = static_cast<int>(at.y + at.height * 0.5f);
            const bool press = runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
            return runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport) && press;
        };
        size_t selectors{};
        for (auto* module : runtime.modules_.All())
        {
            module->State().enabled = true;
            const auto topics = module->ConfigSections();
            runtime.config_.module = module;
            for (size_t topic = 0; topic < topics.size(); ++topic)
            {
                runtime.config_.section = topic;
                const auto selector = runtime.ElementSelector();
                if (selector.count == 0) continue;
                ++selectors;
                const auto layout = runtime.ConfigLayout();
                const auto tabs = runtime.ElementSelectorLayout(selector, layout);
                if (!check(tabs.count == selector.count && tabs.tabs.back().width == 0 &&
                    tabs.tabs[tabs.count - 1].y + 28 <= layout.OptionRow(0).y,
                    "all editable elements have pinned tabs above their settings")) return false;
                for (size_t i = 0; i < runtime.GroupOptionCount(); ++i)
                    if (!check(runtime.GroupOptionIndex(i) >= module->OptionCount() || module->Option(runtime.GroupOptionIndex(i)).selector == module_selector::none,
                        "element selectors do not leave duplicate cycling rows")) return false;
            }
        }
        if (!check(selectors >= 18, "shared selectors cover text and bar editors across modules")) return false;
        runtime.config_.module = &runtime.party_; runtime.config_.section = party_module::text_section;
        for (const float width : {936.0f, 1100.0f})
        {
            runtime.config_.bounds.width = width;
            const auto selector = runtime.ElementSelector();
            const auto tabs = runtime.ElementSelectorLayout(selector, runtime.ConfigLayout());
            if (!check(selector.count == 7 && click(tabs.tabs[6]) && runtime.party_.selected_text_ == 6 &&
                runtime.ElementSelector().selected == 6 && runtime.ElementSelectorTarget() != 0 &&
                runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::hand,
                "Heading is directly selectable with matching highlight, sound and cursor at both widths")) return false;
            const auto layout = runtime.ConfigLayout();
            const auto first = layout.Section(0, 3), last = layout.Section(2, 3), menu = layout.Submenu(0, runtime.party_.ConfigSections().size());
            if (!check(first.x == menu.x && first.y + first.height < menu.y && layout.ElementTabs().y == menu.y &&
                last.x + last.width <= runtime.config_.bounds.x + width - 8,
                "top-level tabs span both secondary navigation and settings without overlapping either")) return false;
            if (!check(click(last) && runtime.config_.module == &runtime.alliance_second_,
                "full-width header tabs route to the correct alliance")) return false;
            runtime.config_.module = &runtime.party_; runtime.config_.section = party_module::text_section;
        }
        runtime.config_.module = &runtime.player_; runtime.config_.section = static_cast<size_t>(bar_config_section::bar_style);
        auto selector = runtime.ElementSelector();
        auto tabs = runtime.ElementSelectorLayout(selector, runtime.ConfigLayout());
        if (!check(selector.combined && selector.count == 4 && std::strcmp(selector.labels[0], "All bars") == 0 &&
            click(tabs.tabs[2]) && runtime.player_.Settings().style_edit_mode == 1 && runtime.player_.Settings().selected_bar == 1,
            "All bars and HP/MP/TP share one direct selector")) return false;
        runtime.player_.Settings().hp_style.height = 12; runtime.player_.Settings().mp_style.height = 23;
        const auto tpHeight = runtime.player_.Settings().tp_style.height;
        tabs = runtime.ElementSelectorLayout(runtime.ElementSelector(), runtime.ConfigLayout());
        if (!check(click(tabs.tabs[0]) && runtime.player_.Settings().style_edit_mode == 0 &&
            runtime.player_.Settings().hp_style.height == 12 && runtime.player_.Settings().mp_style.height == 23 && runtime.player_.Settings().tp_style.height == tpHeight,
            "All bars changes editing scope without changing individual bar heights")) return false;
        const std::array<float, 4> widths{100, 100, 100, 100};
        const ui::element_selector_layout wrapped{{0, 0, 220, 28}, widths};
        if (!check(wrapped.count == 4 && wrapped.tabs[2].y == 32 && wrapped.height == 68 && !wrapped.IndexAt(102, 10),
            "narrow selectors wrap and gaps have no hidden click target")) return false;
        auto store = settings_store::FromIniText(""); runtime.player_.Save(store); runtime.party_.Save(store);
        player_module restoredPlayer; restoredPlayer.Load(store);
        party_module restoredParty; restoredParty.Load(store);
        runtime.editing_ = false;
        return check(restoredPlayer.Settings().style_edit_mode == 0 && restoredPlayer.Settings().selected_bar == 1 &&
            *restoredParty.Option(26).choice == 6, "direct selection retains the existing saved preferences");
    }

    auto config_input_test::ChoiceArrows(void) -> bool
    {
        hud_runtime runtime; runtime.editing_ = true; runtime.config_.module = &runtime.player_;
        runtime.config_.section = static_cast<size_t>(bar_config_section::bar_style);
        runtime.config_.bounds = {40, 40, 1100, 600};
        const D3DVIEWPORT8 viewport{0, 0, 1600, 900, 0, 1};
        const auto at = [&](const ui::rect& bounds) {
            runtime.mouse_x_ = static_cast<int>(bounds.x + bounds.width * 0.5f);
            runtime.mouse_y_ = static_cast<int>(bounds.y + bounds.height * 0.5f);
        };
        const auto click = [&](const ui::rect& bounds) {
            at(bounds);
            const bool press = runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
            return runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport) && press;
        };
        const auto slots = runtime.GroupOptionSlots();
        const auto style = std::find_if(slots.begin(), slots.end(), [&](const auto index) {
            return index < runtime.player_.OptionCount() && runtime.player_.Option(index).choice == &runtime.player_.Settings().hp_style.appearance;
        });
        if (!check(style != slots.end(), "the bar appearance choice is available")) return false;
        const auto row = runtime.ConfigLayout().OptionRow(static_cast<int>(style - slots.begin()));
        const auto option = runtime.player_.Option(*style);
        const int initial = *option.choice, count = static_cast<int>(option.choice_count);
        if (!check(click(ui::config_layout::ChoicePrevious(row)) && *option.choice == (initial + count - 1) % count &&
            click(ui::config_layout::ChoiceNext(row)) && *option.choice == initial,
            "previous and next arrows wrap both ends through normal captured input")) return false;
        if (!check(click(ui::config_layout::ChoiceLabel(row)) && *option.choice == (initial + 1) % count,
            "clicking the value retains next-choice behavior")) return false;
        at(ui::config_layout::ChoicePrevious(row)); const auto previous = runtime.ConfigSoundTarget(viewport);
        at(ui::config_layout::ChoiceNext(row)); const auto next = runtime.ConfigSoundTarget(viewport);
        if (!check(previous != 0 && next != previous && runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::hand,
            "arrows have distinct hover feedback and the hand cursor")) return false;
        if (!check(runtime.player_.Settings().mp_style.appearance == *option.choice && runtime.player_.Settings().tp_style.appearance == *option.choice,
            "choice activation invokes the shared-bar callback")) return false;
        runtime.config_.section = static_cast<size_t>(bar_config_section::text);
        const auto textSlots = runtime.GroupOptionSlots();
        const auto font = std::find_if(textSlots.begin(), textSlots.end(), [&](const auto index) {
            return index < runtime.player_.OptionCount() && runtime.player_.Option(index).kind == module_option_kind::dropdown;
        });
        if (!check(font != textSlots.end(), "the text font dropdown is available")) return false;
        const auto fontRow = runtime.ConfigLayout().OptionRow(static_cast<int>(font - textSlots.begin()));
        if (!check(click(ui::config_layout::ChoicePrevious(fontRow)) && runtime.config_.choice_input.Opened(),
            "real dropdowns retain their normal popup behavior")) return false;
        runtime.CloseChoiceInput(); runtime.editing_ = false;
        return true;
    }

    auto config_input_test::ConfigCollapse(void) -> bool
    {
        hud_runtime runtime;
        runtime.editing_ = true;
        runtime.config_.bounds = {40, 40, 1100, 600};
        runtime.config_.module = &runtime.target_;
        runtime.config_.section = 3;
        const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
        const auto original = runtime.config_.bounds;
        const auto send = [&](uint32_t message, uint32_t buttons, const ui::rect& at) {
            runtime.mouse_x_ = static_cast<int>(at.x + 3); runtime.mouse_y_ = static_cast<int>(at.y + 3);
            return runtime.ProcessPointerMessage(message, buttons, viewport);
        };
        const auto click = [&](const ui::rect& at) {
            const bool down = send(WM_LBUTTONDOWN, MK_LBUTTON, at);
            return send(WM_LBUTTONUP, 0, at) && down;
        };
        const auto eye = runtime.ConfigLayout().HeaderAction(1);
        if (!check(!hud_runtime::Contains(runtime.ConfigLayout().HeaderDragArea(), eye.x + 3, eye.y + 3),
            "the eye button is excluded from title-bar dragging")) return false;
        if (!check(send(WM_LBUTTONDOWN, MK_LBUTTON, eye) && runtime.config_.collapsed &&
            send(WM_LBUTTONDOWN, MK_LBUTTON, eye) && runtime.config_.collapsed && send(WM_LBUTTONUP, 0, eye),
            "one physical eye-button click collapses once and captures its release")) return false;
        if (!check(runtime.ConfigVisibleBounds().height == 32 && runtime.config_.bounds.height == original.height &&
            runtime.config_.module == &runtime.target_ && runtime.config_.section == 3,
            "collapse preserves the full size and selected settings")) return false;
        for (auto* module : runtime.modules_.All()) { module->State().bounds = {-1000, -1000, 100, 60}; module->State().content_bounds = {}; }
        runtime.mouse_x_ = 60; runtime.mouse_y_ = 140;
        if (!check(!runtime.UpdateMouseCapture(viewport, 0) && runtime.ConfigSoundTarget(viewport) == 0 &&
            runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::arrow,
            "the hidden sidebar leaves no mouse capture, sound or cursor targets")) return false;
        runtime.player_.State().bounds = {400, 180, 420, 110};
        const ui::rect preview{430, 200, 1, 1};
        if (!check(send(WM_LBUTTONDOWN, MK_LBUTTON, preview) && runtime.interaction_ == hud_runtime::interaction::drag_module &&
            runtime.config_.module == &runtime.player_ && runtime.config_.collapsed,
            "HUD previews inside the former config body remain selectable and draggable")) return false;
        send(WM_LBUTTONUP, 0, preview);
        if (!check(click(runtime.ConfigLayout().HeaderAction(1)) && !runtime.config_.collapsed &&
            runtime.config_.bounds.width == original.width && runtime.config_.bounds.height == original.height,
            "the eye restores the complete configuration at its saved size")) return false;
        if (!check(click(runtime.ConfigLayout().HeaderAction(1)) && runtime.config_.collapsed, "configuration can collapse again")) return false;
        const ui::rect title{runtime.config_.bounds.x + 20, runtime.config_.bounds.y + 5, 1, 1};
        if (!check(send(WM_LBUTTONDOWN, MK_LBUTTON, title) && runtime.interaction_ == hud_runtime::interaction::drag_config,
            "the collapsed title bar remains draggable")) return false;
        send(WM_LBUTTONUP, 0, title);
        if (!check(click(runtime.ConfigLayout().HeaderAction()) && !runtime.config_.collapsed && runtime.IsAttribution(),
            "attribution expands the window into its settings")) return false;
        return true;
    }

    auto config_input_test::CloseInput(void) -> bool
    {
        class mouse final : public IMouse
        {
        public:
            bool blocked{};
            void AddCallback(const char*, mousecallback_f) override {}
            void RemoveCallback(const char*) override {}
            bool GetBlockInput(void) const override { return this->blocked; }
            void SetBlockInput(bool value) override { this->blocked = value; }
        } input;
        hud_runtime runtime;
        runtime.mouse_ = &input;
        runtime.config_.bounds = {40, 40, 680, 430};
        const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
        const auto close = runtime.ConfigLayout().CloseButton();
        const auto send = [&](uint32_t message, uint32_t buttons, float x, float y) {
            runtime.mouse_x_ = static_cast<int>(x);
            runtime.mouse_y_ = static_cast<int>(y);
            return runtime.ProcessPointerMessage(message, buttons, viewport);
        };
        for (const bool previouslyBlocked : {false, true})
        {
            input.blocked = previouslyBlocked;
            runtime.SetEditing(true, false);
            if (!check(send(WM_LBUTTONDOWN, MK_LBUTTON, close.x + 3, close.y + 3) && runtime.IsEditing() && input.blocked,
                "pressing close keeps config open and game mouse input blocked until release")) return false;
            if (!check(send(WM_MOUSEMOVE, MK_LBUTTON, 1200, 5) && input.blocked && runtime.IsEditing(),
                "moving off close while held cannot reach the game camera")) return false;
            if (!check(send(WM_LBUTTONUP, 0, 1200, 5) && runtime.IsEditing() && !runtime.mouse_down_,
                "releasing outside cancels close and consumes the release")) return false;
            runtime.UpdateMouseCapture(viewport, 0);
            if (!check(input.blocked == previouslyBlocked, "cancelled close restores normal hover capture")) return false;
            if (!check(!send(WM_LBUTTONDOWN, MK_LBUTTON, 1200, 5) &&
                !send(WM_LBUTTONUP, 0, close.x + 3, close.y + 3) && runtime.IsEditing(),
                "a game-owned press cannot close config by releasing over X")) return false;
            send(WM_LBUTTONDBLCLK, MK_LBUTTON, close.x + 3, close.y + 3);
            runtime.UpdateMouseCapture(viewport, MK_LBUTTON);
            if (!check(runtime.IsEditing() && input.blocked, "held-frame updates keep the close gesture captured")) return false;
            if (!check(send(WM_LBUTTONUP, 0, close.x + 3, close.y + 3) && !runtime.IsEditing() &&
                input.blocked == previouslyBlocked && !runtime.mouse_down_ && runtime.mouse_buttons_ == 0 &&
                runtime.interaction_ == hud_runtime::interaction::none,
                "close consumes its matching release before restoring the original game mouse block state")) return false;
        }
        runtime.SetEditing(true, false);
        send(WM_LBUTTONDOWN, MK_LBUTTON, close.x + 3, close.y + 3);
        runtime.FinishInteraction();
        if (!check(send(WM_LBUTTONUP, 0, close.x + 3, close.y + 3) && runtime.IsEditing(),
            "cancelling an interaction cannot later activate close")) return false;
        runtime.SetEditing(false);
        for (const bool previouslyBlocked : {false, true})
            for (int surface = 0; surface < 5; ++surface)
            {
                input.blocked = previouslyBlocked;
                runtime.SetEditing(true, false);
                ui::rect button;
                const auto opened = [&] { return surface == 0 ? runtime.config_.active_color != nullptr : runtime.profile_name_.Focused(); };
                if (surface == 0)
                {
                    runtime.config_.module = &runtime.player_;
                    runtime.config_.active_color = &runtime.player_.Settings().hp_color;
                    button = ui::color_picker_layout{runtime.ConfigLayout().ColorEditor(viewport)}.Close();
                }
                else
                {
                    runtime.profile_dialog_ = surface < 3 ? hud_runtime::profile_dialog::share : hud_runtime::profile_dialog::import_profile;
                    runtime.profile_name_.Open("Close ownership");
                    const ui::profile_dialog_layout dialog{runtime.config_.bounds, viewport, true};
                    button = surface == 1 || surface == 3 ? dialog.Close() : dialog.Cancel();
                }
                if (!check(send(WM_LBUTTONDOWN, MK_LBUTTON, button.x + 3, button.y + 3) && opened() && input.blocked,
                    "submenu close waits for release while blocking native mouse input")) return false;
                if (!check(send(WM_MOUSEMOVE, MK_LBUTTON, 1200, 5) && runtime.UpdateMouseCapture(viewport, MK_LBUTTON) && input.blocked,
                    "a held submenu close stays captured outside its window")) return false;
                if (!check(send(WM_LBUTTONUP, 0, 1200, 5) && opened(),
                    "releasing away from a submenu close cancels dismissal")) return false;
                if (!check(send(WM_LBUTTONDBLCLK, MK_LBUTTON, button.x + 3, button.y + 3) && opened() &&
                    send(WM_LBUTTONUP, 0, button.x + 3, button.y + 3) && !opened() && runtime.IsEditing(),
                    "submenu close consumes its matching release and leaves Configuration open")) return false;
                runtime.SetEditing(false, false);
                if (!check(input.blocked == previouslyBlocked, "submenu dismissal preserves the original native mouse block")) return false;
            }
        return true;
    }

    auto config_input_test::PreviewFocus(void) -> bool
    {
        hud_runtime runtime;
        runtime.editing_ = true;
        runtime.config_.bounds = {30, 30, 1100, 600};
        runtime.chat_second_.State().enabled = true;
        const D3DVIEWPORT8 viewport{0, 0, 1920, 1080, 0, 1};
        for (auto* module : runtime.modules_.All()) { module->State().bounds = {-1000, -1000, 100, 60}; module->State().content_bounds = {}; }
        const auto send = [&](uint32_t message, uint32_t buttons, int x, int y) {
            runtime.mouse_x_ = x; runtime.mouse_y_ = y;
            return runtime.ProcessPointerMessage(message, buttons, viewport);
        };
        for (auto* module : runtime.modules_.All())
        {
            if (!module->HasEditableBounds() || !module->PreviewVisible()) continue;
            module->State().bounds = {1300, 200, 400, 260};
            runtime.config_.module = &runtime.game_hud_;
            runtime.config_.section = 7;
            runtime.config_.scroll = 6;
            send(WM_LBUTTONDOWN, MK_LBUTTON, 1320, 220);
            const bool notificationGroup = std::string_view{module->Id()}.starts_with("notifications.group");
            const auto* expectedModule = notificationGroup ? &runtime.notifications_ : module;
            if (!check(runtime.config_.module == expectedModule && runtime.ActiveConfigPage().Contains(expectedModule) &&
                runtime.config_.section == (notificationGroup ? 2 : 0) && runtime.config_.scroll == 0 && runtime.interaction_module_ == module,
                "a preview press focuses its module or parent group editor and begins dragging the exact module")) return false;
            send(WM_LBUTTONUP, 0, 1320, 220);
            if (!check(module->State().bounds.x == 1300 && module->State().bounds.y == 200, "a stationary preview click leaves its position unchanged")) return false;
            module->State().bounds = {-1000, -1000, 100, 60};
        }
        runtime.player_.State().bounds = {1300, 200, 400, 260};
        runtime.config_.module = &runtime.player_;
        runtime.config_.section = static_cast<size_t>(bar_config_section::text);
        send(WM_LBUTTONDOWN, MK_LBUTTON, 1320, 220);
        send(WM_MOUSEMOVE, MK_LBUTTON, 1370, 250);
        send(WM_LBUTTONUP, 0, 1380, 260);
        if (!check(runtime.player_.State().bounds.x == 1360 && runtime.player_.State().bounds.y == 240 &&
            runtime.config_.section == static_cast<size_t>(bar_config_section::text), "dragging still uses the release position and keeps the selected module's topic")) return false;
        runtime.alliance_second_.hide_preview_ = true;
        runtime.alliance_second_.State().bounds = {1360, 240, 400, 260};
        send(WM_LBUTTONDOWN, MK_LBUTTON, 1380, 260);
        if (!check(runtime.config_.module == &runtime.player_, "hidden previews cannot steal module selection")) return false;
        send(WM_LBUTTONUP, 0, 1380, 260);
        const auto active = runtime.config_.module;
        send(WM_LBUTTONDOWN, MK_LBUTTON, 80, 42); send(WM_LBUTTONUP, 0, 80, 42);
        if (!check(runtime.config_.module == active, "config header clicks retain the current module")) return false;
        auto& world = runtime.world_info_;
        world.State().enabled = true;
        world.State().bounds = {1300, 700, 300, 60};
        runtime.FocusModuleConfig(world);
        const auto toggleWorld = [&] {
            const auto row = runtime.ConfigLayout().OptionRow(0);
            const int x = static_cast<int>(row.x + row.width * 0.5f), y = static_cast<int>(row.y + row.height * 0.5f);
            send(WM_LBUTTONDOWN, MK_LBUTTON, x, y); send(WM_LBUTTONUP, 0, x, y);
        };
        toggleWorld();
        const auto movable = runtime.ControllerMovableModules();
        if (!check(!world.State().enabled && !world.PreviewVisible() && !runtime.MovementToolbar(viewport) &&
            std::find(movable.begin(), movable.end(), &world) == movable.end(),
            "disabling World Info removes its edit preview, toolbar and controller movement target")) return false;
        runtime.config_.module = &runtime.player_;
        send(WM_MOUSEMOVE, 0, 1320, 720);
        if (!check(runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::arrow,
            "disabled World Info has no drag cursor")) return false;
        send(WM_LBUTTONDOWN, MK_LBUTTON, 1320, 720);
        if (!check(runtime.config_.module == &runtime.player_ && runtime.interaction_module_ == nullptr,
            "a disabled preview cannot capture clicks or begin dragging")) return false;
        send(WM_LBUTTONUP, 0, 1320, 720);
        runtime.FocusModuleConfig(world);
        toggleWorld();
        if (!check(world.PreviewVisible() && world.State().bounds.x == 1300 && world.State().bounds.y == 700,
            "reenabling World Info restores its preview at the same position")) return false;
        *world.Option(4).toggle = true;
        *world.Option(6).choice = 2; world.OnOptionChanged(6);
        for (auto& part : runtime.world_parts_) part.State().bounds = {-1000, -1000, 100, 22};
        for (size_t i = 0; i < runtime.world_parts_.size(); ++i)
        {
            auto& part = runtime.world_parts_[i];
            part.State().bounds = {1300, 700, 160, 22};
            send(WM_LBUTTONDOWN, MK_LBUTTON, 1320, 710);
            if (!check(runtime.config_.module == &world && runtime.config_.section == 1 && world.SelectedPart() == i &&
                runtime.MovementSelectionModule() == &part && runtime.interaction_module_ == &part,
                "independent World Info previews select their own movement handle within the shared Layout page")) return false;
            send(WM_MOUSEMOVE, MK_LBUTTON, 1340, 740); send(WM_LBUTTONUP, 0, 1340, 740);
            const auto targets = runtime.ControllerMovableModules();
            if (!check(part.State().bounds.x == 1320 && part.State().bounds.y == 730 && world.State().bounds.x == 1300 &&
                std::find(targets.begin(), targets.end(), &part) != targets.end(),
                "each World Info piece moves independently and remains available to controller movement")) return false;
            part.State().bounds = {-1000, -1000, 100, 22};
        }
        world.State().enabled = false;
        for (auto& part : runtime.world_parts_)
            if (!check(!part.PreviewVisible(), "disabling World Info removes every independent edit preview")) return false;
        const auto closedSelection = runtime.config_.module;
        runtime.editing_ = false;
        runtime.FocusModuleConfig(runtime.target_);
        return check(runtime.config_.module == closedSelection, "preview focus is limited to edit mode");
    }

    auto config_input_test::ExperienceDisplay(void) -> bool
    {
        hud_runtime runtime;
        runtime.editing_ = true;
        runtime.config_.module = &runtime.experience_;
        runtime.config_.section = 1;
        runtime.config_.bounds = {30, 30, 1100, 600};
        const D3DVIEWPORT8 viewport{0, 0, 1600, 1000, 0, 1};
        auto& exp = runtime.experience_;
        const auto click = [&](const ui::rect& bounds) {
            runtime.mouse_x_ = static_cast<int>(bounds.x + bounds.width * 0.5f);
            runtime.mouse_y_ = static_cast<int>(bounds.y + bounds.height * 0.5f);
            const auto target = runtime.ConfigSoundTarget(viewport);
            runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
            runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
            return target != 0;
        };
        if (!check(runtime.IsExperienceDisplay() && ControlCount(runtime) == 7, "bar-info tab exposes only current progress controls")) return false;
        const auto initial = exp.Settings().readouts;
        if (!check(click(ui::config_controls::Segment(runtime.ConfigLayout().ContentTabs(), 1, 3)) &&
            ControlCount(runtime) == 0 && exp.Settings().readouts == initial,
            "Readouts tab replaces cycling selectors without changing saved preferences")) return false;
        const ui::experience_display_editor::geometry geometry{runtime.ConfigLayout()};
        if (!check(geometry.presets.y + geometry.presets.height <= runtime.ConfigLayout().ContentArea().y + runtime.ConfigLayout().ContentArea().height,
            "readout lists, help and actions stay inside the content area")) return false;
        const auto last = static_cast<size_t>(experience_readout::last);
        if (!check(click(geometry.Row(false, 0)) && exp.Settings().readouts[last], "available reading adds directly without a cycling selector")) return false;
        const auto lastOrder = exp.Settings().order[last];
        for (size_t index = 0; index < experience_readout_count; ++index)
            if (!check(!initial[index] || exp.Settings().order[index] < lastOrder, "newly added readouts append after visible readings")) return false;
        click(ui::config_controls::Segment(geometry.actions, 0, 3));
        if (!check(exp.Settings().order[last] < lastOrder, "Move up operates on adjacent visible items")) return false;
        click(ui::config_controls::Segment(geometry.actions, 2, 3));
        if (!check(!exp.Settings().readouts[last], "Remove hides just the selected reading")) return false;

        const auto allTarget = runtime.experience_display_.Target(exp, runtime.ConfigLayout(), geometry.available.x + 8, geometry.available.y + 10);
        runtime.experience_display_.Scroll(exp, runtime.ConfigLayout(), geometry.available.x + 8, geometry.available.y + 10, -3);
        if (!check(runtime.experience_display_.Target(exp, runtime.ConfigLayout(), geometry.available.x + 8, geometry.available.y + 10) != allTarget,
            "available catalog scrolls independently")) return false;
        click(ui::config_controls::Segment(geometry.filters, 4, 5));
        const auto bonusTarget = runtime.experience_display_.Target(exp, runtime.ConfigLayout(), geometry.available.x + 8, geometry.available.y + 10);
        if (!check(bonusTarget >= 0x200 && experience_readouts[bonusTarget - 0x200].group == experience_readout_group::bonuses,
            "category filters reset scrolling and expose matching available readings")) return false;
        const ui::rect clear{geometry.presets.x + 138, geometry.presets.y, 72, 26};
        click(clear);
        if (!check(std::none_of(exp.Settings().readouts.begin(), exp.Settings().readouts.end(), [](bool value) { return value; }), "Clear removes readouts while retaining bar info")) return false;
        click({geometry.presets.x, geometry.presets.y, 132, 26});
        if (!check(exp.Settings().readouts == DefaultExperienceReadouts() && exp.Settings().order == DefaultExperienceReadoutOrder(),
            "Recommended restores the compact job/session/chain defaults")) return false;

        if (!check(click(ui::config_controls::Segment(runtime.ConfigLayout().ContentTabs(), 2, 3)) && ControlCount(runtime) == 11,
            "Layout groups bar dimensions and readout spacing separately")) return false;
        exp.Settings().bar_gap = 17;
        exp.Settings().info_position = 1;
        exp.Settings().details_text.offset_y = 25;
        exp.Settings().readout_text.offset_y = -12;
        const ui::rect meter{0, 50, 500, 22};
        const auto infoBottom = meter.y + meter.height + std::max(16.0f, runtime.design_system_.TextHeight(exp.Settings().details_text.style)) * 0.5f + 25;
        if (!check(exp.ReadoutArea(runtime.design_system_, meter, true, 16).y == infoBottom + 17 - 12,
            "readout Y moves freely relative to the reserved bar-info space")) return false;
        exp.Settings().show_current = true;
        if (!check(exp.ReadoutRepeatsBar(static_cast<size_t>(experience_readout::buffer)) &&
            !exp.ReadoutRepeatsBar(static_cast<size_t>(experience_readout::received)), "current EXP overlap is identified separately from session gains")) return false;
        auto store = settings_store::FromIniText(""); exp.Save(store);
        experience_module restored; restored.ApplyTextDefaults(ui::theme::CreateDefault()); restored.Load(store);
        if (!check(restored.Settings().readouts == exp.Settings().readouts && restored.Settings().order == exp.Settings().order && restored.Settings().bar_gap == 17,
            "editor visibility, ordering and bar gap use existing settings persistence")) return false;
        const ui::config_layout compact{{0, 0, 936, 350}, 15, false, true, 40};
        const ui::experience_display_editor::geometry compactLists{compact};
        return check(compactLists.rows >= 1 && compactLists.presets.y + compactLists.presets.height <= compact.ContentArea().y + compact.ContentArea().height,
            "the editor remains usable at the minimum config size");
    }

    auto config_input_test::EmptyCategories(void) -> bool
    {
        hud_runtime runtime;
        runtime.editing_ = true;
        runtime.config_.bounds = {40, 40, ui::config_layout::default_width, ui::config_layout::default_height};
        const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
        size_t removed{};
        for (auto* module : runtime.modules_.All())
        {
            runtime.config_.module = module;
            const auto topics = module->ConfigSections();
            if (!topics.empty())
            {
                for (size_t topic = 0; topic < topics.size(); ++topic)
                {
                    runtime.config_.section = topic;
                    if (!check(runtime.GroupOptionCount() != 0, "every custom module topic has settings")) return false;
                }
                continue;
            }
            const auto categories = runtime.ConfigCategories();
            if (!check(categories.count != 0, "each module retains at least one populated category")) return false;
            removed += 5 - categories.count;
            size_t reachable{};
            for (size_t index = 0; index < categories.count; ++index)
            {
                runtime.config_.group = categories.groups[index];
                if (!check(runtime.GroupOptionCount() != 0, "every visible standard category has settings")) return false;
                reachable += runtime.GroupOptionCount();
            }
            if (!check(reachable == module->OptionCount(), "filtering empty categories keeps every setting reachable")) return false;
        }
        const auto click = [&](const ui::rect& bounds) {
            runtime.mouse_x_ = static_cast<int>(bounds.x + bounds.width * 0.5f);
            runtime.mouse_y_ = static_cast<int>(bounds.y + bounds.height * 0.5f);
            runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
            runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
        };
        runtime.config_.module = &runtime.general_;
        runtime.config_.group = module_option_group::text;
        const auto categories = runtime.ConfigCategories();
        if (!check(categories.count == 1 && categories.groups[0] == module_option_group::appearance,
            "General only shows its populated Appearance category")) return false;
        runtime.config_.scroll = 8;
        click(runtime.ConfigLayout().Category(0, categories.count));
        if (!check(runtime.config_.group == module_option_group::appearance && ControlCount(runtime) == 2 && runtime.config_.scroll == 0,
            "switching from a missing category selects a valid category and resets scrolling")) return false;
        if (!check(runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::hand,
            "visible category buttons retain their hover cursor")) return false;
        click(runtime.ConfigLayout().Category(4, 5));
        if (!check(runtime.config_.group == module_option_group::appearance && runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::arrow,
            "removed tabs leave no clickable or hoverable ghost buttons")) return false;
        std::printf("Audited %zu modules; removed %zu empty standard categories.\n", runtime.modules_.Count(), removed);
        return true;
    }

    auto config_input_test::SecondaryMenus(void) -> bool
    {
        hud_runtime runtime;
        runtime.editing_ = true;
        runtime.config_.bounds = {40, 40, ui::config_layout::default_width, ui::config_layout::default_height};
        const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
        const auto click = [&](const ui::rect& bounds) {
            runtime.mouse_x_ = static_cast<int>(bounds.x + bounds.width * 0.5f);
            runtime.mouse_y_ = static_cast<int>(bounds.y + bounds.height * 0.5f);
            runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
            runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
        };
        struct page_case { hud_module* module; size_t page, text_selector; };
        for (const auto test : {page_case{&runtime.player_, 1, 6}, page_case{&runtime.target_, 3, 4}, page_case{&runtime.enemy_list_, 4, 6}})
        {
            click(runtime.ConfigLayout().ModuleTab(test.page));
            const auto topics = test.module->ConfigSections();
            if (!check(runtime.config_.module == test.module && runtime.HasConfigSubmenu() && topics.size() == 7 && runtime.ConfigCategories().count == 0,
                "Player, Target and Enemy List use the secondary menu instead of top categories")) return false;
            std::vector<bool> covered(test.module->OptionCount());
            for (size_t topic = 0; topic < topics.size(); ++topic)
            {
                click(runtime.ConfigLayout().Submenu(topic, topics.size()));
                if (!check(runtime.config_.section == topic && runtime.GroupOptionCount() != 0 &&
                    runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::hand,
                    "each secondary section has settings and shares its click and hover bounds")) return false;
                for (size_t index = 0; index < runtime.GroupOptionCount(); ++index) covered[runtime.GroupOptionIndex(index)] = true;
            }
            if (!check(std::all_of(covered.begin(), covered.end(), [](bool value) { return value; }),
                "secondary sections keep every existing setting reachable")) return false;
            click(runtime.ConfigLayout().OptionRow(0));
            if (!check(runtime.GroupOptionIndex(0) == test.text_selector && *test.module->Option(test.text_selector).choice == 1,
                "each Text section exposes and updates its own text selector")) return false;
            click(runtime.ConfigLayout().Category(4, 5));
            if (!check(runtime.config_.section == static_cast<size_t>(bar_config_section::text) &&
                runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::arrow,
                "the old top category area contains no ghost buttons")) return false;
        }
        runtime.player_.Settings().style_edit_mode = 1;
        click(runtime.ConfigLayout().ModuleTab(1));
        click(runtime.ConfigLayout().Submenu(static_cast<size_t>(bar_config_section::bar_style), 7));
        const bool hpCaps = runtime.player_.Settings().hp_style.themed_bookends, mpCaps = runtime.player_.Settings().mp_style.themed_bookends;
        click(runtime.ConfigLayout().OptionRow(1));
        click(runtime.ConfigLayout().OptionRow(4));
        return check(runtime.player_.Settings().selected_bar == 1 && runtime.player_.Settings().mp_style.themed_bookends != mpCaps &&
            runtime.player_.Settings().hp_style.themed_bookends == hpCaps,
            "bar selectors inside secondary sections edit only the selected resource in Per bar mode");
    }

    auto config_input_test::ChatExtrasMenus(void) -> bool
    {
        hud_runtime runtime;
        runtime.editing_ = true;
        runtime.config_.bounds = {40, 40, ui::config_layout::default_width, ui::config_layout::default_height};
        const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
        const auto click = [&](const ui::rect& bounds) {
            runtime.mouse_x_ = static_cast<int>(bounds.x + bounds.width * 0.5f);
            runtime.mouse_y_ = static_cast<int>(bounds.y + bounds.height * 0.5f);
            runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
            runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
        };
        const auto visit = [&](hud_module& module) {
            const auto topics = module.ConfigSections();
            if (!check(runtime.config_.module == &module && runtime.HasConfigSubmenu() && runtime.ConfigCategories().count == 0,
                "Chat and every Extras module use secondary sections instead of top categories")) return false;
            std::vector<size_t> owners(module.OptionCount());
            for (size_t topic = 0; topic < topics.size(); ++topic)
            {
                click(runtime.ConfigLayout().Submenu(topic, topics.size()));
                if (!check(runtime.config_.section == topic && runtime.GroupOptionCount() != 0 &&
                    runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::hand,
                    "each Chat and Extras section has settings with matching click and hover bounds")) return false;
                for (size_t index = 0; index < runtime.GroupOptionCount(); ++index) ++owners[runtime.GroupOptionIndex(index)];
            }
            return check(std::all_of(owners.begin(), owners.end(), [](size_t count) { return count == 1; }),
                "every existing Chat and Extras option appears in exactly one section");
        };
        click(runtime.ConfigLayout().ModuleTab(10));
        if (!visit(runtime.chat_)) return false;
        click(runtime.ConfigLayout().Section(1));
        if (!check(runtime.config_.section == 4, "switching chat windows retains the matching Text section") || !visit(runtime.chat_second_)) return false;
        click(runtime.ConfigLayout().Section(0));
        click(runtime.ConfigLayout().Submenu(0, 5));
        const bool secondEnabled = runtime.chat_second_.State().enabled;
        click(runtime.ConfigLayout().OptionRow(2));
        if (!check(runtime.GroupOptionIndex(2) == 11 && runtime.chat_second_.State().enabled != secondEnabled,
            "the second chat window toggle remains functional in General")) return false;
        click(runtime.ConfigLayout().Submenu(1, 5));
        const int primarySource = *runtime.chat_.Option(8).choice, secondarySource = *runtime.chat_second_.Option(7).choice;
        click(runtime.ConfigLayout().OptionRow(1));
        if (!check(*runtime.chat_.Option(8).choice == (primarySource + 1) % 3 && *runtime.chat_second_.Option(7).choice == secondarySource,
            "Messages updates the active window's native message source independently")) return false;

        click(runtime.ConfigLayout().ModuleTab(11));
        if (!visit(runtime.compass_)) return false;
        click(runtime.ConfigLayout().Submenu(2, 4));
        click(runtime.ConfigLayout().Section(1, 3));
        if (!check(runtime.config_.module == &runtime.world_info_ && runtime.config_.section == 0 && runtime.GroupOptionIndex(0) == 0,
            "a section missing from the next Extras module falls back to General immediately")) return false;
        if (!visit(runtime.world_info_)) return false;
        click(runtime.ConfigLayout().OptionRow(0));
        if (!check(runtime.GroupOptionIndex(0) == 5 && *runtime.world_info_.Option(5).choice == 1,
            "World Info keeps its text selector alongside the profile controls")) return false;
        click(runtime.ConfigLayout().Section(0, 3));
        if (!check(runtime.config_.section == 0, "different Extras sections do not inherit unrelated indices")) return false;
        runtime.SelectConfigModule(&runtime.commands_);
        runtime.SelectConfigTopic(static_cast<size_t>(native_dialog::merchant));
        return check(runtime.config_.section == static_cast<size_t>(native_dialog::merchant) && visit(runtime.commands_),
            "native merchant settings have an independent populated section");
    }

    auto config_input_test::SettingsSearch(void) -> bool
    {
        hud_runtime runtime;
        runtime.editing_ = true; runtime.config_.bounds = {40, 40, 1100, 600};
        runtime.party_.meters_[0].height = 11; runtime.party_.meters_[1].height = 9;
        runtime.party_.meters_[2].height = 7; runtime.party_.meters_[3].height = 5;
        const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
        const auto layout = runtime.ConfigLayout();
        if (!check(layout.InputMode().width == 24 && layout.HeaderMenu(false).width >= 180 &&
            layout.SearchInput().x + layout.SearchInput().width < layout.InputMode().x &&
            layout.HeaderTitle().x + layout.HeaderTitle().width < layout.SearchInput().x,
            "compact input icon retains readable options and leaves a separate header search field")) return false;
        const auto before = runtime.CaptureSettings().Preferences("").ToIniText();
        const auto started = std::chrono::steady_clock::now();
        const auto collapsedSearch = runtime.SettingsSearchBounds();
        if (!check(collapsedSearch.width == 24 && runtime.ControllerConfigItems(config_controller_region::utilities).back().bounds.width == 24,
            "search starts as a compact icon for pointer and controller focus")) return false;
        runtime.mouse_x_ = static_cast<int>(collapsedSearch.x - 12); runtime.mouse_y_ = static_cast<int>(collapsedSearch.y + 11);
        if (!check(!runtime.ProcessSettingsSearchClick(viewport) && runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::move,
            "unused space left of the compact search icon remains a configuration drag target")) return false;
        runtime.BeginPanelInteraction(viewport);
        if (!check(runtime.interaction_ == hud_runtime::interaction::drag_config,
            "the formerly reserved search area starts dragging the configuration window")) return false;
        runtime.FinishInteraction();
        runtime.mouse_x_ = static_cast<int>(collapsedSearch.x + 12); runtime.mouse_y_ = static_cast<int>(collapsedSearch.y + 11);
        if (!check(runtime.ProcessSettingsSearchClick(viewport), "the search icon opens the existing search action")) return false;
        runtime.config_.search_animation_at = 1000;
        runtime.AnimateSettingsSearch(1090);
        const auto expandingSearch = runtime.SettingsSearchBounds();
        if (!check(expandingSearch.width > 24 && expandingSearch.width < 248 &&
            expandingSearch.x + expandingSearch.width == collapsedSearch.x + collapsedSearch.width &&
            runtime.ConfigHeaderDragArea().x + runtime.ConfigHeaderDragArea().width == expandingSearch.x,
            "search expands leftward smoothly without moving the header's other actions")) return false;
        runtime.AnimateSettingsSearch(1180);
        if (!check(runtime.SettingsSearchBounds().width == 248, "search reaches its full input width")) return false;
        if (!check(runtime.config_.search_open && runtime.KeyboardFocused() && runtime.config_.search_entries.size() > 1000 &&
            runtime.CaptureSettings().Preferences("").ToIniText() == before,
            "building the cross-category search catalog preserves every live preference")) return false;
        std::printf("Settings search: %zu entries in %.2f ms\n", runtime.config_.search_entries.size(),
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count());
        runtime.config_.search_input.SelectAll(); runtime.config_.search_input.Insert("PaRtY HP height"); runtime.UpdateSettingsSearch();
        if (!check(!runtime.config_.search_matches.empty(), "search matches multiple case-insensitive terms across category and HP element paths")) return false;
        const auto found = std::find_if(runtime.config_.search_matches.begin(), runtime.config_.search_matches.end(), [&](const auto index) {
            const auto& entry = runtime.config_.search_entries[index];
            return entry.module == "party" && entry.label_terms.find("height") != std::string::npos && entry.path_terms.find(" hp") != std::string::npos;
        });
        if (!check(found != runtime.config_.search_matches.end(), "party HP height is indexed in its actual element context")) return false;
        const auto entry = runtime.config_.search_entries[*found];
        const auto heights = [&] { return std::array{runtime.party_.meters_[0].height, runtime.party_.meters_[1].height,
            runtime.party_.meters_[2].height, runtime.party_.meters_[3].height}; };
        const auto height = heights();
        if (!check(runtime.SelectSettingsSearch(static_cast<size_t>(found - runtime.config_.search_matches.begin())) &&
            runtime.config_.module == &runtime.party_ && !runtime.config_.search_open && heights() == height &&
            runtime.config_.search_highlight_option == entry.option && runtime.MatchesConfigOption(entry.option),
            "opening a result navigates to the actual setting without editing bar values")) return false;
        runtime.BeginSettingsSearch();
        runtime.config_.search_input.SelectAll(); runtime.config_.search_input.Insert("clear me"); runtime.UpdateSettingsSearch();
        const auto clearInput = runtime.SettingsSearchBounds();
        runtime.mouse_x_ = static_cast<int>(clearInput.x + clearInput.width - 12); runtime.mouse_y_ = static_cast<int>(clearInput.y + 11);
        if (!check(runtime.ProcessSettingsSearchClick(viewport) && runtime.config_.search_open && runtime.config_.search_input.Editor().Text().empty() &&
            runtime.config_.search_input.Focused() && runtime.config_.search_matches.empty(), "the typed-query X clears results while preserving typing focus")) return false;
        runtime.config_.search_input.Insert("party hp"); runtime.UpdateSettingsSearch();
        runtime.HandleSettingsSearchController(controller_action::actions, true);
        if (!check(!runtime.config_.search_input.Editor().Text().empty(), "held controller Actions cannot repeat the clear operation")) return false;
        runtime.HandleSettingsSearchController(controller_action::actions, false);
        if (!check(runtime.config_.search_input.Editor().Text().empty() && runtime.KeyboardFocused(), "controller Actions shares Clear without closing search")) return false;
        runtime.config_.search_input.SelectAll(); runtime.config_.search_input.Insert("no_such_setting_4829"); runtime.UpdateSettingsSearch();
        if (!check(runtime.config_.search_matches.empty() && !runtime.SelectSettingsSearch(0), "no-match searches cannot activate a setting")) return false;
        runtime.HandleSettingsSearchKey(VK_ESCAPE, 0, nullptr);
        if (!check(!runtime.config_.search_open && !runtime.KeyboardFocused(), "Escape releases search input without leaking into chat")) return false;
        runtime.config_.search_animation_at = 2000; runtime.AnimateSettingsSearch(2180);
        runtime.mouse_x_ = static_cast<int>(layout.SearchInput().x + 8);
        if (!check(runtime.SettingsSearchBounds().width == 24 && runtime.SettingsSearchTarget(viewport) == 0 &&
            hud_runtime::Contains(runtime.ConfigHeaderDragArea(), runtime.mouse_x_, runtime.mouse_y_),
            "closing search returns to the icon and releases the expanded input's old hit area")) return false;
        runtime.BeginSettingsSearch();
        runtime.config_.search_input.SelectAll(); runtime.config_.search_input.Insert("party hp"); runtime.UpdateSettingsSearch();
        const ui::settings_search_layout results{runtime.ConfigLayout(), runtime.config_.bounds};
        runtime.MoveSettingsSearch(100);
        if (!check(runtime.config_.search_selection >= runtime.config_.search_scroll &&
            runtime.config_.search_selection < runtime.config_.search_scroll + results.rows,
            "keyboard and controller result navigation keeps the selection visible")) return false;
        runtime.HandleControllerConfig(controller_action::cancel, false);
        if (!check(!runtime.config_.search_open, "controller Cancel shares search dismissal")) return false;
        runtime.general_.party_enabled_ = false;
        runtime.BeginSettingsSearch();
        runtime.config_.search_input.SelectAll(); runtime.config_.search_input.Insert("party hp height"); runtime.UpdateSettingsSearch();
        const auto row = ui::settings_search_layout{runtime.ConfigLayout(), runtime.config_.bounds}.Row(0);
        runtime.mouse_x_ = static_cast<int>(row.x + 10); runtime.mouse_y_ = static_cast<int>(row.y + 10);
        if (!check(runtime.ProcessSettingsSearchClick(viewport) && !runtime.config_.search_open && !runtime.general_.PartyEnabled() &&
            runtime.ConfigPageVisible(runtime.ActiveConfigPage()),
            "search can reveal a disabled category's controls without enabling it")) return false;
        runtime.BeginSettingsSearch();
        runtime.config_.search_input.SelectAll(); runtime.config_.search_input.Insert("party hp font"); runtime.UpdateSettingsSearch();
        const auto font = std::find_if(runtime.config_.search_matches.begin(), runtime.config_.search_matches.end(), [&](const auto index) {
            const auto& item = runtime.config_.search_entries[index];
            return item.module == "party" && item.label == "Font" && item.path.find("HP values") != std::string::npos;
        });
        return check(font != runtime.config_.search_matches.end() &&
            runtime.SelectSettingsSearch(static_cast<size_t>(font - runtime.config_.search_matches.begin())) && runtime.party_.selected_text_ == 3 && heights() == height,
            "search opens the HP text element without changing its bar settings");
    }

    auto config_input_test::ConfigSidebar(void) -> bool
    {
        hud_runtime runtime;
        runtime.editing_ = true; runtime.config_.bounds = {40, 40, 920, 600};
        runtime.config_.module = &runtime.party_; runtime.config_.section = 0;
        const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
        const auto initial = runtime.ConfigLayout();
        const auto toggle = initial.SidebarToggle();
        runtime.mouse_x_ = static_cast<int>(toggle.x + 12); runtime.mouse_y_ = static_cast<int>(toggle.y + 11);
        if (!check(runtime.ConfigSoundTarget(viewport) == 6 && runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::hand &&
            !hud_runtime::Contains(initial.HeaderDragArea(), runtime.mouse_x_, runtime.mouse_y_) && runtime.ProcessConfigClick(viewport),
            "sidebar button owns its click and is separate from window dragging")) return false;
        const auto compact = runtime.ConfigLayout();
        if (!check(runtime.config_.sidebar_collapsed && compact.SidebarWidth() == 52 &&
            compact.ContentArea().x == initial.ContentArea().x - 144 && compact.ContentArea().width > initial.ContentArea().width &&
            initial.Columns() == 1 && compact.Columns() == 2 && runtime.config_.module == &runtime.party_,
            "icon-only sidebar returns width to responsive content without changing the selected page")) return false;
        const auto last = compact.ModuleTab(runtime.VisibleConfigPages().size() - 1);
        for (const auto& button : {compact.ProfileSelector(), compact.ProfileAdd(), compact.ProfileActionsButton()})
            if (!check(button.x > runtime.config_.bounds.x && button.x + button.width < runtime.config_.bounds.x + runtime.config_.bounds.width &&
                button.y > last.y + last.height && button.y >= compact.FooterBackground().y && button.y + button.height < runtime.config_.bounds.y + runtime.config_.bounds.height,
                "profile controls remain in a horizontal footer when the sidebar collapses")) return false;
        const auto pages = runtime.VisibleConfigPages(); const auto category = compact.ModuleTab(2);
        runtime.mouse_x_ = static_cast<int>(category.x + category.width * 0.5f); runtime.mouse_y_ = static_cast<int>(category.y + category.height * 0.5f);
        runtime.DrawConfig(viewport);
        if (!check(runtime.config_.tooltip == pages[2].name && runtime.config_.tooltip_anchor.x == category.x &&
            runtime.ProcessConfigClick(viewport) && pages[2].Contains(runtime.config_.module) &&
            runtime.config_.sidebar_collapsed == (pages[2].SectionCount() <= 1),
            "compact category icons retain name tooltips and their normal click action")) return false;
        if (!runtime.config_.sidebar_collapsed) runtime.ToggleConfigSidebar(viewport);
        const auto saved = runtime.CaptureSettings(); hud_runtime restored; restored.LoadSettings(saved);
        if (!check(restored.config_.sidebar_collapsed && restored.config_.bounds.width == 920,
            "sidebar preference reloads without changing the user's window width")) return false;
        runtime.config_.focus.active = true; runtime.config_.focus.region = config_controller_region::utilities;
        runtime.config_.focus.viewport = viewport; runtime.config_.focus.id = "sidebar";
        runtime.controller_surface_ = hud_runtime::controller_surface::config;
        if (!check(runtime.HandleControllerConfig(controller_action::confirm, false) && !runtime.config_.sidebar_collapsed &&
            runtime.config_.focus.id == "sidebar", "controller activation shares the sidebar toggle and keeps stable focus")) return false;
        runtime.HandleControllerConfig(controller_action::confirm, true);
        if (!check(!runtime.config_.sidebar_collapsed, "held Confirm does not repeatedly toggle the sidebar")) return false;
        runtime.config_.focus.region = config_controller_region::options;
        const auto options = runtime.ControllerConfigItems(config_controller_region::options);
        if (!check(!options.empty(), "settings remain available after expanding the sidebar")) return false;
        runtime.config_.focus.id = options.back().id; runtime.RefreshControllerConfigFocus();
        const auto focused = runtime.config_.focus.id;
        runtime.ToggleConfigSidebar(viewport);
        return check(runtime.config_.focus.id == focused && runtime.config_.sidebar_collapsed,
            "content reflow preserves the focused setting");
    }

    auto config_input_test::ConfigWindowScaling(void) -> bool
    {
        hud_runtime runtime;
        runtime.editing_ = true; runtime.config_.basic = false;
        const D3DVIEWPORT8 viewport{0, 0, 3840, 2160, 0, 1};
        runtime.hud_viewport_ = runtime.config_.focus.viewport = viewport;
        runtime.config_.bounds = {120, 80, 1100, 600};
        runtime.SelectConfigModule(&runtime.general_); runtime.config_.section = general_module::appearance_section;
        runtime.config_.group = module_option_group::appearance; runtime.general_.SelectAppearanceTab(2);
        const auto scaleIndex = runtime.general_.MenuScaleOption();
        const auto option = runtime.general_.Option(scaleIndex);
        if (!check(option.kind == module_option_kind::dropdown && option.choice_count == 5 && runtime.ConfigScale() == 1 &&
            runtime.MatchesConfigOption(scaleIndex), "Menu Scale is a Config Settings control with five choices and a 100% default")) return false;
        const auto original = runtime.config_.bounds;
        const auto player = runtime.player_.State().bounds;
        for (int index = 0; index < 5; ++index)
        {
            option.SetChoice(index);
            const auto visible = runtime.ConfigVisibleBounds();
            const float scale = 1 + .25f * index;
            if (!check(visible.x == original.x && visible.y == original.y && visible.width == original.width * scale &&
                visible.height == original.height * scale && runtime.config_.bounds.width == original.width,
                "menu scaling grows the whole window around its saved position without changing authored dimensions")) return false;
        }
        option.SetChoice(2);
        const auto pointAt = [&](const ui::rect& logical) {
            const auto screen = runtime.ConfigTransform().Map(logical);
            runtime.mouse_x_ = static_cast<int>(screen.x + screen.width * .5f);
            runtime.mouse_y_ = static_cast<int>(screen.y + screen.height * .5f);
        };
        const auto click = [&](const ui::rect& logical) {
            pointAt(logical);
            runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
            runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
        };
        click(runtime.ConfigLayout().Language());
        if (!check(runtime.config_.header_menu_open && runtime.config_.language_menu,
            "the scaled header retains its pointer target")) return false;
        runtime.config_.header_menu_open = false;
        runtime.config_.choice_module = &runtime.general_; runtime.config_.choice_option = scaleIndex;
        runtime.config_.choice_input.Open(option, runtime.ConfigLayout().HeaderTitle());
        click(runtime.config_.choice_input.Row(3, runtime.ConfigViewport(viewport)));
        if (!check(runtime.ConfigScale() == 1.75f && !runtime.config_.choice_input.Opened(),
            "the scaled dropdown changes menu size through the shared pointer action")) return false;
        runtime.config_.choice_module = &runtime.general_; runtime.config_.choice_option = scaleIndex;
        runtime.config_.choice_input.Open(option, runtime.ConfigLayout().HeaderTitle());
        runtime.HandleControllerConfig(controller_action::down, false);
        runtime.HandleControllerConfig(controller_action::confirm, true);
        if (!check(runtime.ConfigScale() == 1.75f && runtime.config_.choice_input.Opened(),
            "held controller Confirm cannot apply a scale choice")) return false;
        runtime.HandleControllerConfig(controller_action::confirm, false);
        if (!check(runtime.ConfigScale() == 2 && !runtime.config_.choice_input.Opened(),
            "fresh controller Confirm applies the same menu scale")) return false;
        const auto snapshot = runtime.CaptureSettings();
        general_module restored; restored.Load(snapshot);
        if (!check(restored.MenuScale() == 2 && snapshot.Preferences("").ReadInt("session.config", "menu_scale", -1) == -1,
            "menu scale persists locally and stays out of shared profiles and HUD presets")) return false;
        restored.ResetDefaults(ui::theme::CreateDefault());
        if (!check(restored.MenuScale() == 2, "resetting HUD defaults retains the installation's menu scale")) return false;

        option.SetChoice(2); runtime.config_.focus.active = false;
        pointAt(runtime.ConfigLayout().ResizeGrip());
        if (!check(runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::resize_diagonal &&
            runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport) && runtime.interaction_ == hud_runtime::interaction::resize_config,
            "the scaled resize grip advertises and owns resizing")) return false;
        runtime.mouse_x_ += 150; runtime.mouse_y_ += 90;
        runtime.ProcessPointerMessage(WM_MOUSEMOVE, MK_LBUTTON, viewport);
        runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
        if (!check(runtime.config_.bounds.width == original.width + 100 && runtime.config_.bounds.height == original.height + 60,
            "scaled pointer resize converts screen movement once")) return false;
        runtime.config_.bounds = original;
        runtime.ApplyConfigResize(original, ui::resize_left, 150, 0, viewport);
        if (!check(runtime.config_.bounds.x == original.x + 150 && runtime.config_.bounds.width == original.width - 100 &&
            runtime.ConfigVisibleBounds().x + runtime.ConfigVisibleBounds().width == original.x + original.width * 1.5f,
            "left-edge resizing keeps the opposite screen edge anchored")) return false;
        runtime.config_.bounds = original;
        pointAt(runtime.ConfigLayout().HeaderDragArea(0));
        runtime.BeginPanelInteraction(viewport); runtime.mouse_down_ = true;
        runtime.mouse_x_ += 90; runtime.mouse_y_ += 60; runtime.UpdateInteraction(viewport);
        runtime.mouse_down_ = false; runtime.FinishInteraction();
        if (!check(runtime.config_.bounds.x == original.x + 90 && runtime.config_.bounds.y == original.y + 60,
            "dragging scaled configuration follows physical pointer movement")) return false;
        runtime.config_.collapsed = true;
        if (!check(runtime.ConfigVisibleBounds().height == 48, "the collapsed title bar retains menu scale")) return false;
        runtime.config_.collapsed = false;
        runtime.SetBasicSettings(true, false);
        click(runtime.ConfigLayout().Language());
        if (!check(runtime.config_.header_menu_open && runtime.ConfigScale() == 1.5f,
            "Basic settings share the scaled header and input geometry")) return false;
        runtime.config_.header_menu_open = false;
        D3DCOLOR pickedColor{};
        runtime.config_.active_color = &pickedColor;
        const auto color = runtime.ConfigColorBounds(viewport);
        if (!check(color.x >= 0 && color.y >= 0 && color.x + color.width <= viewport.Width && color.y + color.height <= viewport.Height,
            "scaled color dialogs stay within the physical viewport")) return false;
        runtime.config_.active_color = nullptr;
        runtime.module_menu_target_ = &runtime.player_;
        if (!check(runtime.ChoiceTransform().Identity(), "HUD module context menus retain screen coordinates")) return false;
        runtime.module_menu_target_ = nullptr;
        option.SetChoice(4); runtime.hud_viewport_ = {0, 0, 1280, 720, 0, 1};
        runtime.ClampLayout(runtime.hud_viewport_);
        const auto visible = runtime.ConfigVisibleBounds();
        return check(visible.x >= 0 && visible.y >= 0 && visible.x + visible.width <= 1280.01f && visible.y + visible.height <= 720.01f &&
            runtime.general_.HudScale() == 1 && runtime.player_.State().bounds.x == player.x && runtime.player_.State().bounds.y == player.y,
            "smaller viewports keep Configuration reachable while HUD scale and positions remain independent");
    }

    auto config_input_test::ConfigResize(void) -> bool
    {
        if (!ConfigWindowScaling()) return false;
        hud_runtime runtime;
        runtime.editing_ = true;
        runtime.config_.bounds = {40, 40, 1100, 600};
        const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
        runtime.config_.focus.viewport = viewport;
        const auto initial = runtime.config_.bounds;
        const auto grip = runtime.ConfigLayout().ResizeGrip();
        runtime.mouse_x_ = static_cast<int>(grip.x + grip.width * 0.5f);
        runtime.mouse_y_ = static_cast<int>(grip.y + grip.height * 0.5f);
        if (!check(runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::resize_diagonal &&
            runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport) && runtime.interaction_ == hud_runtime::interaction::resize_config,
            "the config grip owns a resize gesture")) return false;
        runtime.mouse_x_ += 500; runtime.mouse_y_ += 500;
        if (!check(runtime.ProcessPointerMessage(WM_MOUSEMOVE, MK_LBUTTON, viewport) &&
            runtime.config_.bounds.x == initial.x && runtime.config_.bounds.y == initial.y &&
            runtime.config_.bounds.width == viewport.Width - initial.x && runtime.config_.bounds.height == viewport.Height - initial.y,
            "config resizing remains captured beyond the window and stops at the viewport")) return false;
        runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
        if (!check(runtime.interaction_ == hud_runtime::interaction::none,
            "releasing a resize ends its gesture")) return false;
        runtime.ApplyConfigResize(initial, 2 | 8, -2000, -2000, viewport);
        if (!check(runtime.config_.bounds.width == runtime.ConfigLayout().MinimumWidth() &&
            runtime.config_.bounds.height == runtime.ConfigLayout().MinimumHeight(), "config resize enforces usable minimum dimensions")) return false;
        const auto saved = runtime.CaptureSettings();
        hud_runtime restored;
        restored.LoadSettings(saved);
        if (!check(restored.config_.bounds.width == runtime.config_.bounds.width && restored.config_.bounds.height == runtime.config_.bounds.height,
            "a custom config size survives settings reload")) return false;

        runtime.config_.bounds = initial;
        runtime.BeginConfigResize();
        runtime.HandleConfigResize(controller_action::left, false);
        if (!check(runtime.config_.bounds.width == initial.width - 10 && runtime.CaptureSettings().ReadRect("layout.config", {}).width == initial.width,
            "controller resize previews the same geometry without saving an unfinished edit")) return false;
        runtime.HandleConfigResize(controller_action::confirm, true);
        if (!check(runtime.config_.focus.resizing, "held Confirm cannot commit a resize")) return false;
        runtime.HandleConfigResize(controller_action::cancel, false);
        if (!check(runtime.config_.bounds.width == initial.width && !runtime.config_.focus.resizing,
            "Cancel restores configuration dimensions")) return false;
        runtime.BeginConfigResize();
        runtime.HandleConfigResize(controller_action::actions, false);
        runtime.HandleConfigResize(controller_action::down, false);
        runtime.HandleConfigResize(controller_action::confirm, false);
        if (!check(runtime.config_.bounds.height == initial.height + 1 && !runtime.config_.focus.resizing,
            "controller resize supports fine adjustments and explicit commit")) return false;
        runtime.config_.bounds = initial;
        runtime.BeginConfigResize(true);
        runtime.HandleConfigResize(controller_action::left, false);
        controller_status unavailable{};
        unavailable.enumeration_disabled = true; unavailable.suspended = true;
        unavailable.connection_known = true; unavailable.configuration_known = true;
        runtime.UpdateControllerStatus(unavailable);
        if (!check(runtime.config_.focus.resizing && runtime.config_.focus.resize_keyboard &&
            runtime.config_.bounds.width == initial.width - 10,
            "unavailable controller polls do not cancel keyboard-owned resizing")) return false;
        runtime.SetDevice(nullptr);
        if (!check(!runtime.config_.focus.resizing && runtime.config_.bounds.width == initial.width,
            "actual runtime device suspension cancels keyboard resizing and restores dimensions")) return false;
        runtime.config_.collapsed = true;
        return check(runtime.ConfigResizeEdges(initial.x + 2, initial.y + 2) == 0,
            "collapsed configuration has no hidden resize edges");
    }

    auto config_input_test::Navigation(void) -> bool
    {
        hud_runtime runtime;
        runtime.editing_ = true;
        runtime.config_.bounds = {40, 40, 1100, 600};
        const D3DVIEWPORT8 viewport{0, 0, 1600, 900, 0, 1};
        runtime.mouse_x_ = 1135; runtime.mouse_y_ = 635;
        if (!check(runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::resize_diagonal,
            "the config corner advertises its resize grip")) return false;
        const auto click = [&](const ui::rect& bounds) {
            runtime.mouse_x_ = static_cast<int>(bounds.x + bounds.width * 0.5f);
            runtime.mouse_y_ = static_cast<int>(bounds.y + bounds.height * 0.5f);
            runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
            runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
        };
        const auto pages = runtime.VisibleConfigPages();
        std::set<std::string> ids;
        for (const auto& page : pages)
            if (!check(ids.insert(page.id).second && std::strcmp(page.id, "chat2") != 0 &&
                std::strcmp(page.id, "container") != 0 && std::strcmp(page.id, "gamehud") != 0,
                "sidebar identities are unique without duplicate secondary windows or retired demos")) return false;
        const auto select = [&](const char* id) {
            const auto page = std::find_if(pages.begin(), pages.end(), [&](const auto& item) { return std::strcmp(item.id, id) == 0; });
            if (!check(page != pages.end(), "the requested category is available")) return false;
            click(runtime.ConfigLayout().ModuleTab(static_cast<size_t>(page - pages.begin())));
            return true;
        };
        if (!select("general") || !check(runtime.config_.module == &runtime.general_, "General owns shared settings")) return false;
        if (!select("inventory") || !check(runtime.ActiveConfigPage().Contains(&runtime.equipment_) &&
            runtime.ActiveConfigPage().Contains(&runtime.satchel_), "Inventory groups equipment and Satchel with the dots")) return false;
        if (!select("chat") || !check(runtime.config_.module == &runtime.chat_, "Chat opens the first window editor")) return false;
        click(runtime.ConfigLayout().Section(1));
        if (!check(runtime.config_.module == &runtime.chat_second_ && std::strcmp(runtime.ActiveConfigPage().id, "chat") == 0,
            "the second chat window retains its parent category")) return false;
        const bool firstChat = runtime.chat_.State().enabled, secondChat = runtime.chat_second_.State().enabled;
        click(runtime.ConfigLayout().OptionRow(0));
        if (!check(runtime.chat_second_.State().enabled != secondChat && runtime.chat_.State().enabled == firstChat,
            "secondary-window controls preserve the first window preference")) return false;
        if (!select("party") || !check(runtime.ActiveConfigPage().Count() == 3, "Party owns all three lists")) return false;
        const bool firstParty = runtime.party_.State().enabled, firstAlliance = runtime.alliance_first_.State().enabled;
        const bool secondAlliance = runtime.alliance_second_.State().enabled;
        click(runtime.ConfigLayout().Section(2, 3));
        click(runtime.ConfigLayout().OptionRow(0));
        if (!check(runtime.alliance_second_.State().enabled != secondAlliance && runtime.party_.State().enabled == firstParty &&
            runtime.alliance_first_.State().enabled == firstAlliance, "the third list changes independently")) return false;
        click(runtime.ConfigLayout().Section(0, 3));
        const auto topics = runtime.party_.ConfigSections();
        if (!check(topics.size() == party_module::section_count && runtime.HasConfigSubmenu(), "Party exposes its current shared topic menu")) return false;
        for (size_t topic = 0; topic < topics.size(); ++topic)
        {
            click(runtime.ConfigLayout().Submenu(topic, topics.size()));
            if (!check(runtime.config_.section == topic && runtime.GroupOptionCount() != 0, "topic activation selects a populated page")) return false;
            for (const auto index : runtime.GroupOptionSlots())
            {
                if (index >= runtime.party_.OptionCount()) continue;
                if (!check(runtime.party_.ConfigSectionContains(topic, index), "displayed controls retain their topic identity")) return false;
            }
        }
        runtime.config_.scroll = 4; runtime.config_.active_color = &runtime.party_.low_hp_;
        click(runtime.ConfigLayout().Submenu(party_module::status_section, topics.size()));
        if (!check(runtime.config_.scroll == 0 && runtime.config_.active_color == nullptr,
            "switching topics clears stale scrolling and color editors")) return false;
        click(runtime.ConfigLayout().Section(1, 3));
        if (!check(runtime.config_.module == &runtime.alliance_first_ && runtime.config_.section == party_module::status_section,
            "switching lists retains the topic and original module identity")) return false;
        for (const float height : {350.0f, 600.0f})
        {
            const ui::config_layout layout{{0, 0, 1100, height}, pages.size(), true, true};
            const auto last = layout.Submenu(topics.size() - 1, topics.size()), row = layout.OptionRow(0);
            if (!check(last.y + last.height < height - 20 && last.x + last.width < row.x &&
                layout.Section(0, 3).x < row.x && layout.ContentArea().y <= row.y &&
                layout.ContentArea().y + layout.ContentArea().height <= layout.FooterBackground().y,
                "secondary navigation, controls and footer occupy separate bounds")) return false;
        }
        return select("player") && check(runtime.HasConfigSubmenu() && runtime.config_.section == 0 && runtime.GroupOptionIndex(0) == 0,
            "switching to Player resets the topic to General");
    }
}
