#pragma once

#include "settings_store.hpp"

#include <optional>
#include <vector>

namespace xiui
{
    class settings_profiles
    {
    public:
        struct entry { std::string id, name, path; };
        auto Initialize(const std::string& settingsPath) -> void;
        auto Entries(void) const -> const std::vector<entry>& { return this->entries_; }
        auto Current(void) const -> const entry& { return this->entries_[this->selected_]; }
        auto Selected(void) const -> size_t { return this->selected_; }
        auto Ready(void) const -> bool { return !this->entries_.empty(); }
        auto Select(size_t index) -> void { if (index < this->entries_.size()) this->selected_ = index; }
        auto Read(size_t index, std::string& error) const -> std::optional<settings_store>;
        auto Create(std::string name, const settings_store& snapshot, std::string& error) -> bool;
        auto Duplicate(size_t index, std::string name, std::string& error) -> bool;
        auto Remove(size_t index, std::string& error) -> bool;
        auto UniqueName(std::string name) const -> std::string;

    private:
        std::vector<entry> entries_{};
        std::string directory_{};
        size_t selected_{};
        static auto SameName(const std::string& first, const std::string& second) -> bool;
    };
}
