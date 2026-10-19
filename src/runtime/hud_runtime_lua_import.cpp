#include "hud_runtime.hpp"
#include "config/lua_import_layout.hpp"
#include "chat/chat_encoding.hpp"
#include <commdlg.h>
#include <fstream>
#include "modules/readycheck/ready_check_wav.hpp"

#pragma comment(lib, "comdlg32.lib")

namespace xiui
{
    auto hud_runtime::SetProfileImportSource(const bool lua) -> void
    {
        this->profile_import_lua_ = lua;
        this->profile_import_.reset(); this->profile_share_key_.clear(); this->profile_key_view_.Reset();
        this->profile_error_.clear(); this->config_.focus.modal_item = 0;
        if (!lua) { this->profile_name_.Open(""); return; }
        this->profile_lua_done_ = false; this->profile_lua_activate_ = this->profile_lua_links_ = this->profile_lua_controls_ = false;
        this->profile_name_.Open("XIUI Lua");
        const auto source = std::filesystem::path{this->settings_path_}.parent_path() / "addons" / "xiui";
        this->profile_lua_path_.Open(ToUtf8(source.wstring()), 2048);
        this->PreviewLuaProfiles();
    }

    auto hud_runtime::PreviewLuaProfiles() -> bool
    {
        this->profile_error_.clear(); this->profile_lua_done_ = false; this->profile_lua_selection_ = SIZE_MAX;
        auto baseline = std::make_unique<hud_runtime>();
        const auto theme = ui::theme::CreateDefault();
        for (auto* module : baseline->modules_.All()) { module->ApplyTextDefaults(theme); module->ResetDefaults(theme); }
        baseline->LoadSettings(settings_store::FromIniText(this->default_settings_text_));
        const auto defaults = baseline->CaptureSettings();
        const auto normalize = [&](lua_profile_preview& preview) {
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
                    "Native normalization: " + std::string{value} + " -> " + result});
            });
            preview.preferences = std::move(normalized);
        };
        this->profile_lua_preview_ = DiscoverLuaSetup(std::filesystem::path{FromUtf8(this->profile_lua_path_.Editor().Text())}, defaults, normalize);
        this->profile_lua_report_text_ = this->profile_lua_preview_->Report();
        this->profile_lua_report_.Reset();
        this->profile_lua_status_ = this->profile_lua_preview_->error.empty() ? "Review the report, then import as new profiles." : this->profile_lua_preview_->error;
        return this->profile_lua_preview_->Valid();
    }

    auto hud_runtime::BrowseLuaProfile() -> void
    {
        std::array<wchar_t, 32768> filename{};
        OPENFILENAMEW dialog{}; dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = this->core_ != nullptr && this->core_->GetProperties() != nullptr ? this->core_->GetProperties()->GetFinalFantasyHwnd() : nullptr;
        dialog.lpstrFilter = L"XIUI saved profiles (*.lua)\0*.lua\0All files\0*.*\0";
        dialog.lpstrFile = filename.data(); dialog.nMaxFile = static_cast<DWORD>(filename.size());
        dialog.lpstrTitle = L"Select an XIUI Lua profile or profilelist.lua";
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_EXPLORER | OFN_NOCHANGEDIR;
        if (::GetOpenFileNameW(&dialog)) { this->profile_lua_path_.Open(ToUtf8(filename.data()), 2048); this->PreviewLuaProfiles(); }
    }

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
        if (this->profile_lua_done_ || !this->profile_lua_preview_ || !this->profile_lua_preview_->error.empty()) return false;
        if (this->profile_lua_selection_ == SIZE_MAX) return this->profile_lua_preview_->Valid();
        return this->profile_lua_selection_ < this->profile_lua_preview_->profiles.size() && this->profile_lua_preview_->profiles[this->profile_lua_selection_].preview.Valid();
    }

    auto hud_runtime::CreateLuaProfiles() -> bool
    {
        if (!this->LuaImportReady()) { this->profile_lua_status_ = "Scan a valid source and review its errors first."; return false; }
        this->SaveSettings();
        if (!this->DrainSettings()) { this->profile_lua_status_ = "Could not save current settings; import cancelled."; return false; }
        std::vector<settings_profiles::import_entry> profiles;
        std::vector<lua_import_resource> resources;
        const auto& preview = *this->profile_lua_preview_;
        const auto prefix = this->profile_name_.Editor().Text();
        for (size_t i = 0; i < preview.profiles.size(); ++i)
        {
            if (this->profile_lua_selection_ != SIZE_MAX && this->profile_lua_selection_ != i) continue;
            const auto& profile = preview.profiles[i];
            const auto name = prefix.empty() ? "XIUI Lua - " + profile.preview.name : this->profile_lua_selection_ == SIZE_MAX ? prefix + " - " + profile.preview.name : prefix;
            profiles.push_back({name, profile.source_id, profile.preview.preferences.Preferences("")});
            resources.insert(resources.end(), profile.preview.resources.begin(), profile.preview.resources.end());
        }
        std::vector<settings_profiles::import_file> archive{{"report.txt", this->profile_lua_report_text_}};
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
            if (this->profile_lua_selection_ == SIZE_MAX || this->profile_lua_selection_ == i)
                archive.push_back({"local-controller-" + std::to_string(i) + ".txt", preview.profiles[i].preview.local_input.ToIniText()});
        std::vector<size_t> created;
        std::string report;
        std::vector<std::filesystem::path> copied;
        const auto removeCopied = [&] { std::error_code ignored; for (const auto& file : copied) std::filesystem::remove(file, ignored); };
        try
        {
            const auto sounds = std::filesystem::weakly_canonical(std::filesystem::path{this->settings_path_}.parent_path() / "sounds");
            for (const auto& resource : resources)
            {
                if (!ReadyCheckWavFilename(resource.destination)) throw std::runtime_error{"Invalid custom sound filename."};
                const auto source = std::find_if(preview.sources.begin(), preview.sources.end(), [&](const auto& file) { return file.path == resource.source; });
                if (source == preview.sources.end()) throw std::runtime_error{"A resource is missing from the captured source snapshot."};
                std::filesystem::create_directories(sounds);
                const auto destination = sounds / resource.destination;
                if (std::filesystem::exists(destination))
                {
                    if (std::filesystem::file_size(destination) != source->bytes.size()) throw std::runtime_error{"A sound filename conflicts with an existing file."};
                    std::ifstream input{destination, std::ios::binary}; const std::string existing{std::istreambuf_iterator<char>{input}, {}};
                    if (existing != source->bytes) throw std::runtime_error{"A sound filename conflicts with an existing file."};
                    continue;
                }
                const auto file = ::CreateFileW(destination.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (file == INVALID_HANDLE_VALUE) throw std::runtime_error{"Could not copy a custom sound; import cancelled."};
                copied.push_back(destination);
                DWORD count{}; const bool success = ::WriteFile(file, source->bytes.data(), static_cast<DWORD>(source->bytes.size()), &count, nullptr) &&
                    count == source->bytes.size() && ::FlushFileBuffers(file);
                ::CloseHandle(file);
                if (!success) throw std::runtime_error{"Could not save a custom sound; import cancelled."};
            }
        }
        catch (const std::exception& failure) { removeCopied(); this->profile_lua_status_ = failure.what(); return false; }
        if (!this->profiles_.Import(profiles, archive, created, report, this->profile_error_))
        { removeCopied(); this->profile_lua_status_ = this->profile_error_; return false; }
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

    auto hud_runtime::LuaImportTarget(const D3DVIEWPORT8& viewport) const -> uint64_t
    {
        const ui::lua_import_layout layout{this->config_.bounds, viewport};
        const auto x = static_cast<float>(this->mouse_x_), y = static_cast<float>(this->mouse_y_);
        if (Contains(layout.Close(), x, y)) return 100;
        for (size_t i = 0; i <= 13; ++i) if (Contains(layout.Control(i), x, y)) return 100 + i;
        return 0;
    }
    auto hud_runtime::ActivateLuaImportControl(const size_t index) -> void
    {
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
    { const auto target = this->LuaImportTarget(viewport); if (target >= 100) this->ActivateLuaImportControl(static_cast<size_t>(target - 100)); return true; }

    auto hud_runtime::HandleLuaImportKey(const WPARAM key, const LPARAM flags) -> bool
    {
        if ((static_cast<uint32_t>(flags) & 0x80000000u) != 0) return true;
        const bool repeat = (static_cast<uint32_t>(flags) & 0x40000000u) != 0;
        auto& focus = this->config_.focus.modal_item;
        if (key == VK_ESCAPE && !repeat) this->CloseProfileControls();
        else if (key == VK_TAB && !repeat) focus = static_cast<uint8_t>((focus + ((::GetKeyState(VK_SHIFT) & 0x8000) ? 13 : 1)) % 14);
        else if (key == VK_PRIOR || key == VK_NEXT) this->profile_lua_report_.Page(key == VK_PRIOR ? -1 : 1);
        else if (key == VK_RETURN && !repeat)
        { if (focus == 5) this->PreviewLuaProfiles(); else if (focus == 4) focus = 1; else this->ActivateLuaImportControl(focus); }
        else if (focus == 4 || focus == 5)
        {
            const auto* properties = this->core_ ? this->core_->GetProperties() : nullptr;
            (focus == 5 ? this->profile_lua_path_ : this->profile_name_).HandleKey(key, flags, properties ? properties->GetFinalFantasyHwnd() : nullptr);
            if (focus == 5) { this->profile_lua_preview_.reset(); this->profile_lua_done_ = false; this->profile_lua_status_ = "Source changed. Scan to update the preview."; }
        }
        else if (key == VK_UP || key == VK_DOWN) this->profile_lua_report_.Scroll(key == VK_UP ? -1 : 1);
        return true;
    }

    auto hud_runtime::HandleLuaImportController(const controller_action action) -> bool
    {
        auto& focus = this->config_.focus.modal_item;
        if (action == controller_action::cancel) this->CloseProfileControls();
        else if (action == controller_action::confirm) this->ActivateLuaImportControl(focus);
        else if (focus == 12 && (action == controller_action::up || action == controller_action::down)) this->profile_lua_report_.Scroll(action == controller_action::up ? -1 : 1);
        else if (action == controller_action::left || action == controller_action::up) focus = static_cast<uint8_t>((focus + 13) % 14);
        else if (action == controller_action::right || action == controller_action::down) focus = static_cast<uint8_t>((focus + 1) % 14);
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
            if (index == 1 && !this->LuaImportReady()) style.top_color = style.bottom_color = 0x80808080;
            this->config_controls_.Button(layout.Control(index), label, style, active || Contains(layout.Control(index), x, y));
        };
        button(10, "Share key"); button(11, "XIUI Lua", true);
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
        this->config_controls_.Text(this->profile_lua_status_.c_str(), layout.Status(), caption);
        button(0, this->profile_lua_done_ ? "Close" : "Cancel"); button(1, "Import");
        this->config_controls_.IconButton(layout.Close(), ui::icon_glyph::close, Contains(layout.Close(), x, y));
    }
}
