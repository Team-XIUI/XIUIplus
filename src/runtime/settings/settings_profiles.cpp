#include "settings_profiles.hpp"
#include "layout_viewport.hpp"
#include "chat/chat_encoding.hpp"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>

namespace xiui
{
    auto OverlayProfilePreferences(const settings_store& target, const settings_store& source) -> void
    {
        target.OverlayPreferences(source);
        if (!LayoutSections(source).empty())
        {
            const auto viewport = ReadLayoutViewport(source);
            target.WriteInt("layout.viewport", "width", viewport.width);
            target.WriteInt("layout.viewport", "height", viewport.height);
        }
        for (const auto* section : {"chat", "chat2"})
            if (!source.ReadString(section, "channel").empty())
                target.WriteInt(section, "channel_version", source.ReadInt(section, "channel_version", 0));
        if (!source.ReadString("chat.bubbles", "opacity").empty() || !source.ReadString("chat.bubbles", "width").empty() ||
            !source.ReadString("chat.bubbles", "offset_y").empty())
            target.WriteInt("chat.bubbles", "appearance_version", source.ReadInt("chat.bubbles", "appearance_version", 0));
        if (!source.ReadString("meter.experience", "height").empty())
            target.WriteInt("exp", "bar_height_version", source.ReadInt("exp", "bar_height_version", 0));
        if (!source.ReadString("player", "hp_display").empty() || !source.ReadString("player", "mp_display").empty())
            target.WriteInt("player", "number_format_version", source.ReadInt("player", "number_format_version", 0));
        if (!source.ReadString("timing.player", "mini_gap").empty() || !source.ReadString("timing.player", "recovery_mini_gap").empty())
            target.WriteInt("timing.player", "mini_anchor_version", source.ReadInt("timing.player", "mini_anchor_version", 0));
        if (!source.ReadString("text.target.name", "offset_x").empty())
            target.WriteInt("target", "name_anchor_version", source.ReadInt("target", "name_anchor_version", 0));
        if (!source.ReadString("text.target.distance", "top_color").empty() || !source.ReadString("text.target.distance", "bottom_color").empty())
            target.WriteInt("target", "distance_color_version", source.ReadInt("target", "distance_color_version", 0));
        if (!source.ReadString("equipment", "background").empty() || !source.ReadString("equipment", "slot_frames").empty())
            target.WriteInt("equipment", "container_version", source.ReadInt("equipment", "container_version", 0));
        if (!source.ReadString("notifications.group2", "enabled").empty())
            target.WriteInt("notifications", "group2_enabled_version", source.ReadInt("notifications", "group2_enabled_version", 0));
        if (!source.ReadString("target.target_of_target", "placement").empty() || !source.ReadString("target.target_of_target", "arrow").empty())
            target.WriteInt("target.target_of_target", "theme_layout_version", source.ReadInt("target.target_of_target", "theme_layout_version", 0));
        if (!source.ReadString("target.target_of_target", "follow_text").empty())
            target.WriteInt("target.target_of_target", "shared_target_text_version", source.ReadInt("target.target_of_target", "shared_target_text_version", 0));
        if (!source.ReadString("target.mobinfo", "enabled").empty() || !source.ReadString("mobinfo", "enabled").empty() ||
            !source.ReadString("text.target.mobinfo", "offset_x").empty() || !source.ReadString("text.target.mobinfo", "offset_y").empty())
            target.WriteInt("target.mobinfo", "header_layout_version", source.ReadInt("target.mobinfo", "header_layout_version", 0));
        for (const auto* section : {"meter.player.hp", "meter.player.mp", "meter.player.tp"})
            if (!source.ReadString(section, "height").empty())
            {
                target.WriteInt("player", "bar_height_version", source.ReadInt("player", "bar_height_version", 0));
                break;
            }
        for (const auto* section : {"party", "alliance1", "alliance2"})
            if (!source.ReadString(section, "layout").empty())
                target.WriteInt(section, "classic_layout_version", source.ReadInt(section, "classic_layout_version", 0));
        const int frameVersion = source.ReadInt("actionbars", "slot_frame_version", 0);
        // A replaced legacy section must not inherit the snapshot's migration marker.
        source.VisitPreferences([&](const std::string_view section, const std::string_view key, const std::string_view) {
            if (key.size() != 5 || ::_strnicmp(key.data(), "frame", 5) != 0 || section.size() <= 17 ||
                ::_strnicmp(section.data(), "actionbars.", 11) != 0 || ::_strnicmp(section.data() + section.size() - 6, ".slots", 6) != 0) return;
            const std::string name{section};
            target.WriteInt(name.c_str(), "frame_version", source.ReadInt(name.c_str(), "frame_version", frameVersion));
        });
    }

    auto settings_profiles::UniqueName(std::string name) const -> std::string
    {
        if (name.empty()) name = "Imported profile";
        size_t length = std::min<size_t>(50, name.size());
        while (length < name.size() && length > 0 && (static_cast<unsigned char>(name[length]) & 0xC0) == 0x80) --length;
        name.resize(length);
        const auto exists = [&](const std::string& candidate) {
            return std::any_of(this->entries_.begin(), this->entries_.end(), [&](const auto& value) { return SameName(value.name, candidate); });
        };
        if (!exists(name)) return name;
        for (size_t i = 2; i <= 129; ++i) { auto candidate = name + " " + std::to_string(i); if (!exists(candidate)) return candidate; }
        return name;
    }

    auto settings_profiles::Duplicate(const size_t index, std::string name, std::string& error) -> bool
    {
        const auto source = this->Read(index, error);
        return source && this->Create(std::move(name), *source, error);
    }

    auto settings_profiles::SameName(const std::string& first, const std::string& second) -> bool
    {
        const auto a = FromUtf8(first), b = FromUtf8(second);
        return ::CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_EQUAL;
    }

    auto settings_profiles::Rename(const size_t index, std::string name, std::string& error) -> bool
    {
        if (index >= this->entries_.size()) { error = "This profile is unavailable."; return false; }
        if (index == 0) { error = "Default cannot be renamed."; return false; }
        if (!this->ValidateName(name, index, error) || !this->Read(index, error)) return false;
        auto& profile = this->entries_[index];
        settings_store store{profile.path};
        store.WriteString("profile", "name", name);
        if (!store.Flush()) { error = "Could not rename this profile. Check the file and try again."; return false; }
        profile.name = std::move(name);
        error.clear();
        return true;
    }

    auto settings_profiles::Remove(const size_t index, std::string& error) -> bool
    {
        if (index >= this->entries_.size()) { error = "This profile is unavailable."; return false; }
        if (index == 0) { error = "Default cannot be deleted."; return false; }
        if (!::DeleteFileA(this->entries_[index].path.c_str()) && ::GetLastError() != ERROR_FILE_NOT_FOUND)
        { error = "Could not delete this profile. Check the file and try again."; return false; }
        this->entries_.erase(this->entries_.begin() + index);
        if (this->selected_ == index) this->selected_ = 0;
        else if (this->selected_ > index) --this->selected_;
        error.clear();
        return true;
    }

    auto settings_profiles::Initialize(const std::string& settingsPath) -> void
    {
        this->settings_path_ = settingsPath;
        this->entries_.clear(); this->selected_ = 0;
        const auto directory = std::filesystem::path{settingsPath}.parent_path() / "xiuinative-profiles";
        this->directory_ = directory.string();
        std::error_code error;
        std::filesystem::create_directories(directory, error);
        this->entries_.push_back({"default", "Default", (directory / "default.ini").string()});
        for (const auto& file : std::filesystem::directory_iterator{directory, error})
        {
            if (file.is_directory(error) && file.path().filename().string().starts_with("lua-"))
            {
                for (const auto& imported : std::filesystem::directory_iterator{file.path(), error})
                {
                    if (this->entries_.size() >= 128) break;
                    if (!imported.is_regular_file(error) || imported.path().extension() != ".ini" || !imported.path().stem().string().starts_with("profile-")) continue;
                    settings_store store{imported.path().string()};
                    const auto name = store.ReadString("profile", "name"), id = imported.path().stem().string();
                    if (name.empty() || name.size() > 64 || std::any_of(this->entries_.begin(), this->entries_.end(), [&](const auto& item) { return SameName(item.name, name); })) continue;
                    this->entries_.push_back({id, name, imported.path().string()});
                }
                continue;
            }
            if (this->entries_.size() >= 128) break;
            if (!file.is_regular_file(error) || file.path().extension() != ".ini" || file.path().stem() == "default") continue;
            const auto id = file.path().stem().string();
            if (!id.starts_with("profile-")) continue;
            settings_store store{file.path().string()};
            const auto name = store.ReadString("profile", "name");
            if (name.empty() || name.size() > 64 || std::any_of(this->entries_.begin(), this->entries_.end(), [&](const auto& item) { return SameName(item.name, name); })) continue;
            this->entries_.push_back({id, name, file.path().string()});
        }
        std::sort(this->entries_.begin() + 1, this->entries_.end(), [](const auto& a, const auto& b) { return a.name < b.name; });
        const auto active = settings_store{settingsPath}.ReadString("profiles", "active", "default");
        for (size_t index = 0; index < this->entries_.size(); ++index) if (this->entries_[index].id == active) this->selected_ = index;
    }

    auto settings_profiles::Read(const size_t index, std::string& error) const -> std::optional<settings_store>
    {
        if (index >= this->entries_.size()) { error = "This profile is unavailable."; return std::nullopt; }
        const auto& profile = this->entries_[index];
        std::error_code status;
        const auto size = std::filesystem::file_size(profile.path, status);
        if (status || size > 4 * 1024 * 1024) { error = "Could not read the profile. Your current settings are unchanged."; return std::nullopt; }
        std::ifstream file{profile.path, std::ios::binary};
        const std::string text{std::istreambuf_iterator<char>{file}, {}};
        if (!file || text.size() != size || text.empty()) { error = "Could not read the profile. Your current settings are unchanged."; return std::nullopt; }
        auto store = settings_store::FromIniText(text);
        if (store.ReadInt("profile", "version", 0) != 1) { error = "This profile has an unsupported format."; return std::nullopt; }
        error.clear();
        return store;
    }

    auto settings_profiles::BoundProfile(const std::string_view character) const -> std::optional<size_t>
    {
        if (character.empty() || !this->Ready()) return {};
        const auto id = settings_store{this->settings_path_}.ReadString("profiles.characters", std::string{character}.c_str());
        if (id.empty()) return {};
        for (size_t index = 0; index < this->entries_.size(); ++index)
            if (this->entries_[index].id == id)
            {
                std::error_code error;
                return std::filesystem::is_regular_file(this->entries_[index].path, error) ? index : 0;
            }
        return 0;
    }

    auto settings_profiles::ValidateName(std::string& name, const size_t excludedIndex, std::string& error) const -> bool
    {
        const auto begin = name.find_first_not_of(" \t\r\n"), end = name.find_last_not_of(" \t\r\n");
        name = begin == std::string::npos ? "" : name.substr(begin, end - begin + 1);
        if (name.empty()) { error = "Enter a profile name."; return false; }
        if (name.size() > 64 || std::any_of(name.begin(), name.end(), [](const unsigned char c) { return c < 32 || c == 127; }))
        { error = "Use a shorter name without line breaks."; return false; }
        if (!this->Ready()) { error = "Profiles are unavailable."; return false; }
        for (size_t index = 0; index < this->entries_.size(); ++index)
            if (index != excludedIndex && SameName(this->entries_[index].name, name))
            { error = "A profile with that name already exists."; return false; }
        return true;
    }

    auto settings_profiles::Create(std::string name, const settings_store& snapshot, std::string& error) -> bool
    {
        if (!this->ValidateName(name, this->entries_.size(), error)) return false;
        if (this->entries_.size() >= 128) { error = "The profile limit has been reached."; return false; }
        FILETIME time{}; ::GetSystemTimeAsFileTime(&time);
        static std::atomic<uint32_t> serial{};
        char id[80]{};
        std::snprintf(id, sizeof(id), "profile-%08X%08X-%08X-%08X", static_cast<uint32_t>(time.dwHighDateTime),
            static_cast<uint32_t>(time.dwLowDateTime), static_cast<uint32_t>(::GetCurrentProcessId()), serial.fetch_add(1));
        const auto path = (std::filesystem::path{this->directory_} / (std::string{id} + ".ini")).string();
        const auto file = ::CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) { error = "Could not create the profile file."; return false; }
        auto store = snapshot.Preferences(path);
        store.WriteString("profile", "name", name); store.WriteInt("profile", "version", 1);
        const auto text = store.ToIniText();
        DWORD written{};
        const bool saved = ::WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) != FALSE && written == text.size() && ::FlushFileBuffers(file) != FALSE;
        ::CloseHandle(file);
        if (!saved) { ::DeleteFileA(path.c_str()); error = "Could not save the new profile."; return false; }
        this->entries_.push_back({id, std::move(name), path});
        this->selected_ = this->entries_.size() - 1;
        error.clear();
        return true;
    }

    auto settings_profiles::Import(const std::span<const import_entry> profiles, const std::span<const import_file> archive,
        std::vector<size_t>& created, std::string& reportPath, std::string& error) -> bool
    {
        created.clear(); reportPath.clear();
        std::filesystem::path staging;
        std::vector<std::filesystem::path> staged;
        try
        {
            if (!this->Ready() || profiles.empty()) throw std::runtime_error{"No profiles are ready to import."};
            std::vector<const import_entry*> pending;
            std::set<std::string> sources;
            for (const auto& profile : profiles)
            {
                const bool exists = std::any_of(this->entries_.begin(), this->entries_.end(), [&](const auto& entry) {
                    return settings_store{entry.path}.ReadString("profile", "lua_source_id") == profile.source_id;
                });
                if (!exists && sources.insert(profile.source_id).second) pending.push_back(&profile);
            }
            if (pending.empty()) { error = "These source profiles have already been imported."; return false; }
            if (this->entries_.size() + pending.size() > 128) throw std::runtime_error{"Import exceeds the 128-profile limit. Select fewer profiles."};
            FILETIME time{}; ::GetSystemTimeAsFileTime(&time);
            static std::atomic<uint32_t> serial{};
            char batch[80]{};
            std::snprintf(batch, sizeof(batch), "%08X%08X-%08X-%08X", time.dwHighDateTime, time.dwLowDateTime,
                ::GetCurrentProcessId(), serial.fetch_add(1));
            const auto root = std::filesystem::weakly_canonical(this->directory_);
            staging = root / (std::string{"pending-lua-"} + batch);
            const auto destination = root / (std::string{"lua-"} + batch);
            if (!std::filesystem::create_directory(staging)) throw std::runtime_error{"Could not stage the import."};
            const auto write = [&](const std::string& name, const std::string_view bytes) {
                if (name.empty() || name.find_first_of("/\\:") != std::string::npos || name == "." || name == "..") throw std::runtime_error{"Invalid archive filename."};
                const auto path = staging / name;
                const auto file = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (file == INVALID_HANDLE_VALUE) throw std::runtime_error{"Could not stage an import file."};
                staged.push_back(path);
                DWORD count{};
                const bool success = bytes.size() <= MAXDWORD && ::WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &count, nullptr) &&
                    count == bytes.size() && ::FlushFileBuffers(file);
                ::CloseHandle(file);
                if (!success) throw std::runtime_error{"Could not save the import. Existing profiles are unchanged."};
            };
            std::vector<entry> entries;
            for (size_t i = 0; i < pending.size(); ++i)
            {
                const auto& profile = *pending[i];
                auto name = this->UniqueName(profile.name);
                auto original = name;
                size_t length = std::min<size_t>(40, original.size());
                while (length < original.size() && length > 0 && (static_cast<unsigned char>(original[length]) & 0xC0) == 0x80) --length;
                original.resize(length);
                for (size_t suffix = 2; std::any_of(entries.begin(), entries.end(), [&](const auto& entry) { return SameName(name, entry.name); }); ++suffix)
                    name = this->UniqueName(original + " " + std::to_string(suffix));
                if (!this->ValidateName(name, this->entries_.size(), error)) throw std::runtime_error{error};
                const auto id = std::string{"profile-lua-"} + batch + "-" + std::to_string(i);
                auto store = profile.preferences.Preferences("");
                store.WriteString("profile", "name", name); store.WriteInt("profile", "version", 1);
                store.WriteString("profile", "lua_source_id", profile.source_id);
                write(id + ".ini", store.ToIniText());
                entries.push_back({id, name, (destination / (id + ".ini")).string()});
            }
            for (const auto& file : archive)
            {
                if (file.name != "report.txt") { write(file.name, file.bytes); continue; }
                std::string report = file.bytes + "\nNative destinations (source order):\n";
                for (const auto& entry : entries) report += entry.name + " -> " + entry.path + "\n";
                report += "\nNative's profile selector retains its normal name ordering. Source profile order is recorded above.\n";
                write(file.name, report);
            }
            if (std::filesystem::weakly_canonical(staging).parent_path() != root || std::filesystem::weakly_canonical(destination).parent_path() != root)
                throw std::runtime_error{"Import path is outside the profile directory."};
            this->entries_.reserve(this->entries_.size() + entries.size()); created.reserve(entries.size());
            auto savedReportPath = (destination / "report.txt").string();
            if (!::MoveFileExW(staging.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH)) throw std::runtime_error{"Could not publish the import. Existing profiles are unchanged."};
            staging.clear();
            for (auto& entry : entries) { created.push_back(this->entries_.size()); this->entries_.push_back(std::move(entry)); }
            reportPath = std::move(savedReportPath);
            error.clear(); return true;
        }
        catch (const std::exception& failure)
        {
            // Only files created in this private staging directory are removed.
            if (!staging.empty()) { std::error_code ignored; for (const auto& file : staged) std::filesystem::remove(file, ignored); std::filesystem::remove(staging, ignored); }
            error = failure.what(); return false;
        }
    }
}
