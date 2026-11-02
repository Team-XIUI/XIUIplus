#pragma once

#include "profile_file.hpp"
#include "lua_setup_import.hpp"
#include "settings_profiles.hpp"

#include <mutex>
#include <atomic>
#include <thread>

namespace xiui
{
    enum class profile_transfer_kind { export_file, import_file, import_key, scan_lua, stage_lua };

    struct profile_transfer_request
    {
        profile_transfer_kind kind{};
        std::filesystem::path path{};
        std::string name{}, key{};
        settings_store snapshot{""};
        bool browse{};
        std::function<lua_setup_preview(const std::filesystem::path&, std::stop_token)> scan_lua{};
        std::function<std::unique_ptr<settings_profiles::staged_import>(std::stop_token, std::string&)> stage_lua{};
    };

    struct profile_transfer_result
    {
        profile_transfer_kind kind{};
        std::filesystem::path path{};
        std::optional<shared_profile> profile{};
        std::string error{};
        bool cancelled{}, stopped{};
        std::string lua_report{};
        std::shared_ptr<lua_setup_preview> lua_preview{};
        std::unique_ptr<settings_profiles::staged_import> lua_staged{};
    };

    // The worker owns its snapshot and picker; only Take() publishes back to the HUD.
    class profile_transfer
    {
    public:
        ~profile_transfer();
        auto Start(profile_transfer_request request) -> bool;
        auto Busy() const -> bool { return this->worker_.joinable(); }
        auto Selecting() const -> bool { return this->selecting_.load(); }
        auto Take() -> std::optional<profile_transfer_result>;
        auto Cancel() -> void;
        auto Stop() -> void;

    private:
        std::jthread worker_{};
        std::mutex mutex_{};
        HWND picker_{};
        std::atomic<bool> selecting_{};
        std::optional<profile_transfer_result> result_{};

        auto Run(profile_transfer_request request, std::stop_token stop) -> void;
        auto PickFile(const profile_transfer_request& request, std::stop_token stop, std::string& error) -> std::filesystem::path;
        static auto CALLBACK PickerHook(HWND window, UINT message, WPARAM wparam, LPARAM lparam) -> UINT_PTR;
    };
}
