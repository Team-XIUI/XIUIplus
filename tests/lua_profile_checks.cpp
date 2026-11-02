#include "config_input_fixture.hpp"
#include "config/lua_import_layout.hpp"

namespace xiui
{
    auto config_input_test::LuaProfileImport(void) -> bool
    {
        using namespace config_checks;
        try
        {
            const auto read = [](const std::filesystem::path& path) {
                std::ifstream input{path, std::ios::binary};
                if (!input) throw std::runtime_error{"Could not read fixture " + path.string()};
                return std::string{std::istreambuf_iterator<char>{input}, {}};
            };
            const auto write = [](const std::filesystem::path& path, const std::string_view text) {
                std::filesystem::create_directories(path.parent_path());
                std::ofstream file{path, std::ios::binary}; file.write(text.data(), static_cast<std::streamsize>(text.size()));
                if (!file) throw std::runtime_error{"Could not write isolated fixture."};
            };
            auto runtime = std::make_unique<hud_runtime>();
            const auto theme = ui::theme::CreateDefault();
            for (auto* module : runtime->modules_.All()) { module->ApplyTextDefaults(theme); module->ResetDefaults(theme); }
            runtime->default_settings_text_ = read("config/XIUIplus-defaults.ini");
            runtime->LoadSettings(settings_store::FromIniText(runtime->default_settings_text_));
            const auto defaults = runtime->CaptureSettings();
            const auto finishTransfer = [&] {
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{15};
                while (runtime->profile_transfer_.Busy() && std::chrono::steady_clock::now() < deadline)
                {
                    runtime->UpdateProfileTransfer();
                    if (runtime->profile_transfer_.Busy()) std::this_thread::sleep_for(std::chrono::milliseconds{1});
                }
                return !runtime->profile_transfer_.Busy();
            };
            const auto importProfiles = [&] { return runtime->CreateLuaProfiles() && finishTransfer() && runtime->profile_lua_done_; };
            std::filesystem::create_directories("out/lua-import-check");
            for (const auto version : {"1.8.4", "1.8.5"}) for (const auto name : {"defaults", "populated"})
            {
                const auto document = lua_data::reader{read(std::filesystem::path{"tests/fixtures/lua"} / version / (std::string{name} + ".lua"))}.Parse();
                const auto converted = ConvertLuaProfile(document, defaults, name, version);
                write(std::filesystem::path{"out/lua-import-check"} / (std::string{version} + "-" + name + ".txt"), converted.Report());
                if (!check(converted.Valid() && converted.converted > 100 && converted.unresolved > 0,
                    "real release writer fixtures convert supported fields and explicitly report omissions")) return false;
                if (!check(converted.preferences.ReadString("chat.bubbles", "enabled") == defaults.ReadString("chat.bubbles", "enabled"),
                    "Lua conversion retains Native-only feature defaults")) return false;
            }
            const auto mapped = ConvertLuaProfile(lua_data::reader{R"(return {userSettings={
                castBarFastCast={[1]=0.24},castBarFastCastRDMSJ=0.15,
                hotbarBar1={enabled=true,columns=6,slots=12},hotbarGlobal={slotYPadding=9,labelFontSize=18},
                petBarAvatar={hpScaleX=1.5,hpFontSize=18},windowPositions={Hotbar1={x=333,y=444}}
            }})"}.Parse(), defaults, "Mapped", "1.8.5");
            if (!check(mapped.Valid() && mapped.preferences.ReadInt("actionbars.hotbar", "columns", 0) == 6 &&
                mapped.preferences.ReadInt("layout.hotbar", "x", 0) == 333 && mapped.preferences.ReadInt("actionbars.hotbar", "vertical_spacing", 0) == 9 &&
                std::abs(mapped.preferences.ReadFloat("text.actionbars.bar0.1", "scale", 0) - 1.2f) < 0.001f &&
                mapped.preferences.ReadInt("pet.avatar", "HP_width", 0) == 225 && mapped.preferences.ReadBool("pet.avatar", "split_values", false) &&
                std::abs(mapped.preferences.ReadFloat("text.pet.avatar.hp", "scale", 0) - 1.2f) < 0.001f &&
                mapped.preferences.ReadInt("castbar.fastcast", "WAR", 0) == 24 && mapped.preferences.ReadInt("castbar.fastcast", "rdm_sub", 0) == 15,
                "current Native bar IDs, per-bar/per-resource styles and fractional Fast Cast units map correctly")) return false;
            for (const auto input : {"require(", "require( ) return {}", "return T 'not a table'", "return {x=1e999}"})
            {
                bool rejected{};
                try { lua_data::reader{input}.Parse(); } catch (...) { rejected = true; }
                if (!check(rejected, "truncated headers, invalid wrappers and non-finite literals are rejected")) return false;
            }
            if (!check(lua_data::reader{"require( 'common' ); return T{ showPlayerBar=true }"}.Parse().Find("showPlayerBar") != nullptr,
                "serializer headers tolerate spaces around the quoted module")) return false;
            const auto invalid = ConvertLuaProfile(lua_data::reader{"return {userSettings={showPlayerBar={},globalScale=1e100}}"}.Parse(), defaults, "Invalid");
            const auto unrelated = ConvertLuaProfile(lua_data::reader{"return {notXIUI=true}"}.Parse(), defaults, "Wrong file");
            const auto typed = ConvertLuaProfile(lua_data::reader{"return {userSettings={showPlayerBar='true',globalScale='1.5'}}"}.Parse(), defaults, "Legacy scalars");
            const auto badColor = ConvertLuaProfile(lua_data::reader{"return {userSettings={colorCustomization={playerBar={hpTextColor=-1}}}}"}.Parse(), defaults, "Bad color");
            if (!check(!invalid.Valid() && !unrelated.Valid() && !badColor.Valid() && typed.Valid() &&
                typed.preferences.ReadBool("player", "enabled", false) && typed.preferences.ReadInt("layout.player", "width", 0) == 750,
                "types, ranges and file identity are checked while lossless legacy scalar conversions remain supported")) return false;
            std::string flat{"return {"};
            for (size_t i = 0; i < 20000; ++i) flat += "[" + std::to_string(i + 1) + "]=" + std::to_string(i) + ",";
            flat += "[20000]=42}";
            lua_data::reader flatReader{flat}; const auto flatTable = flatReader.Parse();
            if (!check(flatTable.fields.size() == 20000 && flatTable.Index(20000)->Number() == 42 && flatReader.duplicates == 1,
                "large literal tables retain numeric identities and last-value duplicate semantics")) return false;
            const auto root = std::filesystem::weakly_canonical(std::filesystem::temp_directory_path()) /
                ("xiui-lua-import-" + std::to_string(::GetCurrentProcessId()) + "-" + std::to_string(::GetTickCount64()));
            std::filesystem::create_directory(root);
            struct cleanup
            {
                std::filesystem::path path;
                ~cleanup() { std::error_code ignored; if (this->path.parent_path() == std::filesystem::weakly_canonical(std::filesystem::temp_directory_path()) && this->path.filename().string().starts_with("xiui-lua-import-")) std::filesystem::remove_all(this->path, ignored); }
            } cleanupFiles{root};
            runtime->settings_path_ = (root / "XIUIplus.ini").string();
            runtime->profiles_.Initialize(runtime->settings_path_);
            runtime->editing_ = true; runtime->config_.bounds = {20, 20, 1100, 600};
            runtime->player_.State().bounds.x = 777;
            runtime->SaveSettings(); if (!runtime->DrainSettings()) return false;
            const auto current = read(runtime->settings_path_);
            const auto source = root / "addons/xiui";
            write(source / "profiles/profilelist.lua", "return {version='1.8.5',order={'Default','Second'}}");
            write(source / "profiles/Default.lua", "local settings={}; settings['userSettings']={showPlayerBar=false,readyCheckSoundFile='custom.wav',windowPositions={PlayerBar={x=123,y=234}}}; return settings;");
            write(source / "profiles/Second.lua", "return {userSettings={showTargetBar=false}}");
            write(source / "Shuu_6282/settings.lua", "require('common'); local settings=T{}; settings['currentProfile']='Second'; settings['satchelInventoryCache']=T{}; return settings;");
            write(source / "imgui.ini", "[Window][PlayerBar]\nPos=456,567\n");
            std::string wav(44 + 1600, '\0');
            const auto word = [&](const size_t at, const uint16_t value) { std::memcpy(wav.data() + at, &value, 2); };
            const auto dword = [&](const size_t at, const uint32_t value) { std::memcpy(wav.data() + at, &value, 4); };
            std::memcpy(wav.data(), "RIFF", 4); dword(4, static_cast<uint32_t>(wav.size() - 8)); std::memcpy(wav.data() + 8, "WAVEfmt ", 8);
            dword(16, 16); word(20, 1); word(22, 1); dword(24, 8000); dword(28, 16000); word(32, 2); word(34, 16);
            std::memcpy(wav.data() + 36, "data", 4); dword(40, 1600);
            write(source / "sounds/custom.wav", wav);
            const auto discovered = DiscoverLuaSetup(source, defaults);
            if (!check(discovered.Valid() && discovered.profiles.size() == 2 && discovered.profiles[0].preview.name == "Default" && discovered.characters.size() == 1 &&
                discovered.profiles[0].source_id.starts_with(std::string{lua_import_revision} + ":"),
                "folder discovery preserves profile order and reads real character serializer headers without executing Lua")) return false;
            if (!check(discovered.profiles[0].preview.preferences.ReadInt("layout.player", "x", 0) == 123,
                "modern saved positions are converted")) return false;
            if (!check(discovered.profiles[1].preview.preferences.ReadInt("layout.player", "x", 0) == 456 && discovered.profiles[0].preview.resources.size() == 1,
                "legacy positions fill missing coordinates while supported WAV resources are captured for import")) return false;
            write(source / "profiles/Broken.lua", "return {userSettings={showPlayerBar=");
            const auto mixed = DiscoverLuaSetup(source, defaults);
            if (!check(!mixed.Valid() && mixed.error.empty() && mixed.profiles.size() == 3 && mixed.profiles[0].preview.Valid() &&
                !mixed.profiles.back().preview.Valid() && mixed.Report().find("2 ready; 1 blocked") != std::string::npos,
                "damaged files remain visible in the report without hiding valid profiles")) return false;
            std::filesystem::remove(source / "profiles/Broken.lua");
            bool rejected{};
            try { lua_data::reader{"require('os'); return {}"}.Parse(); } catch (...) { rejected = true; }
            if (!check(rejected, "only the inert common serializer header is accepted")) return false;
            std::vector<settings_profiles::import_entry> oldImport{{"Earlier conversion", discovered.profiles[0].source_id.substr(lua_import_revision.size() + 1), defaults.Preferences("")}};
            std::filesystem::create_directory(root / "previous");
            settings_profiles previousConverter; previousConverter.Initialize((root / "previous/XIUIplus.ini").string());
            std::vector<size_t> previousCreated; std::string previousReport, previousError;
            if (!check(previousConverter.Import(oldImport, {}, previousCreated, previousReport, previousError), "an earlier importer identity can be recorded")) return false;
            std::vector<settings_profiles::import_entry> updatedImport{{"Updated conversion", discovered.profiles[0].source_id, defaults.Preferences("")}};
            if (!check(previousConverter.Import(updatedImport, {}, previousCreated, previousReport, previousError) &&
                !previousConverter.Import(updatedImport, {}, previousCreated, previousReport, previousError),
                "improved conversion can create a fresh profile while repeated imports with the same revision remain deduplicated")) return false;
            runtime->OpenProfileDialog(hud_runtime::profile_dialog::import_profile);
            D3DVIEWPORT8 viewport{}; viewport.Width = 1280; viewport.Height = 720;
            const ui::profile_dialog_layout dialog{runtime->config_.bounds, viewport, true};
            runtime->mouse_x_ = static_cast<int>(dialog.ImportSource(1).x + 5); runtime->mouse_y_ = static_cast<int>(dialog.ImportSource(1).y + 5);
            runtime->ProcessProfileClick(viewport);
            if (!check(runtime->profile_import_lua_ && runtime->profile_transfer_.Busy() && !runtime->LuaImportReady() && finishTransfer() && runtime->LuaImportReady() && runtime->profile_lua_preview_->profiles.size() == 2,
                "Import profile exposes XIUI Lua and automatically previews detected profiles")) return false;
            runtime->config_.focus.modal_item = 7;
            runtime->HandleLuaImportController(controller_action::confirm);
            if (!check(runtime->profile_lua_selection_ == 0 && runtime->profile_name_.Editor().Text().find("Default") != std::string::npos,
                "controller navigation selects the same source profile and name as mouse navigation")) return false;
            runtime->CycleLuaProfile(-1);
            if (!check(!runtime->LuaImportControlAvailable(8) && !runtime->LuaImportControlAvailable(13),
                "multi-profile imports cannot accidentally activate or change controller options")) return false;
            if (!check(runtime->CreateLuaProfiles() && runtime->profile_transfer_.Busy() && runtime->profiles_.Entries().size() == 1,
                "batch preparation queues work without publishing profiles")) return false;
            runtime->HandleLuaImportController(controller_action::cancel);
            if (!check(finishTransfer() && runtime->profiles_.Entries().size() == 1,
                "cancelling staging leaves the profile catalog unchanged")) return false;
            runtime->OpenProfileDialog(hud_runtime::profile_dialog::import_profile); runtime->SetProfileImportSource(true);
            if (!check(finishTransfer() && runtime->LuaImportReady(), "cancelled scans and batches can be retried")) return false;
            runtime->DrawProfileOverlay(viewport);
            if (!check(importProfiles() && runtime->profiles_.Entries().size() == 3 && runtime->profiles_.Selected() == 0 && runtime->player_.State().bounds.x == 777,
                "batch import creates separate profiles without activating them or replacing current positions")) return false;
            const auto firstPath = runtime->profiles_.Entries()[1].path;
            const auto imported = settings_store::CachedFile(firstPath);
            if (!check(imported.ReadString("profile", "name") != "Default" && std::filesystem::exists(std::filesystem::path{firstPath}.parent_path() / "report.txt"),
                "Lua Default is renamed and the itemized migration report is saved with frozen source files")) return false;
            const auto customSound = imported.ReadString("readycheck", "sound_file");
            if (!check(!customSound.empty() && read(root / "sounds" / customSound) == wav,
                "compatible sound bytes are published before their imported profile becomes available")) return false;
            runtime->profile_lua_done_ = false;
            if (!check(!importProfiles() && runtime->profiles_.Entries().size() == 3,
                "retrying the same source does not duplicate an imported batch")) return false;
            settings_profiles reloaded; reloaded.Initialize(runtime->settings_path_);
            if (!check(reloaded.Entries().size() == 3 && reloaded.Selected() == 0,
                "atomically published import groups survive reload without changing the selected profile")) return false;
            const auto size = reloaded.Entries().size();
            std::vector<settings_profiles::import_entry> pending{{"Failure fixture", "other-source", defaults.Preferences("")}};
            std::vector<settings_profiles::import_file> invalidArchive{{"../escape.txt", "invalid"}};
            std::vector<size_t> created; std::string report, error;
            if (!check(!reloaded.Import(pending, invalidArchive, created, report, error) && reloaded.Entries().size() == size && !std::filesystem::exists(root / "escape.txt"),
                "a staging failure publishes no profiles or files outside the import directory")) return false;
            runtime->CloseProfileControls();
            write(source / "profiles/Activation.lua", "return {userSettings={hotbarCrossbar={enableDoubleTap=true,doubleTapWindow=0.47},windowPositions={PlayerBar={x=222,y=333}}}}");
            write(source / "Other_999/settings.lua", "return {currentProfile='Activation'}");
            auto links = settings_store{runtime->settings_path_};
            links.WriteString("profiles.characters", "local-6282", "default");
            if (!links.Flush()) return false;
            runtime->profile_character_ = "local-6282";
            runtime->OpenProfileDialog(hud_runtime::profile_dialog::import_profile);
            runtime->SetProfileImportSource(true);
            if (!check(finishTransfer(), "activation scan completes in the background")) return false;
            const auto& profiles = runtime->profile_lua_preview_->profiles;
            const auto activation = std::find_if(profiles.begin(), profiles.end(), [](const auto& profile) { return profile.preview.name == "Activation"; });
            if (activation == profiles.end()) return false;
            runtime->profile_lua_selection_ = static_cast<size_t>(activation - profiles.begin());
            runtime->profile_lua_activate_ = runtime->profile_lua_links_ = runtime->profile_lua_controls_ = true;
            if (!check(importProfiles() && runtime->profiles_.Selected() != 0 && runtime->player_.State().bounds.x == 222,
                "explicit single-profile activation applies imported placement")) return false;
            if (!runtime->DrainSettings()) return false;
            const auto activated = runtime->CaptureSettings();
            const auto saved = settings_store::CachedFile(runtime->settings_path_);
            if (!check(activated.ReadBool("controller.crossbar.device", "double_tap", false) &&
                activated.ReadInt("controller.crossbar.device", "double_tap_ms", 0) == 470 &&
                saved.ReadString("profiles.characters", "local-999") == runtime->profiles_.Entries()[runtime->profiles_.Selected()].id &&
                saved.ReadString("profiles.characters", "local-6282") == "default",
                "opt-in controller behavior converts seconds to milliseconds and links preserve existing associations")) return false;
            return true;
        }
        catch (const std::exception& error) { std::printf("Lua import check: %s\n", error.what()); return false; }
    }
}
