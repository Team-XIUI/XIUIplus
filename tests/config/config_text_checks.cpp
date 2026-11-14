#include "support/config_input_fixture.hpp"
#include "modules/chat/chat_theme.hpp"

namespace xiui
{
    using namespace config_checks;

    auto config_input_test::ConfigText(void) -> bool
    {
        hud_runtime runtime; runtime.editing_ = true; runtime.config_.module = &runtime.general_;
        runtime.config_.section = general_module::appearance_section; runtime.config_.bounds = {40, 40, 1100, 600};
        const D3DVIEWPORT8 viewport{0, 0, 1600, 900, 0, 1};
        const auto click = [&](const ui::rect& at) {
            runtime.mouse_x_ = static_cast<int>(at.x + at.width * 0.5f); runtime.mouse_y_ = static_cast<int>(at.y + at.height * 0.5f);
            const bool press = runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
            return runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport) && press;
        };
        const auto topics = runtime.ConfigTopicRows();
        const auto settings = std::find_if(topics.begin(), topics.end(), [](const auto& row) { return std::strcmp(row.label, "Config Settings") == 0; });
        if (!check(settings != topics.end() && click(runtime.ConfigLayout().Submenu(static_cast<size_t>(settings - topics.begin()), topics.size())) &&
            runtime.general_.AppearanceTab() == 2 && runtime.MatchesConfigOption(runtime.general_.MenuScaleOption()) &&
            runtime.MatchesConfigOption(runtime.general_.ExtraSettingsStart()) && runtime.GroupOptionIndex(0) == runtime.general_.MenuScaleOption() &&
            runtime.GroupOptionIndex(1) == runtime.general_.ExtraSettingsStart() && runtime.GroupOptionIndex(2) == runtime.general_.ConfigTextStart() &&
            runtime.GroupOptionIndex(3) == runtime.general_.ConfigTextStart() + 15,
            "Config Settings unifies menu scale, editing behavior, font and text size in one secondary-column destination")) return false;
        runtime.general_.SelectAppearanceTab(0);
        if (!check(!runtime.MatchesConfigOption(runtime.general_.MenuScaleOption()) && !runtime.MatchesConfigOption(runtime.general_.ExtraSettingsStart()) &&
            std::none_of(topics.begin(), topics.end(), [](const auto& row) { return row.id == "general/extra-settings" || std::strcmp(row.label, "Config Text") == 0; }),
            "window appearance and the secondary menu have no duplicate configuration destinations")) return false;
        runtime.general_.SelectAppearanceTab(2);
        auto& hud = runtime.player_.Settings().name_text.style; hud.font = 2; hud.letter_spacing = 0.75f; hud.scale = 1.1f;
        if (!check(click(ui::config_layout::ChoiceButton(runtime.ConfigLayout().OptionRow(2))) && runtime.config_.choice_input.Opened(),
            "Config font opens the shared four-family dropdown")) return false;
        runtime.SelectChoiceInput(3);
        runtime.BuildSettingsSearch();
        for (const auto index : {runtime.general_.MenuScaleOption(), runtime.general_.ExtraSettingsStart(), runtime.general_.ConfigTextStart()})
        {
            const auto found = std::find_if(runtime.config_.search_entries.begin(), runtime.config_.search_entries.end(), [&](const auto& entry) {
                return entry.module == "general" && entry.option == index;
            });
            if (!check(found != runtime.config_.search_entries.end() && found->path.find("Config Settings") != std::string::npos && found->appearance_tab == 2,
                "settings search uses the unified Config Settings destination")) return false;
        }
        *runtime.general_.Option(runtime.general_.ConfigTextStart() + 1).slider = 1.5f;
        runtime.general_.OnOptionChanged(runtime.general_.ConfigTextStart() + 1);
        const auto size = runtime.general_.Option(runtime.general_.ConfigTextStart() + 15);
        if (!check(size.kind == module_option_kind::slider && *size.slider == 14 && size.minimum == 8 && size.maximum == 18,
            "Config text size appears beside font with the 14-pixel default")) return false;
        *size.slider = 16;
        runtime.general_.OnOptionChanged(runtime.general_.ConfigTextStart() + 15);
        if (!check(hud.font == 2 && hud.letter_spacing == 0.75f && hud.scale == 1.1f && runtime.general_.GlobalFont() == 0,
            "config typography never changes HUD text preferences")) return false;
        for (const float scale : {0.68f, 0.72f, 0.82f, 1.0f})
        {
            auto source = runtime.design_system_.GetTheme().body_text; source.scale = scale;
            const auto style = runtime.config_controls_.Style(source, true);
            auto unspaced = style; unspaced.letter_spacing = 0;
            if (!check(style.font == 3 && style.shadow.enabled && style.stroke.enabled && !style.glow.enabled &&
                std::abs(style.scale - scale * 16 / 12) < 0.0001f &&
                std::abs(style.letter_spacing * ui::TextPixelScale(style.scale) - 1.5f) < 0.0001f &&
                std::abs(runtime.design_system_.MeasureText("ABC", style) - runtime.design_system_.MeasureText("ABC", unspaced) - 3) < 0.0001f,
                "every config size measures the chosen font with consistent pixel spacing and normal config effects")) return false;
        }
        ui::share_key_view view;
        const std::string key(96, 'A'); const ui::rect keyBounds{0, 0, 140, 100};
        view.Draw(runtime.design_system_, runtime.config_controls_, keyBounds, key, false);
        *runtime.general_.Option(runtime.general_.ConfigTextStart()).choice = 1;
        *runtime.general_.Option(runtime.general_.ConfigTextStart() + 1).slider = 4;
        view.Draw(runtime.design_system_, runtime.config_controls_, keyBounds, key, false);
        auto keyStyle = runtime.design_system_.GetTheme().body_text; keyStyle.scale = 0.74f;
        keyStyle = runtime.config_controls_.Style(keyStyle);
        for (const auto& row : view.Lines())
            if (!check(runtime.design_system_.MeasureText(row.c_str(), keyStyle) <= keyBounds.width - 32,
                "share keys reflow when font or letter spacing changes")) return false;
        const auto effect = [&](const size_t index) { return runtime.general_.Option(runtime.general_.ConfigTextStart() + index); };
        *effect(3).color = 0x80403020; *effect(4).slider = -3; *effect(5).slider = 4; *effect(6).toggle = true;
        *effect(7).choice = 2; *effect(8).color = 0xA0102030;
        *effect(9).toggle = true; *effect(10).color = 0x40506070;
        *effect(11).toggle = true; *effect(12).color = 0x708090A0; *effect(13).slider = 28; *effect(14).slider = 0.5f;
        const auto styled = runtime.config_controls_.Style(runtime.design_system_.GetTheme().body_text);
        if (!check(styled.shadow.color == 0x80403020 && styled.shadow.offset_x == -3 && styled.shadow.offset_y == 4 && styled.shadow.soft &&
            styled.stroke.enabled && styled.stroke.color == 0xA0102030 && styled.glow.enabled && styled.glow.color == 0x40506070 &&
            styled.sheen.enabled && styled.sheen.color == 0x708090A0 && styled.sheen.width == 28 && styled.sheen.speed == 0.5f &&
            hud.font == 2 && hud.letter_spacing == 0.75f,
            "config controls resolve live shared effects, offsets and alpha independently of HUD preferences")) return false;
        auto store = settings_store::FromIniText(""); runtime.general_.Save(store);
        general_module restored; restored.Load(store);
        const auto& saved = restored.ConfigText();
        if (!check(saved.font == 1 && saved.size == 16 && saved.letter_spacing == 4 && saved.shadow.color == styled.shadow.color && saved.shadow.offset_x == -3 &&
            saved.shadow.offset_y == 4 && saved.shadow.soft && saved.outline == 2 && saved.outline_color == styled.stroke.color &&
            saved.glow.enabled && saved.glow.color == styled.glow.color && saved.sheen.enabled && saved.sheen.color == styled.sheen.color &&
            saved.sheen.width == 28 && saved.sheen.speed == 0.5f,
            "config typography and effects round trip through normal profile preferences")) return false;
        auto legacy = settings_store::FromIniText("[config.text]\nfont=2\n"); restored.Load(legacy);
        if (!check(restored.ConfigText().size == 14, "profiles without a config size use the 14-pixel default")) return false;
        *effect(2).toggle = false; *effect(7).choice = 0; *effect(9).toggle = false; *effect(11).toggle = false;
        const auto disabled = runtime.config_controls_.Style(runtime.design_system_.GetTheme().body_text, true);
        if (!check(!disabled.shadow.enabled && !disabled.stroke.enabled && !disabled.glow.enabled && !disabled.sheen.enabled,
            "config effects can all be disabled, including button outlines")) return false;
        runtime.general_.ResetDefaults(runtime.design_system_.GetTheme());
        const auto reset = runtime.config_controls_.Style(runtime.design_system_.GetTheme().body_text);
        runtime.editing_ = false;
        return check(reset.font == 0 && std::abs(reset.scale - runtime.design_system_.GetTheme().body_text.scale * 14 / 12) < 0.0001f && reset.letter_spacing == 0 && reset.shadow.enabled && !reset.stroke.enabled &&
            runtime.config_controls_.Style(reset, true).stroke.enabled && !reset.glow.enabled && !reset.sheen.enabled,
            "Reset defaults restores config typography and button-only outlines through the live settings binding");
    }

    auto __cdecl config_input_test::PreviewSound(void) -> void { ++preview_sounds; }

    auto config_input_test::ChatSoundPreview(void) -> bool
    {
        hud_runtime runtime; runtime.editing_ = true; runtime.config_.module = &runtime.chat_; runtime.config_.section = 5;
        runtime.config_.bounds = {40, 40, 1100, 600};
        runtime.ui_sounds_.sounds_[3].function = reinterpret_cast<uintptr_t>(&PreviewSound);
        runtime.ui_sounds_.sounds_[7].function = reinterpret_cast<uintptr_t>(&PreviewSound);
        runtime.ui_sounds_.sounds_[1].function = reinterpret_cast<uintptr_t>(&SelectSound);
        const D3DVIEWPORT8 viewport{0, 0, 1600, 900, 0, 1};
        const auto send = [&](uint32_t message, uint32_t buttons, const ui::rect& at) {
            runtime.mouse_x_ = static_cast<int>(at.x + 3); runtime.mouse_y_ = static_cast<int>(at.y + 3);
            return runtime.ProcessPointerMessage(message, buttons, viewport);
        };
        const auto click = [&](const ui::rect& at) { send(WM_LBUTTONDOWN, MK_LBUTTON, at); return send(WM_LBUTTONUP, 0, at); };
        const auto option = runtime.chat_.Option(runtime.GroupOptionIndex(1));
        click(runtime.ConfigLayout().OptionRow(1));
        const auto button = runtime.config_.choice_input.PreviewButton(4, viewport);
        const auto confirmations = select_sounds; preview_sounds = 0;
        if (!check(click(button) && preview_sounds == 1 && select_sounds == confirmations && runtime.config_.choice_input.Opened() &&
            *option.choice == 1 && runtime.KeyboardFocused() && runtime.mouse_buttons_ == 0,
            "a play icon previews once without a confirm cue, selection, close or leaked pointer release")) return false;
        send(WM_LBUTTONDOWN, MK_LBUTTON, button); send(WM_LBUTTONDOWN, MK_LBUTTON, button); send(WM_LBUTTONUP, 0, button);
        if (!check(preview_sounds == 2, "duplicate pointer messages cannot repeat a preview")) return false;
        runtime.HandleChoiceKey(VK_SPACE, 0); runtime.HandleChoiceKey(VK_SPACE, 0x40000000);
        runtime.HandleChoiceKey(VK_SPACE, static_cast<LPARAM>(0x80000000u));
        if (!check(preview_sounds == 3 && *option.choice == 1 && runtime.config_.choice_input.Opened(),
            "Space previews the highlighted sound once and held/released keys do not repeat it")) return false;
        if (!check(!runtime.PreviewChoiceInput(0), "Off has no sound preview")) return false;
        runtime.config_.choice_input.Scroll(1);
        if (!check(click(runtime.config_.choice_input.PreviewButton(8, viewport)) && preview_sounds == 4 && *option.choice == 1,
            "scrolled sound rows keep independent preview hit areas")) return false;
        click(runtime.config_.choice_input.Row(8, viewport));
        if (!check(*option.choice == 8 && !runtime.config_.choice_input.Opened() && !runtime.KeyboardFocused(),
            "clicking the row still selects the clip normally after previewing")) return false;
        runtime.editing_ = false;
        return true;
    }

    auto config_input_test::ChatCustomization(void) -> bool
    {
        hud_runtime runtime; runtime.editing_ = true; runtime.config_.module = &runtime.chat_; runtime.config_.section = 5;
        runtime.config_.bounds = {40, 40, 1100, 600};
        const D3DVIEWPORT8 viewport{0, 0, 1600, 900, 0, 1};
        const auto send = [&](uint32_t message, uint32_t buttons, const ui::rect& bounds) {
            runtime.mouse_x_ = static_cast<int>(bounds.x + 3); runtime.mouse_y_ = static_cast<int>(bounds.y + 3);
            return runtime.ProcessPointerMessage(message, buttons, viewport);
        };
        const auto click = [&](const ui::rect& bounds) {
            const bool press = send(WM_LBUTTONDOWN, MK_LBUTTON, bounds);
            return send(WM_LBUTTONUP, 0, bounds) && press;
        };
        const auto soundRow = runtime.ConfigLayout().OptionRow(1);
        const auto sound = runtime.chat_.Option(runtime.GroupOptionIndex(1));
        if (!check(sound.kind == module_option_kind::dropdown && *sound.choice == 1 && click(ui::config_layout::ChoiceButton(soundRow)) &&
            runtime.config_.choice_input.Opened() && runtime.KeyboardFocused(), "sound selection opens a real dropdown and owns keyboard input")) return false;
        runtime.HandleChoiceKey(VK_DOWN, 0); runtime.HandleChoiceKey(VK_RETURN, 0);
        if (!check(*sound.choice == 2 && !runtime.config_.choice_input.Opened() && !runtime.KeyboardFocused(), "arrows and Enter commit the highlighted sound")) return false;
        click(soundRow); runtime.HandleChoiceKey(VK_END, 0); runtime.HandleChoiceKey(VK_ESCAPE, 0);
        if (!check(*sound.choice == 2 && !runtime.KeyboardFocused(), "Escape dismisses sound selection without applying it")) return false;
        click(soundRow);
        const auto row = runtime.config_.choice_input.Row(3, viewport);
        if (!check(click(row) && *sound.choice == 3 && !runtime.config_.choice_input.Opened() && runtime.mouse_buttons_ == 0,
            "pointer selection consumes both click halves and commits the chosen sound")) return false;
        click(soundRow);
        const bool enabled = runtime.chat_.State().enabled;
        if (!check(click(runtime.ConfigLayout().ModuleTab(0)) && !runtime.config_.choice_input.Opened() && runtime.config_.module == &runtime.chat_ &&
            runtime.chat_.State().enabled == enabled, "an outside dropdown click closes it without activating the config beneath")) return false;
        ui::choice_dropdown nearEdge; nearEdge.Open(sound, {1550, 880, 148, 24});
        const auto menu = nearEdge.Bounds(viewport);
        if (!check(menu.x >= 0 && menu.y >= 0 && menu.x + menu.width <= 1600 && menu.y + menu.height <= 900,
            "dropdown menus stay inside the viewport near its bottom/right edges")) return false;
        click(soundRow); runtime.config_.section = 6; runtime.NormalizeConfigSelection();
        if (!check(!runtime.config_.choice_input.Opened() && !runtime.KeyboardFocused(), "changing config context dismisses stale sound choices")) return false;
        const auto color = runtime.chat_.Option(runtime.GroupOptionIndex(0));
        const auto colorRow = runtime.ConfigLayout().OptionRow(0);
        if (!check(color.kind == module_option_kind::color_override && !*color.toggle, "chat colors start in Game mode")) return false;
        click(ui::config_layout::OverrideSource(colorRow));
        if (!check(*color.toggle, "source control enables the XIUI+ color override")) return false;
        click(ui::config_layout::OverrideSwatch(colorRow));
        if (!check(runtime.config_.active_color == color.color, "override swatches open the shared color picker")) return false;
        runtime.config_.color_picker.Open(0xFF5599BB); runtime.ApplyPickedColor();
        const auto custom = *color.color;
        click(ui::config_layout::OverrideSource(colorRow));
        if (!check(!*color.toggle && *color.color == custom && runtime.config_.active_color == nullptr,
            "returning to Game keeps the chosen custom color and closes its picker")) return false;
        runtime.editing_ = false;
        return true;
    }

    auto config_input_test::ChatAlertInput(void) -> bool
    {
        hud_runtime runtime;
        runtime.editing_ = true;
        runtime.config_.module = &runtime.chat_;
        runtime.config_.section = 5;
        runtime.config_.bounds = {40, 40, 1100, 600};
        const D3DVIEWPORT8 viewport{0, 0, 1600, 900, 0, 1};
        const auto keywordIndex = runtime.GroupOptionIndex(0);
        const auto option = runtime.chat_.Option(keywordIndex);
        if (!check(option.kind == module_option_kind::text_input && option.text != nullptr, "Sounds starts with an editable keywords field")) return false;
        const auto field = ui::config_layout::TextValue(runtime.ConfigLayout().OptionRow(0));
        const auto click = [&](const ui::rect& bounds) {
            runtime.mouse_x_ = static_cast<int>(bounds.x + 3); runtime.mouse_y_ = static_cast<int>(bounds.y + 3);
            runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
            runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
        };
        click(field);
        if (!check(runtime.config_.text_input.Focused() && runtime.KeyboardFocused() && !runtime.ChatFocused(),
            "keyword editing owns the keyboard instead of the chat composer")) return false;
        runtime.config_.text_input.Insert("Alpha, beta");
        runtime.HandleConfigInputKey(VK_RETURN, 0);
        if (!check(*option.text == "Alpha, beta" && !runtime.KeyboardFocused(), "Enter applies keywords and releases keyboard ownership")) return false;
        click(field); runtime.config_.text_input.Insert("discard this"); runtime.HandleConfigInputKey(VK_ESCAPE, 0);
        if (!check(*option.text == "Alpha, beta" && !runtime.config_.text_input.Focused(), "Escape discards a keyword edit")) return false;
        click(field); runtime.config_.text_input.Insert(std::string(120, 'a'));
        if (!check(runtime.config_.text_input.Editor().Text().size() == 120, "keyword lists can exceed the profile-name field's length")) return false;
        runtime.config_.text_input.Insert(std::string(501, 'b'));
        if (!check(runtime.config_.text_input.Editor().Text().size() == 120, "overlong keyword input is rejected without truncating the draft")) return false;
        runtime.HandleConfigInputKey(VK_TAB, 0);
        if (!check(option.text->size() == 120 && !runtime.KeyboardFocused(), "Tab applies a longer keyword list")) return false;
        click(field); runtime.config_.text_input.Insert("outside, click");
        click(runtime.ConfigLayout().OptionRow(1));
        if (!check(*option.text == "outside, click" && !runtime.config_.text_input.Focused() && runtime.config_.choice_input.Opened(), "clicking another control applies keywords before its own action")) return false;
        runtime.CloseChoiceInput();
        click(field); runtime.config_.text_input.Insert("cancel on navigation");
        runtime.config_.section = 0; runtime.NormalizeConfigSelection();
        if (!check(*option.text == "outside, click" && !runtime.KeyboardFocused(), "a changed config context cancels stale text editing")) return false;
        ui::text_input profileName; profileName.Open(); profileName.Insert(std::string(65, 'a'));
        if (!check(profileName.Editor().Text().empty(), "profile name inputs retain their original length limit")) return false;
        runtime.editing_ = false;
        return true;
    }

    auto config_input_test::ThemedButtons(void) -> bool
    {
        ui::design_system design;
        design.SetControlPalette(0xFFC0C0FE, 0xFFFEC080);
        const auto native = design.GetTheme().colors.control;
        design.SetWindowPalette(0xFF203050);
        const auto blue = design.GetTheme().colors.control;
        design.SetWindowPalette(0xFF605030);
        if (!check(native != blue && blue != design.GetTheme().colors.control && design.GetTheme().colors.control_selected == 0xFFFEC080,
            "shared button faces follow the window palette and retain the native orange hover")) return false;
        const auto path = std::filesystem::temp_directory_path() / ("XIUIplus-button-palette-check-" + std::to_string(::GetCurrentProcessId()) + ".ini");
        struct cleanup { std::filesystem::path path; ~cleanup() { std::error_code ignored; std::filesystem::remove(path, ignored); } } cleanupFile{path};
        { std::ofstream theme{path}; theme << "[colors]\ncontrol=FF123456\ncontrol_selected=FFABCDEF\n"; }
        design.SetThemePath(path.string().c_str());
        design.SetWindowPalette(0xFF304030);
        return check(design.GetTheme().colors.control == 0xFF123456 && design.GetTheme().colors.control_selected == 0xFFABCDEF,
            "explicit button color overrides survive Window Type changes");
    }

    auto config_input_test::GlobalText(void) -> bool
    {
        hud_runtime runtime;
        runtime.editing_ = true; runtime.config_.module = &runtime.general_;
        runtime.config_.section = general_module::appearance_section;
        runtime.config_.bounds = {40, 40, 1100, 600};
        const auto topics = runtime.ConfigTopicRows();
        const auto textTopic = std::find_if(topics.begin(), topics.end(), [](const auto& row) { return row.subview == 1; });
        if (!check(textTopic != topics.end(), "Global Text has a shared topic destination")) return false;
        runtime.SelectConfigTopicRow(static_cast<size_t>(textTopic - topics.begin()));
        if (!check(runtime.general_.AppearanceTab() == 1 &&
            runtime.GroupOptionIndex(0) == runtime.general_.GlobalTextStart() + static_cast<size_t>(text_property::font),
            "Global Text opens with its font control first")) return false;
        auto& name = runtime.player_.Settings().name_text;
        name.style.top_color = 0xAABBCCDD; name.style.bottom_color = 0x88997766;
        name.style.shadow.enabled = name.style.stroke.enabled = name.style.glow.enabled = true;
        name.offset_x = 13; name.offset_y = -7;
        const auto sizeIndex = runtime.general_.GlobalTextStart();
        *runtime.general_.Option(sizeIndex).slider = 1.4f;
        runtime.general_.OnOptionChanged(sizeIndex);
        size_t styles{};
        bool sizes = true;
        for (auto* module : runtime.modules_.All())
            module->VisitTextSettings([&](ui::text_style& style) { ++styles; sizes &= style.scale == 1.4f; });
        if (!check(sizes && styles > 70, "global size reaches hidden modules, both chat windows, alliances, timers and recipient text")) return false;
        name.style.scale = 0.8f;
        const auto family = runtime.general_.Option(sizeIndex + 1);
        if (!check(family.kind == module_option_kind::dropdown && family.choice_count == static_cast<size_t>(ui::FontCount()) && ui::FontCount() > ui::builtin_font_count && std::strcmp(family.choices[3], "Grammara") == 0,
            "global fonts include all bundled and Windows faces")) return false;
        *family.choice = 3; runtime.general_.OnOptionChanged(sizeIndex + 1);
        bool fonts = true;
        for (auto* module : runtime.modules_.All()) module->VisitTextSettings([&](ui::text_style& style) { fonts &= style.font == 3 && style.font_explicit; });
        if (!check(fonts && name.style.scale == 0.8f && name.style.top_color == 0xAABBCCDD && name.style.bottom_color == 0x88997766 &&
            name.style.shadow.enabled && name.style.stroke.enabled && name.style.glow.enabled && name.offset_x == 13 && name.offset_y == -7,
            "font changes preserve module size overrides, colors, opacity, text effects and offsets")) return false;
        name.style.font = 2;
        const auto global = [&](const text_property property) { return runtime.general_.Option(sizeIndex + static_cast<size_t>(property)); };
        *global(text_property::letter_spacing).slider = 1.25f;
        *global(text_property::shadow).toggle = false;
        *global(text_property::shadow_color).color = 0x80403020;
        *global(text_property::shadow_x).slider = -3; *global(text_property::shadow_y).slider = 4;
        *global(text_property::soft_shadow).toggle = true;
        *global(text_property::outline).toggle = false; *global(text_property::outline_color).color = 0xA0102030;
        *global(text_property::glow).toggle = true; *global(text_property::glow_color).color = 0x40506070;
        *global(text_property::sheen).toggle = true; *global(text_property::sheen_color).color = 0x708090A0;
        *global(text_property::sheen_width).slider = 28; *global(text_property::sheen_speed).slider = 0.5f;
        for (size_t i = 2; i < ui::global_text_settings::option_count; ++i) runtime.general_.OnOptionChanged(sizeIndex + i);
        bool effects = true;
        for (auto* module : runtime.modules_.All()) module->VisitTextSettings([&](ui::text_style& style) {
            effects &= style.letter_spacing == 1.25f && !style.shadow.enabled && style.shadow.color == 0x80403020 &&
                style.shadow.offset_x == -3 && style.shadow.offset_y == 4 && style.shadow.soft && !style.stroke.enabled &&
                style.stroke.color == 0xA0102030 && style.glow.enabled && style.glow.color == 0x40506070 &&
                style.sheen.enabled && style.sheen.color == 0x708090A0 && style.sheen.width == 28 && style.sheen.speed == 0.5f;
        });
        if (!check(effects && name.style.scale == 0.8f && name.style.font == 2 && name.style.top_color == 0xAABBCCDD &&
            name.style.bottom_color == 0x88997766 && name.offset_x == 13 && name.offset_y == -7 &&
            runtime.general_.ConfigText().letter_spacing == 0 && !runtime.general_.ConfigText().sheen.enabled,
            "global spacing and every effect reach all HUD text without changing config text, module colors, size, font or positions")) return false;
        name.style.letter_spacing = 0.25f; name.style.shadow.offset_x = 6; name.style.sheen.enabled = false;
        *global(text_property::glow_color).color = 0x8090A0B0;
        runtime.general_.OnOptionChanged(sizeIndex + static_cast<size_t>(text_property::glow_color));
        if (!check(name.style.letter_spacing == 0.25f && name.style.shadow.offset_x == 6 && !name.style.sheen.enabled,
            "a later global property leaves unrelated per-module effect overrides intact")) return false;
        const auto saved = runtime.CaptureSettings();
        hud_runtime restored; restored.LoadSettings(saved);
        std::vector<std::pair<float, int>> before, after;
        std::vector<std::string> owners;
        for (auto* module : runtime.modules_.All()) module->VisitTextSettings([&](ui::text_style& style) { before.emplace_back(style.scale, style.font); owners.emplace_back(module->Id()); });
        for (auto* module : restored.modules_.All()) module->VisitTextSettings([&](ui::text_style& style) { after.emplace_back(style.scale, style.font); });
        for (size_t index = 0; index < std::min(before.size(), after.size()); ++index)
            if (before[index] != after[index]) std::fprintf(stderr, "%s text %zu: expected %.2f/font %d, got %.2f/font %d\n",
                owners[index].c_str(), index, before[index].first, before[index].second, after[index].first, after[index].second);
        if (!check(before == after && restored.general_.GlobalTextSize() == 1.4f && restored.general_.GlobalFont() == 3,
            "global defaults and later per-module overrides persist through complete profile reloads")) return false;
        const auto textStyles = [](hud_runtime& hud) {
            auto store = settings_store::FromIniText(""); size_t index{};
            for (auto* module : hud.modules_.All()) module->VisitTextSettings([&](ui::text_style& style) {
                SaveTextStyle(store, std::to_string(index++).c_str(), style);
            });
            return store.ToIniText();
        };
        if (!check(textStyles(runtime) == textStyles(restored),
            "global effects and individual overrides round trip for every module, including notifications")) return false;
        const auto fresh = LoadTextProfile(saved, "text.new.module", {});
        if (!check(fresh.style.scale == 1.4f && fresh.style.font == 3 && fresh.style.letter_spacing == 1.25f &&
            fresh.style.shadow.offset_x == -3 && fresh.style.shadow.offset_y == 4 && fresh.style.shadow.soft && !fresh.style.stroke.enabled &&
            fresh.style.glow.color == 0x8090A0B0 && fresh.style.sheen.enabled && fresh.style.sheen.width == 28 && fresh.style.sheen.speed == 0.5f,
            "new text profiles inherit each applied global property while saved overrides stay authoritative")) return false;
        const auto legacy = LoadTextProfile(settings_store::FromIniText("[global.text]\nfont=2\nscale=1.8\n"), "text.old.module", name);
        if (!check(legacy.style.font == 2 && legacy.style.scale == name.style.scale && legacy.style.shadow.offset_x == 6 &&
            !legacy.style.sheen.enabled, "older global settings preserve existing effects until their controls are used")) return false;
        ui::embedded_font embedded;
        if (!check(embedded.Load(), "Grammara loads directly from the embedded font resource")) return false;
        HDC dc = ::CreateCompatibleDC(nullptr);
        HFONT font = ::CreateFontA(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, "Grammara");
        if (!check(dc != nullptr && font != nullptr, "the supplied font creates a native font face"))
        { if (dc != nullptr) ::DeleteDC(dc); if (font != nullptr) ::DeleteObject(font); return false; }
        const auto previous = ::SelectObject(dc, font);
        char actual[LF_FACESIZE]{}; ::GetTextFaceA(dc, LF_FACESIZE, actual);
        ::SelectObject(dc, previous); ::DeleteObject(font); ::DeleteDC(dc);
        return check(::_stricmp(actual, "Grammara") == 0, "Grammara resolves to its own face rather than a Windows fallback");
    }

    auto config_input_test::FontChoices(void) -> bool
    {
        const auto names = ui::FontNames();
        const auto find = [&](const char* label) {
            for (int index = 0; index < ui::FontCount(); ++index) if (std::strcmp(names[index], label) == 0) return index;
            return -1;
        };
        bool legacy = true;
        for (const char* family : {"Arial", "Calibri", "Consolas", "Courier New", "Georgia", "Lucida Console", "Microsoft Sans Serif",
            "Segoe UI", "Tahoma", "Times New Roman", "Trebuchet MS", "Verdana"})
            legacy &= find(family) >= 0;
        const int consolas = find("Consolas"), arial = find("Arial");
        if (!check(legacy && consolas >= ui::builtin_font_count && find("Bahnschrift") < 0 && find("Segoe UI") == 1 &&
            ui::FontFamilyIndex("consolas") == consolas && ui::FontFamily(consolas) == "Consolas" && ui::FontFamily(2).empty() &&
            std::strcmp(names[0], "XIUI+ (Bahnschrift)") == 0 && arial < consolas,
            "installed Windows fonts, including every XIUI Legacy family, follow the XIUI+ faces once and in name order")) return false;

        auto store = settings_store::FromIniText("");
        ui::text_style style{}; style.font = consolas; style.font_explicit = true; style.weight_choice = ui::text_weight_regular; style.italic = true;
        SaveTextStyle(store, "fonts.installed", style);
        const auto loaded = LoadTextStyle(store, "fonts.installed", {});
        if (!check(loaded.font == consolas && loaded.font_explicit && loaded.weight_choice == ui::text_weight_regular && loaded.italic &&
            store.ReadInt("fonts.installed", "font", -1) == 0 && store.ReadString("fonts.installed", "font_family") == "Consolas",
            "installed fonts save by family with weight and italic, and older builds read the XIUI+ face")) return false;
        style.font = 2; SaveTextStyle(store, "fonts.installed", style);
        SaveTextStyle(store, "fonts.builtin", ui::text_style{});
        if (!check(LoadTextStyle(store, "fonts.installed", {}).font == 2 && store.ReadString("fonts.installed", "font_family", "kept").empty() &&
            store.ReadString("fonts.builtin", "font_family", "absent") == "absent",
            "returning to a built-in face clears its family, and sections that never used one gain no key")) return false;

        store.WriteString("fonts.shared", "font_family", "XIUI Missing Test Font");
        const auto missing = LoadTextStyle(store, "fonts.shared", {});
        SaveTextStyle(store, "fonts.shared", missing);
        ui::text_rasterizer rasterizer;
        if (!check(missing.font >= ui::builtin_font_count && ui::FontFamily(missing.font) == "XIUI Missing Test Font" &&
            ui::FontFamilyIndex("xiui missing test font") == missing.font && store.ReadString("fonts.shared", "font_family") == "XIUI Missing Test Font" &&
            !rasterizer.Metrics(missing.font, 15).available && rasterizer.Rasterize(missing.font, 15, 'W').has_value() &&
            ui::FontFamilyIndex("") == 0 && ui::FontFamilyIndex(std::string(40, 'x')) == 0,
            "a shared profile's missing font keeps its saved name and draws with the XIUI+ face")) return false;

        const auto coverage = [&](const ui::font_variant variant) {
            const auto glyph = rasterizer.Rasterize(consolas, 15, 'W', variant);
            return glyph ? glyph->masks[0] : std::vector<uint8_t>{};
        };
        const auto sum = [](const std::vector<uint8_t>& mask) { size_t total{}; for (const auto value : mask) total += value; return total; };
        const auto regular = coverage(ui::font_weight::regular), bold = coverage(ui::font_weight::bold), slanted = coverage({ui::font_weight::regular, true});
        if (!check(rasterizer.Metrics(consolas, 15).available && !regular.empty() && sum(bold) > sum(regular) + sum(regular) / 10 &&
            slanted.size() == regular.size() && slanted != regular,
            "installed fonts render their own bold and italic glyphs")) return false;

        // Tall-metric families such as Noto Sans would otherwise resize HUD rows and drop values onto their bars.
        bool boxes = true, unclipped = true, symbols{};
        int measured{};
        for (int index = ui::builtin_font_count; index < ui::FontCount(); ++index)
        {
            symbols = symbols || std::strcmp(names[index], "Wingdings") == 0 || std::strcmp(names[index], "Webdings") == 0;
            for (const int pixels : {11, 15, 24})
            {
                const auto reference = rasterizer.Metrics(0, pixels), metrics = rasterizer.Metrics(index, pixels);
                if (!metrics.available) continue;
                ++measured;
                boxes &= metrics.height == reference.height && metrics.ascent == reference.ascent;
                for (const uint32_t codepoint : {static_cast<uint32_t>('g'), 0xC5u})
                {
                    const auto glyph = rasterizer.Rasterize(index, pixels, codepoint);
                    if (!glyph) continue;
                    const auto& mask = glyph->masks[0];
                    bool clean = true;
                    for (int x = 0; x < glyph->width; ++x) clean &= mask[x] == 0 && mask[(glyph->height - 1) * glyph->width + x] == 0;
                    if (!clean) std::fprintf(stderr, "%s clips U+%04X at %dpx\n", names[index], codepoint, pixels);
                    unclipped &= clean;
                }
            }
        }
        if (!check(measured > 100 && boxes && unclipped && !symbols,
            "installed text fonts share the XIUI+ line box and baseline without clipping tall or descending glyphs; symbol fonts are not listed")) return false;

        hud_runtime runtime;
        const auto start = runtime.general_.GlobalTextStart();
        const auto global = [&](const text_property property) { return runtime.general_.Option(start + static_cast<size_t>(property)); };
        const auto weight = global(text_property::weight), italic = global(text_property::italic), font = global(text_property::font);
        if (!check(weight.kind == module_option_kind::choice && weight.choice_count == 3 && std::strcmp(weight.choices[2], "Bold") == 0 &&
            italic.kind == module_option_kind::toggle && font.choice_count == static_cast<size_t>(ui::FontCount()),
            "Global Text offers every font, a weight choice and italic")) return false;
        *weight.choice = ui::text_weight_bold; runtime.general_.OnOptionChanged(start + static_cast<size_t>(text_property::weight));
        *italic.toggle = true; runtime.general_.OnOptionChanged(start + static_cast<size_t>(text_property::italic));
        *font.choice = consolas; runtime.general_.OnOptionChanged(start + static_cast<size_t>(text_property::font));
        auto& name = runtime.player_.Settings().name_text;
        const auto nameWeight = TextProfileOption(16, name), nameItalic = TextProfileOption(17, name);
        if (!check(nameWeight.choice == &name.style.weight_choice && nameItalic.toggle == &name.style.italic && text_profile_option_count == 18,
            "every module text element exposes its own weight and italic")) return false;
        *nameWeight.choice = ui::text_weight_regular;
        const auto variants = [](hud_runtime& hud, const int font, const int chosen, const auto& name) {
            bool all = true; size_t count{};
            for (auto* module : hud.modules_.All()) module->VisitTextSettings([&](ui::text_style& style) {
                ++count; all &= style.font == font && style.italic && (&style == &name.style ? style.weight_choice == ui::text_weight_regular : style.weight_choice == chosen);
            });
            return all && count > 70;
        };
        hud_runtime restored; restored.LoadSettings(runtime.CaptureSettings());
        if (!check(variants(runtime, consolas, ui::text_weight_bold, name) && variants(restored, consolas, ui::text_weight_bold, restored.player_.Settings().name_text),
            "global font, weight and italic reach all HUD text, keep a module override and survive profile reloads")) return false;

        ui::design_system design;
        auto chatStyle = ui::theme::CreateDefault().body_text; chatStyle.weight_choice = ui::text_weight_bold; chatStyle.italic = true;
        auto overridden = chatStyle; overridden.weight = ui::font_weight::medium;
        if (!check(chatStyle.Variant().weight == ui::font_weight::bold && chatStyle.Variant().italic && overridden.Variant().weight == ui::font_weight::medium &&
            ChatRunWeight(design, false, chatStyle) == ui::font_weight::bold && ChatRunWeight(design, true) == ui::font_weight::bold &&
            ChatRunWeight(design, false) == ui::font_weight::regular,
            "chat prose follows a saved weight while emphasis stays bold and code overrides keep precedence")) return false;

        ui::choice_dropdown dropdown; *font.choice = 0; dropdown.Open(font, {0, 0, 120, 24});
        const auto label = [&] { return std::string{font.ChoiceText(static_cast<size_t>(dropdown.Highlighted()))}; };
        const bool jumped = dropdown.Jump('c') && (label().front() == 'C' || label().front() == 'c');
        const auto first = dropdown.Highlighted();
        const bool next = dropdown.Jump('C') && dropdown.Highlighted() > first && std::tolower(static_cast<unsigned char>(label().front())) == 'c';
        const auto before = dropdown.Highlighted(); dropdown.Page(1);
        return check(jumped && next && dropdown.Highlighted() == std::min(before + dropdown.VisibleCount(), ui::FontCount() - 1) && !dropdown.Jump('~'),
            "long dropdowns jump by first letter and page by their visible rows");
    }
}
