#include "settings_profiles.hpp"
#include "chat/chat_encoding.hpp"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace xiui
{
    auto OverlayProfilePreferences(const settings_store& target, const settings_store& source) -> void
    {
        target.OverlayPreferences(source);
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
}
