#include "config_input_fixture.hpp"

namespace xiui
{
    using namespace config_checks;

    auto config_input_test::ProfileSharing(void) -> bool
    {
        std::string error;
        const auto presetResource = ::FindResourceW(nullptr, MAKEINTRESOURCEW(305), MAKEINTRESOURCEW(10));
        if (!check(presetResource != nullptr, "the portable sharing preset is embedded separately from release defaults")) return false;
        const auto* presetBytes = static_cast<const char*>(::LockResource(::LoadResource(nullptr, presetResource)));
        const std::string_view preset{presetBytes, ::SizeofResource(nullptr, presetResource)};
        if (!check(profile_share_detail::Checksum(preset) == 0xB0E857FE, "XIUI2 retains its immutable portable preset")) return false;
        auto compactSource = settings_store::FromIniText(preset);
        const auto compactKey = EncodeProfileKey("Default", compactSource, error);
        auto compactDecoded = DecodeProfileKey(compactKey, error);
        if (!check(compactKey.starts_with("XIUI2.") && compactKey.size() < 200 && compactDecoded &&
            compactDecoded->preferences.ToIniText() == compactSource.ToIniText(),
            "a default profile shares as a short key and restores every preference without relying on current defaults")) return false;
        compactSource.WriteFloat("text.player.name", "shadow_x", -4);
        compactSource.WriteRect("layout.exp", {337, 1402, 860, 145});
        compactSource.WriteString("future.module", "custom", "settings from a newer module");
        const auto customizedKey = EncodeProfileKey("Custom setup", compactSource, error);
        compactDecoded = DecodeProfileKey(customizedKey, error);
        if (!check(customizedKey.starts_with("XIUI2.") && customizedKey.size() < 500 && compactDecoded &&
            compactDecoded->name == "Custom setup" && compactDecoded->preferences.ToIniText() == compactSource.ToIniText(),
            "compact keys retain changed text styles, positions and fields added by future modules")) return false;
        auto damagedCompact = customizedKey; damagedCompact[6] = damagedCompact[6] == 'A' ? 'B' : 'A';
        const auto patch = profile_share_detail::EncodeDelta("Custom setup", compactSource);
        if (!check(!DecodeProfileKey(damagedCompact, error) && !DecodeProfileKey(customizedKey.substr(0, customizedKey.size() - 4), error) &&
            !profile_share_detail::DecodeDelta(patch.substr(0, patch.size() - 1)) && !profile_share_detail::DecodeDelta(std::string(5, '\xFF')),
            "compact imports reject damaged keys, truncated patches and oversized integer fields")) return false;
        std::printf("Compact default key: %zu characters; customized key: %zu characters.\n", compactKey.size(), customizedKey.size());
        auto source = settings_store::FromIniText("[player]\nenabled=0\n[layout.player]\nx=123\n[chat.sounds]\nparty=3\n[chat.color.party]\noverride=1\ncolor=FF5599BB\n[session.exp.42]\nreceived=123\n[session.gil]\nnet=456\n[profiles]\nactive=foreign\n");
        const auto key = EncodeProfileKey("Shared setup", source, error);
        const auto partial = profile_share_detail::DecodeDelta(profile_share_detail::EncodeDelta("Partial", source));
        if (!check(partial && partial->preferences.ToIniText() == source.Preferences("").ToIniText() && key.starts_with("XIUI1."),
            "removed dictionary fields round trip and sparse profiles retain the smaller legacy format")) return false;
        ui::share_key_view keyView;
        std::array<float, 128> advances{}; advances.fill(1);
        keyView.Reflow(key, 32, 2, advances);
        std::string visibleKey;
        for (const auto& line : keyView.Lines()) visibleKey += line;
        keyView.End();
        if (!check(visibleKey == key && keyView.Lines().size() > 2 && keyView.First() > 0,
            "the wrapped key view retains every character and can scroll to the end without ellipses")) return false;
        keyView.Home();
        if (!check(keyView.First() == 0, "key navigation returns to the beginning")) return false;
        auto decoded = DecodeProfileKey(key, error);
        if (!check(!key.empty() && key == EncodeProfileKey("Shared setup", source, error) && decoded && decoded->name == "Shared setup" &&
            !decoded->preferences.ReadBool("player", "enabled", true) && decoded->preferences.ReadInt("chat.sounds", "party", 0) == 3 &&
            decoded->preferences.ReadArgb("chat.color.party", "color", 0) == 0xFF5599BB,
            "portable keys round-trip layout, chat sounds and custom colors deterministically")) return false;
        if (!check(decoded->preferences.ReadInt("session.exp.42", "received", -1) == -1 &&
            decoded->preferences.ReadInt("session.gil", "net", -1) == -1 && decoded->preferences.ReadString("profiles", "active", "missing") == "missing",
            "share keys omit private session totals and active-profile metadata")) return false;
        if (!check(DecodeProfileKey(" \n" + key.substr(0, 30) + "\n" + key.substr(30) + "\t", error).has_value(),
            "share keys tolerate whitespace introduced while copying")) return false;
        auto damaged = key; damaged[6] = damaged[6] == '0' ? '1' : '0';
        if (!check(!DecodeProfileKey(damaged, error) && !DecodeProfileKey("XIUI2." + key.substr(6), error) &&
            !DecodeProfileKey(key + "!", error) && !DecodeProfileKey(std::string(410 * 1024, 'a'), error),
            "damaged, unsupported and oversized keys are rejected")) return false;
        auto bulky = settings_store::FromIniText(""); bulky.WriteString("large", "value", std::string(300 * 1024, 'a'));
        if (!check(EncodeProfileKey("Too large", bulky, error).empty(), "share payloads have a bounded decompressed size")) return false;

        const auto root = std::filesystem::temp_directory_path() / ("xiui-sharing-" + std::to_string(::GetCurrentProcessId()) + "-" + std::to_string(::GetTickCount64()));
        if (!check(std::filesystem::create_directory(root), "sharing checks use an isolated settings directory")) return false;
        struct cleanup
        {
            std::filesystem::path root;
            ~cleanup()
            {
                std::error_code ignored;
                const auto directory = this->root / "xiuinative-profiles";
                for (const auto& file : std::filesystem::directory_iterator{directory, ignored}) std::filesystem::remove(file.path(), ignored);
                std::filesystem::remove(directory, ignored); std::filesystem::remove(this->root / "xiuinative.ini", ignored);
                std::filesystem::remove(this->root, ignored);
            }
        } cleanupFiles{root};
        hud_runtime runtime; runtime.editing_ = true; runtime.settings_path_ = (root / "xiuinative.ini").string();
        runtime.config_.bounds = {30, 30, 1100, 600}; runtime.profiles_.Initialize(runtime.settings_path_);
        runtime.player_.State().bounds.x = 123; runtime.SaveSettings(); runtime.DrainSettings();
        const D3DVIEWPORT8 viewport{0, 0, 1600, 900, 0, 1};
        const auto click = [&](const ui::rect& bounds) {
            runtime.mouse_x_ = static_cast<int>(bounds.x + 3); runtime.mouse_y_ = static_cast<int>(bounds.y + 3);
            runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
            runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
        };
        click(runtime.ConfigLayout().ProfileActionsButton());
        if (!check(runtime.profile_actions_ && runtime.KeyboardFocused(), "profile actions menu owns keyboard input")) return false;
        click(runtime.ConfigLayout().ProfileActionRow(0));
        if (!check(runtime.profile_name_.Focused() && runtime.profile_name_.Editor().Text() == "Default Copy" && runtime.CreateProfile() &&
            runtime.profiles_.Current().name == "Default Copy" && runtime.profiles_.Entries().size() == 2,
            "Duplicate creates and selects an independently named copy of the selected profile")) return false;
        runtime.player_.State().bounds.x = 321; runtime.SaveSettings(); runtime.DrainSettings();
        if (!check(runtime.SelectProfile(0) && runtime.player_.State().bounds.x == 123,
            "editing a duplicated profile leaves its source unchanged")) return false;
        if (!check(runtime.OpenProfileDialog(hud_runtime::profile_dialog::share) && !runtime.profile_share_key_.empty() && runtime.KeyboardFocused(),
            "Share opens a copyable portable key for the complete current setup")) return false;
        const auto completeKey = runtime.profile_share_key_;
        const ui::profile_dialog_layout sharingDialog{runtime.config_.bounds, viewport, true};
        runtime.DrawProfileOverlay(viewport);
        visibleKey.clear();
        for (const auto& line : runtime.profile_key_view_.Lines()) visibleKey += line;
        if (!check(visibleKey == completeKey, "Share displays the complete key immediately")) return false;
        runtime.mouse_x_ = static_cast<int>(sharingDialog.Key().x + 3); runtime.mouse_y_ = static_cast<int>(sharingDialog.Key().y + 3);
        if (!check(runtime.ProfileSoundTarget(viewport) == (0xF000000000000000ULL | 14),
            "the visible share key is directly copyable")) return false;
        decoded = DecodeProfileKey(completeKey, error);
        if (!check(decoded && decoded->preferences.ReadRect("layout.player", {}).x == 123,
            "a full runtime snapshot compresses and imports correctly")) return false;
        runtime.HandleProfileKey(VK_ESCAPE, 0);
        runtime.OpenProfileDialog(hud_runtime::profile_dialog::import_profile);
        const ui::profile_dialog_layout dialog{runtime.config_.bounds, viewport, true};
        if (!check(dialog.Paste().y + dialog.Paste().height < dialog.Key().y,
            "Paste key sits above the import key field")) return false;
        runtime.mouse_x_ = static_cast<int>(dialog.Create().x + 3); runtime.mouse_y_ = static_cast<int>(dialog.Create().y + 3);
        if (!check(runtime.ProfileSoundTarget(viewport) == 0, "Import remains disabled until a valid key is loaded")) return false;
        if (!check(runtime.LoadProfileKey(" \n" + completeKey.substr(0, 30) + "\n" + completeKey.substr(30)) && runtime.profile_share_key_ == completeKey,
            "pasted wrapping is removed before displaying the complete import key")) return false;
        if (!check(runtime.LoadProfileKey(completeKey) && runtime.profile_name_.Editor().Text() == "Default 2" &&
            !runtime.LoadProfileKey("damaged") && !runtime.CreateProfile() && runtime.profiles_.Entries().size() == 2 && runtime.player_.State().bounds.x == 123,
            "imports suggest unique names and invalid keys cannot create or overwrite profiles")) return false;
        if (!check(runtime.LoadProfileKey(completeKey) && runtime.CreateProfile() && runtime.profiles_.Current().name == "Default 2" &&
            runtime.profiles_.Entries().size() == 3 && runtime.player_.State().bounds.x == 123 && runtime.SelectProfile(1) && runtime.player_.State().bounds.x == 321,
            "Import creates a separate profile while preserving existing originals and duplicates")) return false;
        runtime.FlushSettings(); runtime.editing_ = false;
        return true;
    }

    auto config_input_test::ProfileDeletion(void) -> bool
    {
        const auto root = std::filesystem::temp_directory_path() / ("xiui-profile-delete-" + std::to_string(::GetCurrentProcessId()) + "-" + std::to_string(::GetTickCount64()));
        if (!check(std::filesystem::create_directory(root), "profile deletion uses an isolated directory")) return false;
        struct cleanup
        {
            std::filesystem::path root;
            ~cleanup()
            {
                std::error_code error;
                const auto profiles = this->root / "xiuinative-profiles";
                for (const auto& file : std::filesystem::directory_iterator{profiles, error}) std::filesystem::remove(file.path(), error);
                std::filesystem::remove(profiles, error);
                std::filesystem::remove(this->root / "xiuinative.ini", error);
                std::filesystem::remove(this->root, error);
            }
        } cleanupFiles{root};
        hud_runtime runtime; runtime.editing_ = true; runtime.settings_path_ = (root / "xiuinative.ini").string();
        runtime.config_.bounds = {30, 30, 1100, 600}; runtime.profiles_.Initialize(runtime.settings_path_);
        runtime.player_.State().bounds.x = 1200;
        runtime.SaveSettings();
        if (!check(runtime.DrainSettings(), "Default is saved before testing deletion")) return false;
        const auto defaultPath = std::filesystem::path{runtime.profiles_.Current().path};
        std::string error;
        if (!check(!runtime.profiles_.Remove(0, error) && std::filesystem::exists(defaultPath), "Default is protected at the storage layer")) return false;
        runtime.OpenProfileDialog(hud_runtime::profile_dialog::create); runtime.profile_name_.Insert("Temporary");
        if (!check(runtime.CreateProfile() && runtime.DrainSettings(), "a disposable profile can be created")) return false;
        runtime.player_.State().bounds.x = 1450;
        runtime.SaveSettings(); runtime.DrainSettings();
        const auto removedPath = runtime.profiles_.Current().path;
        const D3DVIEWPORT8 viewport{0, 0, 1920, 1080, 0, 1};
        const auto click = [&](const ui::rect& bounds) {
            runtime.mouse_x_ = static_cast<int>(bounds.x + 3); runtime.mouse_y_ = static_cast<int>(bounds.y + 3);
            const bool down = runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
            return runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport) && down;
        };
        const auto open = [&] {
            return click(runtime.ConfigLayout().ProfileActionsButton()) && click(runtime.ConfigLayout().ProfileActionRow(ui::profile_delete_action));
        };
        if (!check(open() && runtime.profile_dialog_ == hud_runtime::profile_dialog::delete_profile && runtime.KeyboardFocused() &&
            runtime.profile_name_.Editor().Text() == "Temporary" && std::filesystem::exists(removedPath),
            "Delete profile opens a named confirmation and owns input without removing the file")) return false;
        runtime.HandleProfileKey('A', 0);
        if (!check(runtime.profile_name_.Editor().Text() == "Temporary", "the profile being deleted is read-only")) return false;
        runtime.HandleProfileKey(VK_RETURN, 0);
        if (!check(!runtime.profile_name_.Focused() && std::filesystem::exists(removedPath), "Enter defaults to Cancel")) return false;
        if (!check(open(), "the confirmation can be reopened")) return false;
        const ui::profile_dialog_layout dialog{runtime.config_.bounds, viewport};
        click(dialog.Cancel());
        if (!check(!runtime.profile_name_.Focused() && std::filesystem::exists(removedPath), "Cancel keeps the selected profile")) return false;
        if (!check(open(), "a new delete confirmation can be requested")) return false;
        runtime.HandleProfileKey(VK_ESCAPE, 0);
        if (!check(!runtime.profile_name_.Focused() && std::filesystem::exists(removedPath), "Escape cancels deletion")) return false;
        const auto locked = ::CreateFileA(removedPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (!check(locked != INVALID_HANDLE_VALUE, "the disposable profile can simulate a deletion failure")) return false;
        const bool removed = runtime.profiles_.Remove(runtime.profiles_.Selected(), error);
        ::CloseHandle(locked);
        if (!check(!removed && !error.empty() && runtime.profiles_.Current().name == "Temporary" && std::filesystem::exists(removedPath),
            "failed file removal preserves the catalog and active profile")) return false;
        if (!check(open(), "the profile remains available after a failed removal")) return false;
        const auto held = defaultPath.string() + ".held";
        std::filesystem::rename(defaultPath, held);
        const bool missingFallback = runtime.DeleteProfile();
        std::filesystem::rename(held, defaultPath);
        if (!check(!missingFallback && !runtime.profile_error_.empty() && runtime.profile_name_.Focused() && std::filesystem::exists(removedPath),
            "an unavailable Default leaves the profile and confirmation intact")) return false;
        click(dialog.Create());
        if (!check(runtime.DrainSettings() && !runtime.profile_name_.Focused() && runtime.profiles_.Selected() == 0 &&
            runtime.player_.State().bounds.x == 1200 && !std::filesystem::exists(removedPath),
            "confirming Delete removes the file and restores Default without overwriting its layout")) return false;
        runtime.SaveSettings(); runtime.DrainSettings();
        settings_profiles catalog; catalog.Initialize(runtime.settings_path_);
        if (!check(catalog.Entries().size() == 1 && catalog.Selected() == 0 && !std::filesystem::exists(removedPath),
            "autosave and restarting the profile catalog do not recreate deleted profiles")) return false;
        click(runtime.ConfigLayout().ProfileActionsButton());
        const auto disabled = runtime.ConfigLayout().ProfileActionRow(ui::profile_delete_action);
        runtime.mouse_x_ = static_cast<int>(disabled.x + 3); runtime.mouse_y_ = static_cast<int>(disabled.y + 3);
        if (!check(runtime.ProfileSoundTarget(viewport) == 0 && !runtime.OpenProfileDialog(hud_runtime::profile_dialog::delete_profile),
            "Default has no enabled delete action")) return false;
        const auto& theme = runtime.design_system_.GetTheme();
        for (const bool highlighted : {false, true})
        {
            const auto color = ui::button_renderer::FaceColor(theme, highlighted, ui::button_tone::danger);
            const auto text = ui::button_renderer::TextStyle(theme, runtime.config_controls_.Style(theme.body_text, true), highlighted, ui::button_tone::danger);
            if (!check(((color >> 16) & 255) > ((color >> 8) & 255) && ((color >> 16) & 255) > (color & 255) && text.shadow.enabled && text.stroke.enabled,
                "Delete uses a red native button tint in both states while retaining config text effects")) return false;
        }
        runtime.editing_ = false;
        return true;
    }

    auto config_input_test::Profiles(void) -> bool
    {
        const auto root = std::filesystem::temp_directory_path() / ("xiui-profiles-" + std::to_string(::GetCurrentProcessId()) + "-" + std::to_string(::GetTickCount64()));
        if (!check(std::filesystem::create_directory(root), "profile check owns an isolated directory")) return false;
        struct cleanup
        {
            std::filesystem::path root;
            ~cleanup()
            {
                std::error_code error;
                const auto profiles = this->root / "xiuinative-profiles";
                for (const auto& file : std::filesystem::directory_iterator{profiles, error}) std::filesystem::remove(file.path(), error);
                std::filesystem::remove(profiles, error);
                std::filesystem::remove(this->root / "xiuinative.ini", error);
                std::filesystem::remove(this->root, error);
            }
        } cleanupFiles{root};
        hud_runtime runtime;
        runtime.editing_ = true;
        runtime.settings_path_ = (root / "xiuinative.ini").string();
        runtime.config_.bounds = {30, 30, 1100, 600};
        runtime.profiles_.Initialize(runtime.settings_path_);
        runtime.player_.State().bounds = {1210, 300, 400, 110};
        runtime.player_.Settings().show_identity = true;
        runtime.SaveSettings();
        if (!check(runtime.DrainSettings() && runtime.profiles_.Current().name == "Default", "existing settings become the initial Default profile")) return false;
        std::string renameError;
        if (!check(!runtime.ProfileActionAvailable(ui::profile_rename_action) &&
            !runtime.OpenProfileDialog(hud_runtime::profile_dialog::rename) && !runtime.profiles_.Rename(0, "Other", renameError) &&
            runtime.profiles_.Current().name == "Default", "Default cannot be renamed through the menu, dialog or storage")) return false;
        const D3DVIEWPORT8 viewport{0, 0, 1920, 1080, 0, 1};
        const auto click = [&](const ui::rect& bounds) {
            runtime.mouse_x_ = static_cast<int>(bounds.x + bounds.width * 0.5f);
            runtime.mouse_y_ = static_cast<int>(bounds.y + bounds.height * 0.5f);
            runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
            runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
        };
        click(runtime.ConfigLayout().ProfileAdd());
        if (!check(runtime.profile_name_.Focused() && runtime.KeyboardFocused(), "plus opens the create dialog and captures keyboard input")) return false;
        const auto before = runtime.player_.State().bounds;
        click(before);
        if (!check(runtime.profile_name_.Focused() && runtime.interaction_ == hud_runtime::interaction::none && runtime.player_.State().bounds.x == before.x,
            "modal dialog prevents preview clicks and dragging underneath it")) return false;
        runtime.HandleProfileKey(VK_RETURN, 0);
        if (!check(runtime.profile_name_.Focused() && !runtime.profile_error_.empty() && runtime.profiles_.Entries().size() == 1, "blank names remain in the dialog with an error")) return false;
        runtime.profile_name_.Insert("  Combat  ");
        runtime.HandleProfileKey(VK_RETURN, 0);
        if (!check(!runtime.profile_name_.Focused() && runtime.profiles_.Current().name == "Combat" && runtime.profiles_.Entries().size() == 2,
            "Enter creates, trims and selects a copy of the current setup")) return false;
        runtime.player_.Settings().show_identity = false;
        runtime.player_.State().bounds.x = 1450;
        runtime.SaveSettings(); runtime.FlushSettings();
        if (!check(runtime.SelectProfile(0) && runtime.player_.Settings().show_identity && runtime.player_.State().bounds.x == 1210,
            "switching back restores Default's own preferences and layout")) return false;
        if (!check(runtime.SelectProfile(1) && !runtime.player_.Settings().show_identity && runtime.player_.State().bounds.x == 1450,
            "automatic profile saving retains edits when switching away and back")) return false;
        click(runtime.ConfigLayout().ProfileAdd());
        runtime.profile_name_.Insert("combat"); runtime.HandleProfileKey(VK_RETURN, 0);
        if (!check(runtime.profile_name_.Focused() && !runtime.profile_error_.empty() && runtime.profiles_.Entries().size() == 2,
            "case-insensitive duplicate names do not replace an existing profile")) return false;
        runtime.HandleProfileKey(VK_ESCAPE, 0);
        if (!check(!runtime.profile_name_.Focused() && !runtime.KeyboardFocused(), "Escape cancels and releases dialog keyboard ownership")) return false;
        click(runtime.ConfigLayout().ProfileSelector());
        if (!check(runtime.profile_dropdown_ && runtime.KeyboardFocused(), "the selector opens a real dropdown")) return false;
        click(runtime.ConfigLayout().ProfileRow(0, 2));
        if (!check(!runtime.profile_dropdown_ && runtime.profiles_.Selected() == 0, "clicking a dropdown item switches profiles")) return false;
        click(runtime.ConfigLayout().ProfileSelector());
        click({1750, 800, 10, 10});
        if (!check(!runtime.profile_dropdown_, "an outside click dismisses the dropdown without moving a preview")) return false;
        if (!check(runtime.SelectProfile(1), "named profile can be selected again")) return false;
        runtime.FlushSettings();
        settings_profiles restored; restored.Initialize(runtime.settings_path_);
        if (!check(restored.Current().name == "Combat" && restored.Entries().size() == 2, "profile catalog and active selection survive a fresh initialization")) return false;
        const auto original = runtime.profiles_.Current();
        if (!check(runtime.ObserveProfileCharacter("rename-check"), "renaming retains an existing character binding")) return false;
        click(runtime.ConfigLayout().ProfileActionsButton());
        click(runtime.ConfigLayout().ProfileActionRow(ui::profile_rename_action));
        if (!check(runtime.profile_dialog_ == hud_runtime::profile_dialog::rename && runtime.profile_name_.Editor().Text() == "Combat",
            "the custom profile menu opens Rename with its current name")) return false;
        runtime.profile_name_.SelectAll(); runtime.profile_name_.Insert("Discarded");
        runtime.HandleProfileKey(VK_ESCAPE, 0);
        if (!check(runtime.profiles_.Current().name == "Combat", "canceling Rename keeps the existing name")) return false;
        runtime.OpenProfileDialog(hud_runtime::profile_dialog::rename);
        for (const auto* invalid : {"  ", "default"})
        {
            runtime.profile_name_.SelectAll(); runtime.profile_name_.Insert(invalid);
            runtime.HandleProfileKey(VK_RETURN, 0);
            if (!check(runtime.profile_name_.Focused() && !runtime.profile_error_.empty() && runtime.profiles_.Current().name == "Combat",
                "blank and duplicate rename attempts keep the dialog and profile intact")) return false;
        }
        runtime.profile_name_.SelectAll(); runtime.profile_name_.Insert("  Raid UI  ");
        click(ui::profile_dialog_layout{runtime.config_.bounds, viewport}.Create());
        if (!check(!runtime.profile_name_.Focused() && runtime.profiles_.Current().name == "Raid UI",
            "the Rename button trims and commits the name")) return false;
        runtime.OpenProfileDialog(hud_runtime::profile_dialog::rename);
        runtime.profile_name_.SelectAll(); runtime.profile_name_.Insert("raid ui");
        runtime.HandleProfileKey(VK_RETURN, 0);
        if (!check(runtime.profiles_.Current().name == "raid ui", "Enter permits changing only the name's case")) return false;
        runtime.OpenProfileDialog(hud_runtime::profile_dialog::rename);
        runtime.profile_name_.SelectAll(); runtime.profile_name_.Insert("Combat UI");
        runtime.HandleControllerConfig(controller_action::right, false);
        runtime.HandleControllerConfig(controller_action::confirm, true);
        if (!check(runtime.profile_name_.Focused() && runtime.profiles_.Current().name == "raid ui", "held controller confirmation cannot rename")) return false;
        runtime.HandleControllerConfig(controller_action::confirm, false);
        if (!check(runtime.DrainSettings() && !runtime.profile_name_.Focused() && runtime.profiles_.Current().name == "Combat UI" &&
            runtime.profiles_.Current().id == original.id && runtime.profiles_.Current().path == original.path &&
            runtime.profiles_.Entries().size() == 2 && runtime.player_.State().bounds.x == 1450,
            "controller Rename preserves profile identity, count and layout")) return false;
        restored.Initialize(runtime.settings_path_);
        if (!check(restored.Current().name == "Combat UI" && restored.Current().id == original.id &&
            restored.BoundProfile("rename-check") == restored.Selected() &&
            settings_store{original.path}.ReadInt("layout.player", "x", 0) == 1450,
            "the renamed profile, preferences and character binding survive reloading")) return false;
        std::string error;
        auto snapshot = settings_store::FromIniText("[player]\nenabled=0\n[session.exp.42]\nreceived=123\n[session.gil]\nnet=456\n[profiles]\nactive=wrong\n");
        if (!check(restored.Create("Separate", snapshot, error), "profile snapshots can be created independently")) return false;
        const auto preferences = restored.Read(restored.Selected(), error);
        if (!check(preferences && !preferences->ReadBool("player", "enabled", true) && preferences->ReadInt("session.exp.42", "received", -1) == -1 &&
            preferences->ReadInt("session.gil", "net", -1) == -1 && preferences->ReadString("profiles", "active", "missing") == "missing",
            "profile files exclude EXP/gil sessions and active-profile metadata")) return false;
        auto merged = settings_store::FromIniText("[session.exp.42]\nreceived=999\n"); merged.OverlayPreferences(snapshot);
        if (!check(merged.ReadInt("session.exp.42", "received", 0) == 999 && !merged.ReadBool("player", "enabled", true), "applying profile preferences preserves live session data")) return false;
        const ui::config_layout compact{{0, 0, 936, 350}, 15, false, true};
        const auto lastTab = compact.ModuleTab(14), selector = compact.ProfileSelector();
        runtime.editing_ = false;
        return check(lastTab.y + lastTab.height + 8 <= selector.y && selector.x < ui::config_layout::sidebar_width,
            "profile controls fit the bottom-left sidebar at the minimum window height");
    }

    auto config_input_test::CharacterProfiles(void) -> bool
    {
        const auto root = std::filesystem::temp_directory_path() / ("xiui-character-profiles-" + std::to_string(::GetCurrentProcessId()) + "-" + std::to_string(::GetTickCount64()));
        if (!check(std::filesystem::create_directory(root), "character profile check owns an isolated directory")) return false;
        struct cleanup
        {
            std::filesystem::path root;
            ~cleanup()
            {
                std::error_code error;
                const auto profiles = this->root / "xiuinative-profiles";
                for (const auto& file : std::filesystem::directory_iterator{profiles, error}) std::filesystem::remove(file.path(), error);
                std::filesystem::remove(profiles, error);
                std::filesystem::remove(this->root / "xiuinative.ini", error);
                std::filesystem::remove(this->root, error);
            }
        } cleanupFiles{root};
        hud_runtime runtime;
        runtime.editing_ = true;
        runtime.settings_path_ = (root / "xiuinative.ini").string();
        runtime.profiles_.Initialize(runtime.settings_path_);
        runtime.player_.State().bounds.x = 100;
        runtime.SaveSettings(); runtime.FlushSettings();
        const auto create = [&](const char* name) {
            return runtime.OpenProfileDialog(hud_runtime::profile_dialog::create) &&
                (runtime.profile_name_.Insert(name), runtime.CreateProfile());
        };
        if (!check(runtime.ObserveProfileCharacter("world-1-11") && create("Character A"),
            "a first character adopts the compatibility profile and can select a new shared profile")) return false;
        const auto a = runtime.profiles_.Selected();
        const auto aId = runtime.profiles_.Current().id;
        runtime.player_.State().bounds.x = 137; runtime.SaveSettings();
        if (!check(runtime.ObserveProfileCharacter("world-1-22") && create("Character B"),
            "another character can select a different profile from the same library")) return false;
        const auto b = runtime.profiles_.Selected();
        const auto bId = runtime.profiles_.Current().id;
        runtime.player_.State().bounds.x = 240; runtime.SaveSettings();
        runtime.chat_.AdoptDraft({"/p unsent draft", 15});
        runtime.OpenProfileDialog(hud_runtime::profile_dialog::delete_profile);
        if (!check(runtime.ObserveProfileCharacter("world-1-11") && runtime.profiles_.Selected() == a &&
            runtime.player_.State().bounds.x == 137 && !runtime.profile_name_.Focused(),
            "character changes drain pending saves, restore the bound layout and cancel destructive dialogs")) return false;
        runtime_state draft{}; runtime.CaptureChat(draft);
        if (!check(std::strcmp(draft.chat_draft, "/p unsent draft") == 0 && !draft.chat_focused,
            "automatic profile changes retain unsent chat without submitting or capturing typing")) return false;
        runtime.FlushSettings();
        std::string error;
        const auto savedB = runtime.profiles_.Read(b, error);
        if (!check(savedB && savedB->ReadRect("layout.player", {}).x == 240,
            "the previous character's pending layout is saved into its own profile")) return false;

        runtime.ObserveProfileCharacter("");
        runtime.player_.State().bounds.x = 999; runtime.SaveSettings(); runtime.FlushSettings();
        const auto savedA = runtime.profiles_.Read(a, error);
        if (!check(savedA && savedA->ReadRect("layout.player", {}).x == 137 && !runtime.profile_character_ready_,
            "unknown login identity cannot overwrite the last character's profile")) return false;
        if (!check(runtime.ObserveProfileCharacter("world-1-22") && runtime.player_.State().bounds.x == 240,
            "the second character restores its own selection after an unknown login interval")) return false;
        runtime.FlushSettings();

        if (!check(runtime.profiles_.Create("Broken", runtime.CaptureSettings(), error), "create a separate invalid-load fixture")) return false;
        const auto broken = runtime.profiles_.Current();
        auto file = settings_store{broken.path}; file.WriteInt("profile", "version", 2);
        if (!check(file.Flush(), "mark only the fixture profile unsupported")) return false;
        runtime.profiles_.Select(b);
        auto bindings = settings_store{runtime.settings_path_};
        bindings.WriteString("profiles.characters", "world-2-11", broken.id);
        bindings.WriteString("profiles.characters", "world-1-33", "profile-missing");
        if (!check(bindings.Flush(), "write local character bindings")) return false;
        if (!check(!runtime.ObserveProfileCharacter("world-2-11") && !runtime.profile_character_ready_ &&
            runtime.profiles_.Selected() == b && runtime.player_.State().bounds.x == 240 && !runtime.profile_error_.empty(),
            "an unsupported bound profile keeps committed settings and suspends profile autosaving")) return false;
        runtime.player_.State().bounds.x = 777; runtime.SaveSettings(); runtime.FlushSettings();
        const auto protectedB = runtime.profiles_.Read(b, error);
        if (!check(protectedB && protectedB->ReadRect("layout.player", {}).x == 240 && runtime.SelectProfile(b),
            "failed character loads cannot contaminate another profile and explicit selection recovers")) return false;
        runtime.FlushSettings();
        if (!check(runtime.profiles_.BoundProfile("world-1-11") == a && runtime.profiles_.BoundProfile("world-2-11") == b,
            "the same character ID on two worlds has independent profile bindings")) return false;
        if (!check(runtime.ObserveProfileCharacter("world-1-33") && runtime.profiles_.Selected() == 0 &&
            runtime.player_.State().bounds.x == 100, "missing profile IDs fall back to Default through the normal load path")) return false;
        runtime.FlushSettings();
        const auto local = settings_store{runtime.settings_path_};
        if (!check(local.ReadString("profiles.characters", "world-1-11") == aId &&
            local.ReadString("profiles.characters", "world-1-22") == bId &&
            local.ReadString("profiles.characters", "world-1-33") == "default",
            "fallback repairs only the current binding and retains other characters' selections")) return false;

        hud_runtime fresh;
        fresh.settings_path_ = runtime.settings_path_;
        fresh.LoadSettings(); fresh.profiles_.Initialize(fresh.settings_path_);
        if (!check(fresh.ObserveProfileCharacter("world-1-11") && fresh.profiles_.Current().id == aId &&
            fresh.player_.State().bounds.x == 137, "a fresh runtime restores the character binding independently of the last global active profile")) return false;
        fresh.FlushSettings();
        auto portable = settings_store::FromIniText("[PROFILES.Characters]\nworld-1-11=foreign\n[profiles.other]\nlocal=private\n[player]\nenabled=0\n");
        const auto exported = portable.Preferences("");
        auto merged = settings_store::FromIniText("[profiles.characters]\nworld-1-11=local\n");
        merged.OverlayPreferences(portable);
        const auto key = EncodeProfileKey("Portable", portable, error);
        const auto decoded = DecodeProfileKey(key, error);
        return check(exported.ReadString("profiles.characters", "world-1-11").empty() &&
            exported.ReadString("profiles.other", "local").empty() && merged.ReadString("profiles.characters", "world-1-11") == "local" &&
            decoded && decoded->preferences.ReadString("profiles.characters", "world-1-11").empty(),
            "profile files, overlays and share keys exclude local character bindings case-insensitively");
    }

    auto config_input_test::DefaultProfile(const char* source) -> bool
    {
        const auto root = std::filesystem::temp_directory_path() / ("xiui-defaults-" + std::to_string(::GetCurrentProcessId()));
        std::filesystem::create_directory(root);
        struct cleanup { std::filesystem::path path; ~cleanup() { std::error_code ignored; std::filesystem::remove_all(path, ignored); } } cleanupFiles{root};
        hud_runtime runtime;
        runtime.Initialize(nullptr, root.string().c_str());
        runtime.FlushSettings();
        std::ifstream file{source, std::ios::binary};
        const std::string expected{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
        if (!check(!expected.empty() && runtime.default_settings_text_ == expected, "runtime embeds the shipped defaults profile")) return false;
        const auto preferences = [](hud_runtime& hud) { return hud.CaptureSettings().Preferences("").ToIniText(); };
        const auto baseline = preferences(runtime);
        size_t styles{}; bool fonts = runtime.general_.GlobalFont() == 0;
        for (auto* module : runtime.modules_.All()) module->VisitTextSettings([&](ui::text_style& style) { ++styles; fonts &= style.font == 0; });
        if (!check(fonts && styles > 70, "fresh defaults use XIUI Bahnschrift for all HUD text")) return false;
        for (auto* module : runtime.modules_.All())
        {
            module->State().bounds.x += 117; module->State().bounds.y += 83;
            module->VisitTextSettings([](ui::text_style& style) { style.font = 3; style.letter_spacing = 2; });
        }
        runtime.party_.spacing_ += 3;
        runtime.player_.Settings().name_text.offset_x = 13;
        runtime.player_.Settings().name_text.style.top_color = 0xAABBCCDD;
        auto changed = runtime.CaptureSettings();
        changed.WriteFloat("status.buffs", "columns", 2);
        changed.WriteFloat("status.debuffs", "offset_y", 36);
        changed.WriteFloat("status.party.buffs", "row_spacing", 13);
        runtime.LoadSettings(changed);
        runtime.editing_ = true; runtime.config_.module = &runtime.party_;
        runtime.config_.section = party_module::status_section;
        runtime.config_.bounds = {40, 40, 1100, 600};
        const auto customized = preferences(runtime);
        const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
        const auto click = [&](const ui::rect& at) {
            runtime.mouse_x_ = static_cast<int>(at.x + 4); runtime.mouse_y_ = static_cast<int>(at.y + 4);
            const bool down = runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
            const bool up = runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
            return down && up;
        };
        if (!check(!runtime.ProfileActionAvailable(ui::profile_delete_action) && runtime.ProfileActionAvailable(ui::profile_reset_action),
            "Default disables Delete while keeping global Reset to defaults available")) return false;
        click(runtime.ConfigLayout().ProfileActionsButton());
        click(runtime.ConfigLayout().ProfileActionRow(ui::profile_reset_action));
        if (!check(runtime.profile_dialog_ == hud_runtime::profile_dialog::reset_defaults && runtime.profile_name_.Focused() &&
            runtime.profile_keyboard_selection_ == 0 && preferences(runtime) == customized,
            "three-dot Reset opens its Cancel-first warning from Party without changing settings")) return false;
        runtime.HandleProfileKey(VK_RETURN, 0);
        runtime.HandleKey(VK_RETURN, static_cast<LPARAM>(0x80000000u), false);
        if (!check(!runtime.profile_name_.Focused() && preferences(runtime) == customized, "default Enter cancels the entire reset")) return false;
        click(runtime.ConfigLayout().ProfileActionsButton());
        click(runtime.ConfigLayout().ProfileActionRow(ui::profile_reset_action));
        const ui::profile_dialog_layout dialog{runtime.config_.bounds, viewport};
        if (!check(click(dialog.Create()) && !runtime.ApplyConfirmedAction() && preferences(runtime) == baseline,
            "explicit pointer Reset restores all modules, text, positions and settings exactly once")) return false;
        runtime.FlushSettings(); runtime.ReloadSettings();
        if (!check(preferences(runtime) == baseline, "complete defaults persist after save and reload")) return false;
        runtime.party_.spacing_ += 6; runtime.buffs_.State().bounds.x += 200;
        runtime.config_.module = &runtime.enemy_list_; runtime.config_.section = 0;
        runtime.config_.focus.region = config_controller_region::profiles;
        runtime.config_.focus.id = "actions"; runtime.RefreshControllerConfigFocus();
        runtime.HandleControllerConfig(controller_action::confirm, false);
        for (size_t i = 0; i < 3; ++i) runtime.HandleControllerConfig(controller_action::down, false);
        if (!check(runtime.profile_keyboard_selection_ == ui::profile_reset_action, "controller navigation reaches Reset after disabled Delete")) return false;
        runtime.HandleControllerConfig(controller_action::confirm, false);
        runtime.HandleControllerConfig(controller_action::confirm, true);
        if (!check(runtime.profile_name_.Focused() && preferences(runtime) != baseline, "held Confirm cannot accept global reset")) return false;
        runtime.HandleControllerConfig(controller_action::right, false);
        runtime.HandleControllerConfig(controller_action::confirm, false);
        runtime.HandleControllerConfig(controller_action::confirm, true);
        if (!check(!runtime.profile_name_.Focused() && preferences(runtime) == baseline,
            "controller Reset restores everything from another category and ignores held reactivation")) return false;
        runtime.FlushSettings();
        if (!check(runtime.HandleCommand("save sync"), "explicit synchronous save uses the shared command path")) return false;
        runtime.ReloadSettings();
        return check(preferences(runtime) == baseline && runtime.party_.source_ == &runtime.party_source_,
            "completed saves retain the full baseline and native source connections");
    }

    auto config_input_test::PartyPresets(const char* classicPath, const char* xivPath) -> bool
    {
        hud_runtime runtime;
        runtime.editing_ = true; runtime.config_.module = &runtime.party_;
        runtime.config_.section = party_module::general_section;
        runtime.config_.bounds = {40, 40, 1100, 600};
        auto& party = runtime.party_;
        party.state_.bounds.x = 550; party.state_.bounds.y = 400;
        party.split_statuses_[0].enabled = false;
        party.text_[0].style.font = 2; party.text_[0].offset_x = 9;
        const auto alliance = runtime.alliance_first_.State().bounds, alliance2 = runtime.alliance_second_.State().bounds;
        const auto slots = runtime.GroupOptionSlots();
        const auto first = std::find(slots.begin(), slots.end(), party_module::preset_options_start);
        if (!check(first != slots.end() && std::count_if(slots.begin(), slots.end(), [&](size_t index) {
            return index < party.OptionCount() && party.Option(index).card;
        }) == 2, "Party General exposes exactly two preset cards below its controls")) return false;
        const auto cardSlot = static_cast<int>(first - slots.begin());
        for (size_t i = 0; i < 2; ++i)
            if (!check(party.Option(party_module::preset_options_start + i).kind == module_option_kind::action &&
                party.Option(party_module::preset_options_start + i).confirmation != nullptr &&
                party.Option(party_module::preset_options_start + i).card,
                "Classic and XIV presets use shared action buttons with a reset warning")) return false;
        for (const auto& section : party.ConfigSections())
            if (!check(std::strcmp(section.name, "Presets") != 0, "Party has no duplicate Presets page")) return false;
        const auto firstCard = runtime.ConfigLayout().OptionRow(cardSlot), secondCard = runtime.ConfigLayout().OptionRow(cardSlot + 1);
        if (!check(firstCard.width > 200 && firstCard.height > 90 && secondCard.y == firstCard.y && secondCard.x >= firstCard.x + firstCard.width,
            "Classic and XIV boxes share one row without overlapping")) return false;
        const auto button = runtime.ConfigLayout().OptionRow(cardSlot);
        const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
        runtime.mouse_x_ = static_cast<int>(button.x + 4); runtime.mouse_y_ = static_cast<int>(button.y + 4);
        const auto click = [&] {
            runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
            runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
        };
        click();
        if (!check(runtime.profile_name_.Focused() && runtime.profile_dialog_ == hud_runtime::profile_dialog::confirm_action &&
            runtime.profile_keyboard_selection_ == 0 && party.text_[0].style.font == 2 && !party.split_statuses_[0].enabled,
            "opening a preset warning selects Cancel and leaves custom settings untouched")) return false;
        runtime.HandleProfileKey(VK_RETURN, 0);
        if (!check(!runtime.profile_name_.Focused() && party.text_[0].style.font == 2,
            "keyboard confirmation defaults to Cancel")) return false;
        click(); runtime.HandleProfileKey(VK_ESCAPE, 0);
        if (!check(!runtime.profile_name_.Focused() && party.text_[0].offset_x == 9, "Escape cancels without resetting the preset")) return false;
        click();
        if (!check(runtime.ApplyConfirmedAction() && !runtime.ApplyConfirmedAction(), "an explicitly accepted preset executes only once")) return false;
        const auto classic = party.LayoutResources({0, 0, party.LayoutContainer().canvas_width, 0}, {true, true, true, true, false});
        if (!check(!party.show_sync_ && party.job_icons_ && party.icon_follow_name_ && party.tp_ && party.tp_display_ == 1 && party.meters_[0].appearance == ui::meter_style::window_theme &&
            classic.bars[1].width < classic.bars[0].width &&
            classic.bars[1].x + classic.bars[1].width == classic.bars[0].x + classic.bars[0].width &&
            classic.bars[2].x < classic.bars[0].x && classic.bars[2].width == 28 && party.LayoutContainer().width < 240 &&
            party.text_[0].style.font == 0 && party.text_[0].offset_x == 44 && party.split_statuses_[0].enabled &&
            party.meters_[0].height == 10 && party.text_[3].offset_y == -6 && party.text_[5].offset_y == -6 &&
            party.icon_offset_x_ == 9 && party.icon_offset_y_ == 1 && party.text_[party_module::tp_text].placement == 1 &&
            party.state_.bounds.x == 550 && party.state_.bounds.y == 400,
            "Classic restores Cody's captured bars, text, job-icon adjustments and TP placement while retaining position")) return false;
        if (classicPath != nullptr)
        {
            std::ifstream input{classicPath, std::ios::binary};
            const std::string captured{std::istreambuf_iterator<char>{input}, {}};
            if (!check(!captured.empty(), "the saved Classic preset is available")) return false;
            party_module expected; expected.Load(settings_store::FromIniText(captured));
            expected.state_.bounds.x = 550; expected.state_.bounds.y = 400;
            auto expectedSettings = settings_store::FromIniText(""); expected.Save(expectedSettings);
            auto actual = settings_store::FromIniText(""); party.Save(actual);
            if (!check(actual.ToIniText() == expectedSettings.ToIniText(),
                "embedded Classic restores every captured Party preference through normal persistence")) return false;
            party_module allianceClassic{1}; allianceClassic.state_.bounds.x = 210; allianceClassic.state_.bounds.y = 310;
            allianceClassic.ApplyPreset(0);
            if (!check(allianceClassic.group_ == 1 && allianceClassic.state_.bounds.x == 210 && allianceClassic.state_.bounds.y == 310,
                "Classic retains the selected alliance identity and position")) return false;
            allianceClassic.group_ = 0;
            allianceClassic.state_.bounds.x = 550; allianceClassic.state_.bounds.y = 400;
            auto allianceSettings = settings_store::FromIniText(""); allianceClassic.Save(allianceSettings);
            if (!check(allianceSettings.ToIniText() == expectedSettings.ToIniText(),
                "the same complete Classic preferences apply to an alliance list")) return false;
        }
        const auto originalIconSize = party.icon_size_;
        runtime.config_.focus.region = config_controller_region::options;
        const auto items = runtime.ControllerConfigItems(config_controller_region::options);
        const auto xiv = std::find_if(items.begin(), items.end(), [](const auto& item) { return item.index == party_module::preset_options_start + 1; });
        if (!check(xiv != items.end(), "controller focus exposes the XIV card below the ordinary controls")) return false;
        runtime.config_.bounds.height = 330;
        runtime.config_.focus.id = xiv->id;
        runtime.RefreshControllerConfigFocus();
        const auto revealed = runtime.ControllerConfigItems(config_controller_region::options);
        const auto shown = std::find_if(revealed.begin(), revealed.end(), [](const auto& item) { return item.index == party_module::preset_options_start + 1; });
        if (!check(shown != revealed.end() && shown->bounds.y + shown->bounds.height <= runtime.config_.bounds.y + runtime.config_.bounds.height - 24,
            "controller focus scrolls the full preset card into a short config window")) return false;
        runtime.HandleControllerConfig(controller_action::confirm, false);
        runtime.HandleControllerConfig(controller_action::confirm, true);
        if (!check(runtime.profile_name_.Focused() && party.icon_size_ == originalIconSize,
            "held controller Confirm cannot accept the warning")) return false;
        runtime.HandleControllerConfig(controller_action::confirm, false);
        if (!check(!runtime.profile_name_.Focused() && party.icon_size_ == originalIconSize,
            "controller confirmation defaults to Cancel")) return false;
        runtime.HandleControllerConfig(controller_action::confirm, false);
        runtime.HandleControllerConfig(controller_action::right, false);
        runtime.HandleControllerConfig(controller_action::confirm, false);
        runtime.HandleControllerConfig(controller_action::confirm, true);
        if (!check(!runtime.profile_name_.Focused() && party.state_.bounds.x == 550 && party.state_.bounds.y == 400 &&
            runtime.alliance_first_.icon_size_ == party.icon_size_ && runtime.alliance_second_.icon_size_ == party.icon_size_ &&
            runtime.alliance_first_.state_.bounds.x == alliance.x && runtime.alliance_first_.state_.bounds.y == alliance.y &&
            runtime.alliance_second_.state_.bounds.x == alliance2.x && runtime.alliance_second_.state_.bounds.y == alliance2.y,
            "explicit controller Apply restores captured XIV preferences while preserving every list position")) return false;
        if (xivPath != nullptr)
        {
            std::ifstream input{xivPath,std::ios::binary};
            const std::string captured{std::istreambuf_iterator<char>{input},{}};
            if (!check(!captured.empty(),"the captured XIV preset is available")) return false;
            party_module expected; expected.Load(settings_store::FromIniText(captured));
            expected.state_.bounds.x=550; expected.state_.bounds.y=400;
            auto expectedSettings=settings_store::FromIniText(""); expected.Save(expectedSettings);
            for (const auto* list : {&party,&runtime.alliance_first_,&runtime.alliance_second_})
            {
                auto normalized=*list;
                normalized.group_=0; normalized.state_.bounds.x=550; normalized.state_.bounds.y=400;
                auto actual=settings_store::FromIniText(""); normalized.Save(actual);
                if (!check(actual.ToIniText()==expectedSettings.ToIniText(),
                    "XIV restores every captured preference on Party and both alliances")) return false;
            }
        }
        runtime.LoadConfigSelection(settings_store::FromIniText("[session.config]\nmodule=party\nsection=12\n"));
        if (!check(runtime.config_.module == &party && runtime.config_.section == party_module::general_section,
            "old Party Presets navigation migrates to Party General")) return false;
        runtime.LoadConfigSelection(settings_store::FromIniText("[session.config]\nmodule=general\nsection=6\nparty_presets_general=1\n"));
        if (!check(runtime.config_.module == &party && runtime.config_.section == party_module::general_section,
            "the previous General preset page migrates to Party General")) return false;
        runtime.LoadConfigSelection(settings_store::FromIniText("[session.config]\nmodule=party\nsection=13\n"));
        if (!check(runtime.config_.module == &party && runtime.config_.section == party_module::window_section,
            "the old Window page keeps its navigation destination")) return false;
        party_module restored; restored.Load(runtime.CaptureSettings());
        return check(restored.layout_ == party.layout_ && restored.meters_[0].appearance == party.meters_[0].appearance &&
            restored.state_.bounds.x == 550 && restored.tp_ == party.tp_, "preset results persist through ordinary module settings");
    }
}
