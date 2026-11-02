#include "hud_runtime.hpp"
#include "config/lua_import_layout.hpp"
#include "chat/chat_encoding.hpp"
#include "settings/storage_paths.hpp"

namespace xiui
{
    auto hud_runtime::SetProfileImportSource(const bool lua) -> void
    {
        this->profile_transfer_.Cancel();
        this->profile_import_lua_ = lua;
        this->profile_import_.reset(); this->profile_file_.clear(); this->profile_legacy_import_ = false;
        this->profile_error_.clear(); this->config_.focus.modal_item = 0;
        if (!lua) { this->profile_name_.Open(""); return; }
        this->profile_lua_done_ = false; this->profile_lua_activate_ = this->profile_lua_links_ = this->profile_lua_controls_ = false;
        this->profile_lua_preview_.reset(); this->profile_lua_report_text_.clear(); this->profile_lua_report_.Reset();
        this->profile_name_.Open("XIUI Lua");
        const auto source = AshitaConfigDirectory(this->settings_path_) / "addons" / "xiui";
        this->profile_lua_path_.Open(ToUtf8(source.wstring()), 2048);
        this->PreviewLuaProfiles();
    }

    auto hud_runtime::PrepareLuaProfiles(const std::filesystem::path& path, const std::string& defaultText, const std::stop_token stop) -> lua_setup_preview
    {
        if (stop.stop_requested()) return {};
        auto baseline = std::make_unique<hud_runtime>();
        const auto theme = ui::theme::CreateDefault();
        for (auto* module : baseline->modules_.All()) { module->ApplyTextDefaults(theme); module->ResetDefaults(theme); }
        baseline->LoadSettings(settings_store::FromIniText(defaultText));
        const auto defaults = baseline->CaptureSettings();
        const auto normalize = [&](lua_profile_preview& preview) {
            if (stop.stop_requested()) return;
            baseline->LoadSettings(preview.preferences);
            auto normalized = baseline->CaptureSettings().Preferences("");
            preview.preferences.VisitPreferences([&](const auto section, const auto key, const auto value) {
                const auto result = normalized.ReadString(std::string{section}.c_str(), std::string{key}.c_str());
                if (result == value) return;
                char* firstEnd{}; char* secondEnd{};
                const std::string sourceValue{value};
                const auto first = std::strtod(sourceValue.c_str(), &firstEnd);
                const auto second = std::strtod(result.c_str(), &secondEnd);
                if (firstEnd && secondEnd && *firstEnd == 0 && *secondEnd == 0 && std::abs(first - second) < 0.001) return;
                preview.issues.push_back({lua_import_severity::warning, std::string{section} + "." + std::string{key},
                    "Native normalization: " + (value.size() > 100 ? "[" + std::to_string(value.size()) + " bytes]" : std::string{value}) + " -> " +
                    (result.size() > 100 ? "[" + std::to_string(result.size()) + " bytes]" : result)});
            });
            preview.preferences = std::move(normalized);
        };
        return DiscoverLuaSetup(path, defaults, normalize, stop);
    }

    auto hud_runtime::PreviewLuaProfiles(const bool browse) -> bool
    {
        if (this->profile_transfer_.Busy()) { this->profile_lua_status_ = "Previous operation is stopping. Choose Scan again shortly."; return false; }
        this->profile_lua_done_ = false; this->profile_lua_selection_ = SIZE_MAX;
        this->profile_lua_preview_.reset(); this->profile_lua_report_text_.clear(); this->profile_lua_report_.Reset();
        profile_transfer_request request; request.kind = profile_transfer_kind::scan_lua; request.browse = browse;
        request.path = std::filesystem::path{FromUtf8(this->profile_lua_path_.Editor().Text())};
        request.scan_lua = [defaults = this->default_settings_text_](const auto& path, const std::stop_token stop) {
            return PrepareLuaProfiles(path, defaults, stop);
        };
        this->profile_lua_status_ = "Scanning and checking Lua profiles...";
        return this->StartProfileTransfer(std::move(request));
    }

    auto hud_runtime::BrowseLuaProfile() -> void { this->PreviewLuaProfiles(true); }

    auto hud_runtime::CycleLuaProfile(const int direction) -> void
    {
        if (!this->profile_lua_preview_ || this->profile_lua_preview_->profiles.empty()) return;
        const auto count = this->profile_lua_preview_->profiles.size() + 1;
        const auto current = this->profile_lua_selection_ == SIZE_MAX ? 0 : this->profile_lua_selection_ + 1;
        const auto next = (current + (direction < 0 ? count - 1 : 1)) % count;
        this->profile_lua_selection_ = next == 0 ? SIZE_MAX : next - 1;
        this->profile_name_.Open(next == 0 ? "XIUI Lua" : this->profiles_.UniqueName("XIUI Lua - " + this->profile_lua_preview_->profiles[next - 1].preview.name));
        this->profile_lua_report_text_ = this->profile_lua_preview_->Report(this->profile_lua_selection_);
        this->profile_lua_report_.Reset(); this->profile_lua_done_ = false;
    }

    auto hud_runtime::LuaImportReady() const -> bool
    {
        if (this->profile_transfer_.Busy() || this->profile_lua_done_ || !this->profile_lua_preview_ || !this->profile_lua_preview_->error.empty()) return false;
        if (this->profile_lua_selection_ == SIZE_MAX) return this->profile_lua_preview_->Valid();
        return this->profile_lua_selection_ < this->profile_lua_preview_->profiles.size() && this->profile_lua_preview_->profiles[this->profile_lua_selection_].preview.Valid();
    }

    auto hud_runtime::CreateLuaProfiles() -> bool
    {
        if (!this->LuaImportReady()) { this->profile_lua_status_ = "Scan a valid source and review its errors first."; return false; }
        this->SaveSettings();
        profile_transfer_request request; request.kind = profile_transfer_kind::stage_lua;
        request.stage_lua = [catalog = this->profiles_, snapshot = this->profile_lua_preview_, selection = this->profile_lua_selection_,
            prefix = this->profile_name_.Editor().Text(), reportText = this->profile_lua_report_text_,
            settings = &this->settings_writer_, profileWriter = &this->profile_writer_](std::stop_token stop, std::string& error) mutable -> std::unique_ptr<settings_profiles::staged_import> {
            if (!settings->Flush() || !profileWriter->Flush()) { error = "Could not save current settings; import cancelled."; return {}; }
            if (stop.stop_requested()) return {};
            std::vector<settings_profiles::import_entry> profiles;
            std::vector<lua_import_resource> resources;
            const auto& preview = *snapshot;
            for (size_t i = 0; i < preview.profiles.size(); ++i)
            {
                if (selection != SIZE_MAX && selection != i) continue;
                if (stop.stop_requested()) return {};
                const auto& profile = preview.profiles[i];
                const auto name = prefix.empty() ? "XIUI Lua - " + profile.preview.name : selection == SIZE_MAX ? prefix + " - " + profile.preview.name : prefix;
                profiles.push_back({name, profile.source_id, profile.preview.preferences.Preferences("")});
                resources.insert(resources.end(), profile.preview.resources.begin(), profile.preview.resources.end());
            }
            std::vector<settings_profiles::import_file> archive{{"report.txt", reportText}};
            std::string manifest;
            for (size_t i = 0; i < preview.sources.size(); ++i)
            {
                const auto& source = preview.sources[i];
                const auto filename = "source-" + std::to_string(i) + source.path.extension().string();
                archive.push_back({filename, source.bytes});
                manifest += filename + "  " + source.fingerprint + "  " + ToUtf8(source.path.wstring()) + "\n";
            }
            archive.push_back({"sources.txt", manifest});
            for (size_t i = 0; i < preview.profiles.size(); ++i)
                if (selection == SIZE_MAX || selection == i)
                    archive.push_back({"local-controller-" + std::to_string(i) + ".txt", preview.profiles[i].preview.local_input.ToIniText()});
            std::vector<settings_profiles::import_file> sounds;
            for (const auto& resource : resources)
            {
                const auto source = std::find_if(preview.sources.begin(), preview.sources.end(), [&](const auto& file) { return file.path == resource.source; });
                if (source == preview.sources.end()) { error = "A resource is missing from the captured source snapshot."; return {}; }
                if (std::none_of(sounds.begin(), sounds.end(), [&](const auto& file) { return file.name == resource.destination; }))
                    sounds.push_back({resource.destination, source->bytes});
            }
            return catalog.StageImport(profiles, archive, sounds, error, stop);
        };
        this->profile_lua_status_ = "Preparing profiles and source archive...";
        return this->StartProfileTransfer(std::move(request));
    }

    auto hud_runtime::FinishLuaImport(settings_profiles::staged_import& staged) -> bool
    {
        const auto& preview = *this->profile_lua_preview_;
        std::vector<size_t> created;
        std::string report;
        if (!this->profiles_.CommitImport(staged, created, report, this->profile_error_))
        { this->profile_lua_status_ = this->profile_error_; return false; }
        if (this->profile_lua_links_ && !this->profile_character_.empty())
        {
            const auto dash = this->profile_character_.rfind('-');
            auto links = settings_store{this->settings_path_};
            for (const auto& link : preview.characters)
            {
                const auto underscore = link.character.rfind('_');
                if (dash == std::string::npos || underscore == std::string::npos) continue;
                const auto id = link.character.substr(underscore + 1);
                if (id.empty() || id.find_first_not_of("0123456789") != std::string::npos) continue;
                const auto key = this->profile_character_.substr(0, dash + 1) + id;
                if (!links.ReadString("profiles.characters", key.c_str()).empty()) continue;
                const auto source = std::find_if(preview.profiles.begin(), preview.profiles.end(), [&](const auto& profile) { return profile.preview.name == link.profile; });
                if (source == preview.profiles.end()) continue;
                for (const auto index : created)
                    if (settings_store{this->profiles_.Entries()[index].path}.ReadString("profile", "lua_source_id") == source->source_id)
                        links.WriteString("profiles.characters", key.c_str(), this->profiles_.Entries()[index].id);
            }
            if (!links.Flush()) this->profile_error_ = "Profiles imported; optional character links could not be saved.";
        }
        this->profile_lua_done_ = true;
        this->profile_lua_status_ = std::to_string(created.size()) + " profiles imported. Your current setup is unchanged.";
        if (!this->profile_error_.empty()) this->profile_lua_status_ += " " + this->profile_error_;
        if (this->core_ && this->core_->GetChatManager()) this->core_->GetChatManager()->Writef(1, false, "[XIUI] Imported %zu Lua profiles. Report: %s", created.size(), report.c_str());
        if (this->profile_lua_activate_ && created.size() == 1)
        {
            const auto index = created.front();
            const auto preferences = this->profiles_.Read(index, this->profile_error_);
            if (preferences)
            {
                auto snapshot = this->CaptureSettings();
                OverlayProfilePreferences(snapshot, *preferences);
                if (this->profile_lua_controls_)
                {
                    const auto id = preferences->ReadString("profile", "lua_source_id");
                    const auto source = std::find_if(preview.profiles.begin(), preview.profiles.end(), [&](const auto& profile) { return profile.source_id == id; });
                    if (source != preview.profiles.end())
                        for (const auto* key : {"expanded", "double_tap", "double_tap_ms", "min_hold_ms", "trigger_press", "trigger_release"})
                        {
                            const auto value = source->preview.local_input.ReadString("controller.crossbar.device", key);
                            if (!value.empty()) snapshot.WriteString("controller.crossbar.device", key, value);
                        }
                }
                this->profiles_.Select(index); this->LoadSettings(snapshot); this->BindObservedProfile(); this->CloseProfileControls(); this->SaveSettings();
            }
        }
        return true;
    }

    auto hud_runtime::LuaImportControlAvailable(const size_t index) const -> bool
    {
        if (index == 0) return true;
        if (this->profile_transfer_.Busy() || index == 11) return false;
        if (index == 1) return this->LuaImportReady();
        if (index == 6 || index == 7) return this->profile_lua_preview_ && !this->profile_lua_preview_->profiles.empty();
        const bool single = this->profile_lua_preview_ && (this->profile_lua_selection_ != SIZE_MAX || this->profile_lua_preview_->profiles.size() == 1);
        if (index == 8) return single;
        if (index == 9) return this->profile_lua_preview_ && !this->profile_lua_preview_->characters.empty();
        if (index == 12) return !this->profile_lua_report_text_.empty();
        if (index == 13) return single && this->profile_lua_activate_;
        return index < 14;
    }

    auto hud_runtime::LuaImportTarget(const D3DVIEWPORT8& viewport) const -> uint64_t
    {
        const ui::lua_import_layout layout{this->config_.bounds, viewport};
        const auto x = static_cast<float>(this->mouse_x_), y = static_cast<float>(this->mouse_y_);
        if (Contains(layout.Close(), x, y)) return 100;
        for (size_t i = 0; i <= 13; ++i) if (this->LuaImportControlAvailable(i) && Contains(layout.Control(i), x, y)) return 100 + i;
        return 0;
    }
    auto hud_runtime::ActivateLuaImportControl(const size_t index) -> void
    {
        if (!this->LuaImportControlAvailable(index)) return;
        this->config_.focus.modal_item = static_cast<uint8_t>(index);
        if (index == 0) this->CloseProfileControls();
        else if (index == 1) this->CreateLuaProfiles();
        else if (index == 2) this->PreviewLuaProfiles();
        else if (index == 3) this->BrowseLuaProfile();
        else if (index == 4) this->profile_name_.SelectAll();
        else if (index == 5) this->profile_lua_path_.SelectAll();
        else if (index == 6 || index == 7) this->CycleLuaProfile(index == 6 ? -1 : 1);
        else if (index == 8) this->profile_lua_activate_ = !this->profile_lua_activate_;
        else if (index == 9) this->profile_lua_links_ = !this->profile_lua_links_;
        else if (index == 10) this->SetProfileImportSource(false);
        else if (index == 13) this->profile_lua_controls_ = !this->profile_lua_controls_;
    }
    auto hud_runtime::LuaImportClick(const D3DVIEWPORT8& viewport) -> bool
    {
        const auto target = this->LuaImportTarget(viewport);
        if (target == 100)
        {
            const ui::lua_import_layout layout{this->config_.bounds, viewport};
            this->BeginCloseInteraction(interaction::close_profile,
                Contains(layout.Close(), this->mouse_x_, this->mouse_y_) ? layout.Close() : layout.Control(0));
        }
        else if (target > 100) this->ActivateLuaImportControl(static_cast<size_t>(target - 100));
        return true;
    }

    auto hud_runtime::HandleLuaImportKey(const WPARAM key, const LPARAM flags) -> bool
    {
        if ((static_cast<uint32_t>(flags) & 0x80000000u) != 0) return true;
        const bool repeat = (static_cast<uint32_t>(flags) & 0x40000000u) != 0;
        auto& focus = this->config_.focus.modal_item;
        if (key == VK_ESCAPE && !repeat) this->CloseProfileControls();
        else if (this->profile_transfer_.Busy()) return true;
        else if (key == VK_TAB && !repeat)
        {
            const int step = (::GetKeyState(VK_SHIFT) & 0x8000) ? 13 : 1;
            do { focus = static_cast<uint8_t>((focus + step) % 14); } while (!this->LuaImportControlAvailable(focus));
        }
        else if (key == VK_PRIOR || key == VK_NEXT) this->profile_lua_report_.Page(key == VK_PRIOR ? -1 : 1);
        else if (key == VK_RETURN && !repeat)
        { if (focus == 5) this->PreviewLuaProfiles(); else if (focus == 4) focus = this->LuaImportReady() ? 1 : 2; else this->ActivateLuaImportControl(focus); }
        else if (focus == 4 || focus == 5)
        {
            const auto* properties = this->core_ ? this->core_->GetProperties() : nullptr;
            const auto previous = this->profile_lua_path_.Editor().Text();
            (focus == 5 ? this->profile_lua_path_ : this->profile_name_).HandleKey(key, flags, properties ? properties->GetFinalFantasyHwnd() : nullptr);
            if (focus == 5 && previous != this->profile_lua_path_.Editor().Text()) { this->profile_lua_preview_.reset(); this->profile_lua_done_ = false; this->profile_lua_status_ = "Source changed. Scan to update the preview."; }
        }
        else if (key == VK_UP || key == VK_DOWN) this->profile_lua_report_.Scroll(key == VK_UP ? -1 : 1);
        return true;
    }

    auto hud_runtime::HandleLuaImportController(const controller_action action) -> bool
    {
        auto& focus = this->config_.focus.modal_item;
        if (action == controller_action::cancel) this->CloseProfileControls();
        else if (this->profile_transfer_.Busy()) { focus = 0; if (action == controller_action::confirm) this->CloseProfileControls(); }
        else if (action == controller_action::confirm) this->ActivateLuaImportControl(focus);
        else if (focus == 12 && (action == controller_action::up || action == controller_action::down)) this->profile_lua_report_.Scroll(action == controller_action::up ? -1 : 1);
        else if (action == controller_action::left || action == controller_action::up || action == controller_action::right || action == controller_action::down)
        {
            const int step = action == controller_action::left || action == controller_action::up ? 13 : 1;
            do { focus = static_cast<uint8_t>((focus + step) % 14); } while (!this->LuaImportControlAvailable(focus));
        }
        return true;
    }

    auto hud_runtime::DrawLuaProfileImport(const D3DVIEWPORT8& viewport) -> void
    {
        const ui::lua_import_layout layout{this->config_.bounds, viewport};
        const auto& theme = this->design_system_.GetTheme();
        auto body = theme.body_text; body.scale = 0.8f;
        auto caption = theme.caption_text; caption.scale = 0.72f;
        this->design_system_.FillRect(this->config_.bounds, 0x66000000);
        this->config_controls_.Panel(layout.Bounds());
        this->config_controls_.Text("Import profile", layout.Title(), body);
        const auto x = static_cast<float>(this->mouse_x_), y = static_cast<float>(this->mouse_y_);
        const auto button = [&](const size_t index, const char* label, const bool active = false) {
            auto style = body; style.alignment = ui::text_alignment::center;
            const bool available = this->LuaImportControlAvailable(index);
            if (!available && !active) style.top_color = style.bottom_color = 0x80808080;
            this->config_controls_.Button(layout.Control(index), label, style, active || (available && Contains(layout.Control(index), x, y)));
        };
        button(10, "Profile file"); button(11, "XIUI Lua", true);
        this->config_controls_.Text("XIUI folder or saved profile", layout.PathLabel(), caption);
        this->config_controls_.TextInput(layout.Control(5), this->profile_lua_path_);
        button(3, "Browse..."); button(2, "Scan"); button(6, "<"); button(7, ">");
        const auto selection = !this->profile_lua_preview_ ? "No source loaded" : this->profile_lua_selection_ == SIZE_MAX ?
            "All profiles (" + std::to_string(this->profile_lua_preview_->profiles.size()) + ")" : this->profile_lua_preview_->profiles[this->profile_lua_selection_].preview.name;
        this->config_controls_.Text(selection.c_str(), layout.Selection(), body);
        this->config_controls_.Text(this->profile_lua_selection_ == SIZE_MAX ? "Name prefix for new profiles" : "New profile name", layout.NameLabel(), caption);
        this->config_controls_.TextInput(layout.Control(4), this->profile_name_);
        button(8, this->profile_lua_activate_ ? "[x] Activate single import" : "[ ] Activate single import");
        button(9, this->profile_lua_links_ ? "[x] Link unassigned chars" : "[ ] Link unassigned chars");
        button(13, this->profile_lua_controls_ ? "[x] Controller options" : "[ ] Controller options");
        this->profile_lua_report_.Draw(this->design_system_, this->config_controls_, layout.Control(12), this->profile_lua_report_text_, Contains(layout.Control(12), x, y), false,
            "Scan to preview conversions and unsupported settings.");
        const auto* status = this->profile_transfer_.Busy() && this->profile_transfer_.Selecting() ? "Choose a Lua profile in Windows, or Cancel here." : this->profile_lua_status_.c_str();
        this->config_controls_.Text(status, layout.Status(), caption);
        button(0, this->profile_lua_done_ ? "Close" : "Cancel"); button(1, "Import");
        this->config_controls_.IconButton(layout.Close(), ui::icon_glyph::close, Contains(layout.Close(), x, y));
    }
}
