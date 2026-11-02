#pragma once

#include "settings_store.hpp"

#include <optional>
#include <vector>
#include <span>
#include <filesystem>
#include <memory>
#include <stop_token>

namespace xiui
{
    auto OverlayProfilePreferences(const settings_store& target, const settings_store& source) -> void;

    class settings_profiles
    {
    public:
        struct entry { std::string id, name, path; };
        struct import_entry { std::string name, source_id; settings_store preferences; };
        struct import_file { std::string name, bytes; };
        struct staged_import
        {
            staged_import() = default;
            staged_import(const staged_import&) = delete;
            auto operator=(const staged_import&) -> staged_import& = delete;
            ~staged_import();
            std::filesystem::path staging, destination;
            std::vector<std::filesystem::path> files;
            std::vector<std::pair<std::filesystem::path, std::filesystem::path>> sounds;
            std::vector<entry> entries;
        };
        auto Initialize(const std::string& settingsPath) -> void;
        auto Entries(void) const -> const std::vector<entry>& { return this->entries_; }
        auto Current(void) const -> const entry& { return this->entries_[this->selected_]; }
        auto Selected(void) const -> size_t { return this->selected_; }
        auto Ready(void) const -> bool { return !this->entries_.empty(); }
        auto Select(size_t index) -> void { if (index < this->entries_.size()) this->selected_ = index; }
        auto Read(size_t index, std::string& error) const -> std::optional<settings_store>;
        auto Create(std::string name, const settings_store& snapshot, std::string& error) -> bool;
        auto Import(std::span<const import_entry> profiles, std::span<const import_file> archive,
            std::vector<size_t>& created, std::string& reportPath, std::string& error) -> bool;
        auto StageImport(std::span<const import_entry> profiles, std::span<const import_file> archive,
            std::span<const import_file> sounds, std::string& error, std::stop_token stop = {}) const -> std::unique_ptr<staged_import>;
        auto CommitImport(staged_import& staged, std::vector<size_t>& created, std::string& reportPath, std::string& error) -> bool;
        auto Duplicate(size_t index, std::string name, std::string& error) -> bool;
        auto Rename(size_t index, std::string name, std::string& error) -> bool;
        auto Remove(size_t index, std::string& error) -> bool;
        auto UniqueName(std::string name) const -> std::string;
        auto BoundProfile(std::string_view character) const -> std::optional<size_t>;

    private:
        std::vector<entry> entries_{};
        std::string directory_{};
        std::string settings_path_{};
        size_t selected_{};
        static auto SameName(const std::string& first, const std::string& second) -> bool;
        auto ValidateName(std::string& name, size_t excludedIndex, std::string& error) const -> bool;
    };
}
