#pragma once

#include "menu_texture.hpp"

#include <condition_variable>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace xiui::resources
{
    struct custom_icon_entry { std::string key, label, category; };
    enum class custom_icon_state : uint8_t { idle, loading, ready, failed };
    enum class custom_icon_scan_error : uint8_t { none, folder, limit };
    struct custom_icon_catalog
    {
        std::shared_ptr<const std::vector<custom_icon_entry>> entries;
        uint64_t revision{}, generation{};
        bool scanning{}, scanned{};
        custom_icon_scan_error error{};
    };
    struct decoded_custom_icon { std::string key; uint64_t generation{}; std::optional<menu_texture> image; };

    class custom_icon_library
    {
    public:
        ~custom_icon_library(void) { this->Shutdown(); }
        auto SetDirectory(std::filesystem::path directory) -> void;
        auto Directory(void) const -> const std::filesystem::path& { return this->directory_; }
        auto Refresh(void) -> void;
        auto Catalog(void) const -> custom_icon_catalog;
        auto Request(std::string_view key) -> void;
        auto State(std::string_view key) const -> custom_icon_state;
        auto TakeImages(size_t limit = 4) -> std::vector<decoded_custom_icon>;
        auto Shutdown(void) -> void;
        static auto RelativePath(std::string_view key) -> std::optional<std::filesystem::path>;
        static auto DecodeImage(std::span<const uint8_t> bytes) -> std::optional<menu_texture>;

    private:
        mutable std::mutex mutex_{};
        std::condition_variable wake_{};
        std::jthread worker_{};
        std::filesystem::path directory_{};
        std::shared_ptr<const std::vector<custom_icon_entry>> entries_{std::make_shared<const std::vector<custom_icon_entry>>()};
        std::unordered_map<std::string, custom_icon_state> states_{};
        std::deque<std::string> requested_{};
        std::deque<decoded_custom_icon> completed_{};
        uint64_t revision_{}, generation_{};
        bool scan_requested_{}, scanning_{}, scanned_{};
        custom_icon_scan_error error_{};
        auto Start(void) -> bool;
        auto Run(std::stop_token stop) -> void;
    };
}
