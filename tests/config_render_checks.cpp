#include "config_input_fixture.hpp"

namespace xiui
{
    using namespace config_checks;

    auto config_input_test::WindowFrame(const char* dat) -> bool
    {
        std::ifstream file{dat, std::ios::binary};
        const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>{file}, {}};
        auto window = resources::DecodeMenuWindow(bytes);
        if (!check(window && window->patches[5].additive && !window->patches[3].additive,
            "Type 7 retains its distinct native left/right blend modes")) return false;
        const auto original = window->patches[5];
        auto unchanged = *window;
        ui::container_renderer::CorrectWindowFrame(unchanged, 5);
        if (!check(unchanged.patches[5].additive && unchanged.patches[5].colors == original.colors,
            "Type 7 correction does not change other themes")) return false;
        unchanged.patches[5].colors[0] = 0x50505050;
        ui::container_renderer::CorrectWindowFrame(unchanged, 6);
        if (!check(unchanged.patches[5].additive && unchanged.patches[5].colors[0] == 0x50505050,
            "custom DAT edge colors are preserved")) return false;
        ui::container_renderer::CorrectWindowFrame(*window, 6);
        return check(window->patches[3].colors == std::array<uint32_t, 4>{0x91616161, 0x91616161, 0x91616161, 0x91616161} &&
            window->patches[5].colors == window->patches[3].colors && !window->patches[5].additive &&
            window->patches[5].u == original.u && window->patches[5].v == original.v && window->patches[5].width == original.width &&
            window->patches[5].height == original.height && window->patches[5].texture == original.texture,
            "Type 7 right edge matches the left intensity/blend while retaining its artwork and native color conversion");
    }

    auto config_input_test::NativeWindowBorders(const char* type6Dat, const char* type7Dat, const char* liveLeft) -> bool
    {
        const auto load = [](const char* path) {
            std::ifstream file{path, std::ios::binary};
            return std::vector<uint8_t>{std::istreambuf_iterator<char>{file}, {}};
        };
        const auto window = resources::DecodeMenuWindow(load(type6Dat));
        if (!check(window.has_value(), "decode installed Type 6")) return false;
        const auto& top = window->patches[1]; const auto& left = window->patches[3];
        if (!check(top.additive && left.additive && top.colors == std::array<uint32_t, 4>{0x61808080, 0x61808080, 0x61808080, 0x61808080} &&
            left.colors == std::array<uint32_t, 4>{0x00808080, 0x0D808080, 0x00808080, 0x0D808080},
            "Type 6 uses native load-time color/alpha conversion, including faint gold side edges")) return false;
        if (!check(resources::MenuVertexColorFromRgba(0x00000000) == 0x00010101 &&
            resources::MenuVertexColorFromRgba(0xFFFFFFFF) == 0xFFFFFFFF &&
            resources::MenuVertexColorFromRgba(0xFE123456) == 0xFF573513 &&
            resources::MenuVertexColorFromRgba(0x017F7F7F) == 0x03808080,
            "native conversion retains transparency, clamps channels and truncates alpha like the client")) return false;
        if (liveLeft != nullptr)
        {
            const auto sprite = load(liveLeft);
            if (!check(sprite.size() >= 0x60 + 3 * 28 + 4, "live native sprite snapshot contains four vertices")) return false;
            for (size_t i = 0; i < 4; ++i)
            {
                uint32_t diffuse{}; std::memcpy(&diffuse, sprite.data() + 0x60 + i * 28, 4);
                if (!check(diffuse == left.colors[i], "decoded Type 6 edge matches live native frame diffuse colors exactly")) return false;
            }
        }
        return WindowFrame(type7Dat);
    }

    auto config_input_test::Attribution(void) -> bool
    {
        hud_runtime runtime;
        runtime.editing_ = true;
        runtime.config_.bounds = {40, 40, 1100, 600};
        runtime.config_.scroll = 6;
        const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
        const auto icon = runtime.ConfigLayout().HeaderAction(), close = runtime.ConfigLayout().CloseButton();
        const auto drag = runtime.ConfigLayout().HeaderDragArea();
        if (!check(icon.x + icon.width < close.x && drag.x + drag.width <= icon.x,
            "attribution sits beside close and outside the config drag area")) return false;
        runtime.mouse_x_ = static_cast<int>(icon.x + icon.width * 0.5f);
        runtime.mouse_y_ = static_cast<int>(icon.y + icon.height * 0.5f);
        if (!check(runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::hand && runtime.ConfigSoundTarget(viewport) == 3,
            "attribution shares its draw, cursor and hover-sound target")) return false;
        if (!check(runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport) && runtime.config_.module == &runtime.general_ &&
            runtime.config_.section == general_module::attribution_section && runtime.config_.scroll == 0 &&
            runtime.interaction_ == hud_runtime::interaction::none && runtime.IsEditing(), "header icon opens attribution without closing or dragging config")) return false;
        if (!check(runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport) && runtime.IsEditing(), "attribution press and release remain captured")) return false;
        bool cadence{}, momentum{};
        if (!check(ControlCount(runtime) == ui::attributions.size(), "attribution contains the complete credits list")) return false;
        for (size_t row = 0; row < runtime.GroupOptionCount(); ++row)
        {
            const auto index = runtime.GroupOptionIndex(row);
            const auto option = runtime.general_.Option(index);
            if (!check(option.kind == module_option_kind::action && std::strcmp(option.action_text, "Copy link") == 0 &&
                !runtime.general_.ConfigSectionContains(general_module::enabled_modules_section, index),
                "credit source links are separate from module switches")) return false;
            if (std::strcmp(option.label, "Cadence") == 0) cadence = std::strstr(option.description, "Kipling") != nullptr;
            if (std::strcmp(option.label, "Momentum") == 0) momentum = std::strstr(option.description, "Kipling") != nullptr;
            if (!check(std::strcmp(option.label, "XIVParty") != 0 && std::strcmp(option.label, "XIUI") != 0,
                "XIVParty and XIUI are absent from project credits")) return false;
        }
        if (!check(cadence && momentum, "Kipling is credited for both Cadence and Momentum")) return false;
        const auto click = [&](const ui::rect& bounds) {
            runtime.mouse_x_ = static_cast<int>(bounds.x + bounds.width * 0.5f);
            runtime.mouse_y_ = static_cast<int>(bounds.y + bounds.height * 0.5f);
            const bool down = runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
            const bool up = runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
            return down && up;
        };
        const auto section = ui::config_controls::Segment(runtime.ConfigLayout().ContentTabs(), 1, std::size(ui::attribution_views));
        runtime.mouse_x_ = static_cast<int>(section.x + 4); runtime.mouse_y_ = static_cast<int>(section.y + 4);
        if (!check(runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::hand && runtime.ConfigSoundTarget(viewport) != 0,
            "contributor section uses shared segmented-control cursor and sound targets")) return false;
        if (!check(click(section) && runtime.general_.AttributionView() == 1 && runtime.config_.scroll == 0 &&
            ControlCount(runtime) == ui::xiui_contributors.size(), "Original XIUI shows every contributor in one view")) return false;
        static std::string openedProfile;
        runtime.general_.open_link_ = [](HWND, const char* url) -> bool { openedProfile = url; return true; };
        size_t linked{}, unlinked{}, total{};
        size_t counts[3]{};
        const char* names[]{"Tirem", "Shuu", "JQuick", "Nerf"};
        const char* urls[]{"https://github.com/tirem", "https://github.com/Shuu-37", "https://github.com/jquick", "https://github.com/NerfOnline"};
        for (size_t group = 0; group < std::size(ui::contributor_groups); ++group)
        {
            const ui::contributor_list_layout list{runtime.ConfigLayout()};
            const auto header = list.Header(group);
            runtime.mouse_x_ = static_cast<int>(header.x + 4); runtime.mouse_y_ = static_cast<int>(header.y + 4);
            openedProfile.clear();
            if (!check(list.Visible(header) && runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::arrow &&
                runtime.ConfigSoundTarget(viewport) == 0 && click(header) && openedProfile.empty(),
                "Founders, Core Contributors and Contributors are visible noninteractive list headings")) return false;
        }
        for (size_t row = 0; row < runtime.GroupOptionCount(); ++row)
        {
            const auto index = runtime.GroupOptionIndex(row);
            const auto& contributor = ui::xiui_contributors[index - runtime.general_.ContributorsStart()];
            const auto option = runtime.general_.Option(index);
            if (!check(option.kind == module_option_kind::action && option.action_icon == ui::icon_glyph::github &&
                std::strcmp(option.label, contributor.name) == 0 && option.available == (contributor.url != nullptr) &&
                std::strcmp(option.label, "onimitch") != 0 && std::strcmp(option.label, "tukhubvm") != 0 &&
                !runtime.general_.ConfigSectionContains(general_module::grid_section, index), "curated contributors retain GitHub actions and removed names stay absent")) return false;
            if (row < 4 && !check(std::strcmp(option.label, names[row]) == 0 && std::strcmp(contributor.url, urls[row]) == 0,
                "founder and core display names retain the correct GitHub profiles")) return false;
            ++total; ++counts[static_cast<size_t>(contributor.group)];
            const ui::contributor_list_layout list{runtime.ConfigLayout()};
            const auto member = list.Member(row), button = ui::config_layout::ChoiceButton(member);
            if (!check(list.Visible(member) && member.y >= list.Header(static_cast<size_t>(contributor.group)).y + list.Header(0).height,
                "each contributor appears beneath its category header")) return false;
            runtime.mouse_x_ = static_cast<int>(button.x + 4); runtime.mouse_y_ = static_cast<int>(button.y + 4);
            if (!check((runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::hand) == (contributor.url != nullptr) &&
                (runtime.ConfigSoundTarget(viewport) != 0) == (contributor.url != nullptr), "profile buttons share drawing, cursor and sound geometry")) return false;
            openedProfile.clear();
            if (!check(click(button), "GitHub action press and release stay in config")) return false;
            if (contributor.url != nullptr)
            {
                ++linked;
                if (!check(openedProfile == contributor.url, "GitHub buttons open each contributor's own profile through the link handler")) return false;
            }
            else
            {
                ++unlinked;
                if (!check(openedProfile.empty(), "historical names without linked accounts never open guessed profiles")) return false;
            }
        }
        if (!check(total == ui::xiui_contributors.size() && linked == 11 && unlinked == 1 && counts[0] == 2 && counts[1] == 2 && counts[2] == 8,
            "all curated contributors appear once under the correct group header")) return false;
        runtime.config_.bounds.height = 430;
        const ui::contributor_list_layout compact{runtime.ConfigLayout()};
        if (!check(!compact.Visible(compact.Member(ui::xiui_contributors.size() - 1)), "compact windows keep overflow rows outside the visible list")) return false;
        runtime.config_.scroll = 1000; runtime.ClampOptionScroll();
        const ui::contributor_list_layout scrolled{runtime.ConfigLayout(), runtime.config_.scroll};
        if (!check(runtime.config_.scroll == compact.MaximumScroll() && scrolled.Visible(scrolled.Member(ui::xiui_contributors.size() - 1)),
            "compact contributor lists scroll all the way to the final profile")) return false;
        openedProfile.clear();
        if (!check(click(ui::config_layout::ChoiceButton(scrolled.Member(ui::xiui_contributors.size() - 1))) &&
            openedProfile == ui::xiui_contributors.back().url, "scrolled profile buttons retain matching click targets")) return false;
        if (!check(click(ui::config_controls::Segment(runtime.ConfigLayout().ContentTabs(), 0, std::size(ui::attribution_views))) &&
            ControlCount(runtime) == ui::attributions.size(), "project credits remain reachable after browsing contributors")) return false;
        if (!check(click(ui::config_controls::Segment(runtime.ConfigLayout().ContentTabs(), 2, std::size(ui::attribution_views))) &&
            runtime.general_.AttributionView() == 2 && ControlCount(runtime) == ui::ashita_credits.size(), "Ashita has a separate attribution tab")) return false;
        const auto atomCredit = runtime.general_.Option(runtime.general_.AshitaStart() + 1);
        if (!check(std::strcmp(atomCredit.label, "atom0s") == 0 && std::strstr(atomCredit.description, "Thank you") != nullptr,
            "the Ashita tab includes a short thank-you to atom0s")) return false;
        runtime.config_.focus.active = true;
        runtime.config_.focus.region = config_controller_region::options;
        const auto ashitaItems = runtime.ControllerConfigItems(config_controller_region::options);
        if (!check(ashitaItems.size() == ui::ashita_credits.size(), "Ashita credit links are reachable through shared controller controls")) return false;
        openedProfile.clear();
        runtime.general_.OnOptionChanged(runtime.general_.AshitaStart() + 1);
        if (!check(openedProfile == "https://github.com/atom0s", "the atom0s credit opens the correct profile")) return false;
        return true;
    }

    auto config_input_test::NativeWindowsTab(void) -> bool
    {
        const auto root = std::filesystem::temp_directory_path() / ("xiui-native-windows-" + std::to_string(::GetCurrentProcessId()) + "-" + std::to_string(::GetTickCount64()));
        std::filesystem::create_directory(root);
        struct cleanup { std::filesystem::path root; ~cleanup() { std::error_code ignored; std::filesystem::remove_all(root, ignored); } } cleanupFiles{root};
        hud_runtime runtime;
        runtime.settings_path_ = (root / "xiuinative.ini").string();
        runtime.profiles_.Initialize(runtime.settings_path_);
        runtime.editing_ = true;
        runtime.config_.bounds = {40, 40, 1100, 600};
        runtime.game_hud_.SetAvailable(0xFFFFFFFF);
        const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
        const auto click = [&](const ui::rect& bounds) {
            runtime.mouse_x_ = static_cast<int>(bounds.x + bounds.width * 0.5f);
            runtime.mouse_y_ = static_cast<int>(bounds.y + bounds.height * 0.5f);
            runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
            runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
        };
        const auto pages = runtime.ConfigPages();
        if (!check(pages.front().primary == &runtime.general_ && !pages.back().Contains(&runtime.game_hud_),
            "Game HUD leaves Extras and remains part of General")) return false;
        click(runtime.ConfigLayout().ModuleTab(0));
        click(runtime.ConfigLayout().Submenu(general_module::game_hud_section, runtime.general_.ConfigSections().size()));
        if (!check(runtime.config_.module == &runtime.general_ && runtime.config_.section == general_module::game_hud_section &&
            ControlCount(runtime) == native_hud_options.size() + 3, "General sidebar exposes Game HUD controls and bulk actions")) return false;
        for (size_t i = 0; i < runtime.game_hud_.OptionCount(); ++i)
        {
            const auto expected = i == 0 ? 0 : i == 1 ? game_hud_module::hide_all_option : i == 2 ? game_hud_module::show_all_option : i - 2;
            if (!check(runtime.GroupOptionIndex(i) == runtime.general_.GameHudStart() + expected, "General reuses the native module's controls and order")) return false;
        }
        const auto items = runtime.ControllerConfigItems(config_controller_region::options);
        const auto first = std::find_if(items.begin(), items.end(), [&](const auto& item) { return item.index == runtime.general_.GameHudStart() + 1; });
        if (!check(first != items.end(), "native HUD controls remain in shared controller navigation")) return false;
        click(first->bounds);
        *runtime.general_.Option(general_module::dark_backing_opacity_option).slider = 0.45f;
        const auto saved = runtime.CaptureSettings();
        game_hud_module restoredHud;
        general_module restoredGeneral;
        restoredHud.Load(saved); restoredGeneral.Load(saved);
        if (!check(!*restoredHud.Option(1).toggle && std::abs(restoredGeneral.DarkBackingOpacity() - 0.45f) < 0.01f &&
            !saved.ReadBool("game_hud", ("show_" + std::string{native_hud_options[0].key}).c_str(), true), "native visibility and appearance retain their saved keys")) return false;
        runtime.HandleCommand("general");
        if (!check(runtime.config_.module == &runtime.general_ && runtime.config_.section == general_module::appearance_section,
            "General command retains its Appearance destination")) return false;
        runtime.HandleCommand("gamehud");
        if (!check(runtime.config_.module == &runtime.general_ && runtime.config_.section == general_module::game_hud_section,
            "gamehud command opens General / Game HUD")) return false;
        runtime.general_.OnOptionChanged(runtime.general_.GameHudStart() + game_hud_module::show_all_option);
        if (!check(runtime.game_hud_.HiddenMask() == 0, "General forwards bulk actions to the existing owner")) return false;
        const auto legacy = settings_store::FromIniText("[session.config]\nmodule=gamehud\ntopic_id=general\n");
        runtime.LoadConfigSelection(legacy);
        if (!check(runtime.config_.module == &runtime.general_ && runtime.config_.section == general_module::game_hud_section,
            "old Game HUD navigation reopens at its new destination")) return false;
        runtime.SaveConfigSelection(legacy);
        return check(legacy.ReadString("session.config", "module") == "general" && legacy.ReadString("session.config", "topic_id") == "game-hud",
            "new navigation saves explicit General / Game HUD identities");
    }

    auto config_input_test::DarkBacking(void) -> bool
    {
        const auto path = std::filesystem::temp_directory_path() / ("xiuinative-dark-backing-check-" + std::to_string(::GetCurrentProcessId()) + ".ini");
        struct cleanup { std::filesystem::path path; ~cleanup() { std::error_code ignored; std::filesystem::remove(path, ignored); } } cleanupFile{path};
        hud_runtime runtime;
        runtime.settings_path_ = path.string();
        runtime.config_.bounds = {40, 40, 820, 430};
        runtime.editing_ = true;
        runtime.config_.module = &runtime.general_;
        runtime.config_.group = module_option_group::appearance;
        const auto option = runtime.general_.Option(general_module::dark_backing_opacity_option);
        if (!check(option.available && option.kind == module_option_kind::slider && option.minimum == 0 && option.maximum == 1 &&
            *option.slider == 1, "General exposes an independent dark backing slider with the existing appearance as its default")) return false;
        const auto track = ui::config_layout::SliderTrack(runtime.ConfigLayout().OptionRow(1));
        const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
        const auto originalTheme = runtime.design_system_.GetTheme();
        for (const float value : {0.0f, 0.5f, 1.0f})
        {
            runtime.mouse_x_ = static_cast<int>(track.x + track.width * value);
            runtime.mouse_y_ = static_cast<int>(track.y + track.height * 0.5f);
            runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
            runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
            runtime.FlushSettings();
            general_module restored;
            restored.Load(settings_store{path.string()});
            runtime.design_system_.SetDarkBackingOpacity(restored.DarkBackingOpacity());
            if (!check(std::abs(runtime.design_system_.DarkBackingOpacity() - value) < 0.01f && !runtime.general_.TakeWindowTypeRequest() &&
                runtime.design_system_.GetTheme().colors.border == originalTheme.colors.border &&
                runtime.design_system_.GetTheme().meter.track_opacity == originalTheme.meter.track_opacity,
                "dragging and reloading the backing slider preserves native theme and artwork opacity")) return false;
        }
        runtime.design_system_.SetDarkBackingOpacity(-1);
        if (!check(runtime.design_system_.DarkBackingOpacity() == 0, "negative backing opacity clamps to zero")) return false;
        runtime.design_system_.SetDarkBackingOpacity(2);
        runtime.general_.ResetDefaults(originalTheme);
        return check(runtime.design_system_.DarkBackingOpacity() == 1 && runtime.general_.DarkBackingOpacity() == 1,
            "renderer bounds and resetting General restore full backing opacity");
    }

    auto __cdecl config_input_test::SetWindowType(const uint32_t id, const int value) -> int
    {
        if (id != 0x12) return -1;
        window_record[2] = value;
        ++window_writes;
        return 1;
    }

    auto config_input_test::WindowTheme(void) -> bool
    {
        std::array<uint8_t, 8> config{};
        std::array<uint32_t, 19 * 11> table{};
        const auto configPointer = reinterpret_cast<uintptr_t>(config.data()), tablePointer = reinterpret_cast<uintptr_t>(table.data());
        std::memcpy(config.data() + 4, &tablePointer, sizeof(tablePointer));
        window_record = table.data() + 18 * 11;
        window_record[1] = 18; window_record[2] = 1; window_record[7] = 7;
        window_writes = 0;
        native_window_config client;
        client.config_slot_ = reinterpret_cast<uintptr_t>(&configPointer);
        client.setter_ = reinterpret_cast<uintptr_t>(&SetWindowType);
        if (!check(client.Type() == 1 && client.SetType(4) && client.Type() == 4 && window_writes == 1 &&
            client.SetType(4) && !client.SetType(8) && !client.SetType(-1) && window_writes == 1,
            "Window Type uses the native setter once and rejects invalid or unchanged choices")) return false;
        general_module module;
        module.UpdateWindowType(client.Type());
        auto option = module.Option(general_module::window_theme_option);
        if (!check(option.available && option.choice_count == 8 && *option.choice == 4, "theme control reflects the current client setting")) return false;
        *option.choice = 2;
        module.OnOptionChanged(general_module::window_theme_option);
        module.UpdateWindowType(uint8_t{4});
        const auto request = module.TakeWindowTypeRequest();
        if (!check(*option.choice == 2 && request == 2 && !module.TakeWindowTypeRequest() && client.SetType(*request),
            "a UI change survives snapshot refresh and is consumed once on the game thread")) return false;
        module.UpdateWindowType(client.Type());
        module.ResetDefaults(ui::theme::CreateDefault());
        if (!check(*option.choice == 2 && client.Type() == 2 && window_writes == 2,
            "resetting XIUI preferences does not overwrite the client theme")) return false;
        module.UpdateWindowType(uint8_t{6});
        if (!check(*option.choice == 6, "changes made in the native menu sync back into XIUI")) return false;
        module.UpdateWindowType(std::nullopt);
        window_record[1] = 19;
        return check(!module.Option(general_module::window_theme_option).available && !client.Type() && !client.SetType(0),
            "unavailable or mismatched native settings disable the control");
    }

    auto config_input_test::Presentation(const char* dat) -> bool
    {
        if (!WindowTheme()) return false;
        hud_runtime runtime;
        std::array<bool, static_cast<size_t>(ui::icon_glyph::unknown)> icons{};
        const auto pages = runtime.ConfigPages();
        for (const auto& page : pages)
        {
            const auto id = static_cast<size_t>(ui::config_controls::IconId(page.id));
            if (!check(id < icons.size() && !icons[id], "every config sidebar module has a distinct icon")) return false;
            icons[id] = true;
        }
        auto text = ui::theme::CreateDefault().body_text;
        text.glow.enabled = text.sheen.enabled = true;
        const auto label = ui::config_controls::TextStyle(text), button = ui::config_controls::TextStyle(text, true);
        if (!check(label.shadow.enabled && !label.stroke.enabled && !label.glow.enabled && !label.sheen.enabled &&
            button.shadow.enabled && button.stroke.enabled, "config labels have shadows and button text also has an outline")) return false;
        const auto theme = ui::theme::CreateDefault();
        const auto regular = ui::button_renderer::TextStyle(theme, ui::config_controls::TextStyle(theme.caption_text, true), false);
        const auto active = ui::button_renderer::TextStyle(theme, regular, true);
        const auto hudButton = ui::button_renderer::TextStyle(theme, theme.meter_text, false);
        if (!check(regular.top_color == theme.colors.text && regular.bottom_color == theme.colors.text &&
            active.top_color == theme.colors.control_selected && regular.shadow.enabled && regular.stroke.enabled &&
            hudButton.top_color != hudButton.bottom_color && hudButton.stroke.enabled,
            "shared buttons brighten regular labels, retain active colors, and preserve HUD gradients and effects")) return false;
        ui::design_system paletteDesign;
        paletteDesign.SetControlPalette(0xFFC0C0FE, 0xFFFEC080);
        paletteDesign.SetWindowPalette(0xFF303840);
        if (!check(paletteDesign.GetTheme().colors.control != 0xFFC0C0FE && paletteDesign.GetTheme().colors.control_selected == 0xFFFEC080 &&
            paletteDesign.GetTheme().colors.accent != 0xFFFEC080, "native control highlights stay warm across window changes without recoloring HUD accents")) return false;
        for (const float height : {350.0f, 430.0f, 720.0f})
        for (const bool sections : {false, true})
        {
            const ui::config_layout layout{{0, 0, 820, height}, pages.size(), sections};
            const auto last = layout.ModuleTab(pages.size() - 1);
            if (!check(last.y + last.height <= layout.ProfileSelector().y, "all sidebar rows clear profile controls even at minimum height")) return false;
            const auto row = layout.OptionRow(layout.VisibleSlots() - 1);
            const auto control = ui::config_layout::ChoiceButton(row), description = ui::config_layout::Description(row);
            if (!check(control.y + control.height < description.y && description.x + description.width <= row.x + row.width &&
                description.y + description.height <= height - 24, "option controls and descriptions stay separate and clear the footer")) return false;
        }
        const D3DVIEWPORT8 viewport{20, 10, 1280, 720, 0, 1};
        const auto tip = ui::config_layout::Tooltip({1250, 690, 40, 24}, 300, viewport);
        if (!check(tip.x >= 28 && tip.x + tip.width <= 1292 && tip.y + tip.height <= 722,
            "edge tooltips remain inside the viewport")) return false;
        const auto below = ui::config_layout::Tooltip({100, 100, 280, 56}, 300, viewport);
        if (!check(below.y > 156, "option descriptions place their tooltip below the controls")) return false;
        if (dat == nullptr) return true;
        std::ifstream file{dat, std::ios::binary};
        const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>{file}, {}};
        bool haveButtons{}, haveHighlight{};
        for (size_t at = 0; at + 32 <= bytes.size();)
        {
            uint32_t meta{}; std::memcpy(&meta, bytes.data() + at + 4, 4);
            const size_t size = ((meta >> 7) & 0xFFFFF) * size_t{16};
            if (size < 16 || size > bytes.size() - at) break;
            if ((meta & 0x7F) == 0x31 && std::string_view{reinterpret_cast<const char*>(bytes.data() + at + 16), 16} == "menu    windowps")
            {
                auto definition = resources::DecodeMenuSprite(std::span{bytes}.subspan(at + 16, size - 16), 13);
                if (!check(definition.has_value(), "decode the native button definition")) return false;
                auto& quads = definition->front();
                std::erase_if(quads, [](const auto& q) { return q.texture != "buttonto"; });
                if (!check(quads.size() == 2 && quads[0].u == 16 && quads[0].v == 0 && quads[0].width == 48 &&
                    quads[1].u == 0 && quads[1].v == 16 && quads[1].width == 40 && quads[1].points[6] == 88 &&
                    quads[0].additive && quads[1].additive && quads[0].colors[0] == 0x7060607F,
                    "native two-piece button geometry, blue vertex color, UVs and blend survive extraction")) return false;
                haveButtons = true;
            }
            else if ((meta & 0x7F) == 0x31 && std::string_view{reinterpret_cast<const char*>(bytes.data() + at + 16), 16} == "menu    ncol    ")
            {
                const auto palette = resources::DecodeMenuSprite(std::span{bytes}.subspan(at + 16, size - 16), 8);
                if (!check(palette && palette->front().size() == 1 && palette->front().front().colors[0] == 0x7F7F6040,
                    "native orange palette entry preserves red/blue channel order and alpha")) return false;
                haveHighlight = true;
            }
            at += size;
        }
        return check(haveButtons && haveHighlight, "native button and highlight DAT entries must be present");
    }
}
