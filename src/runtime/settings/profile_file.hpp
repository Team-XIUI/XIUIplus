#pragma once

#include "profile_share.hpp"

#include <filesystem>
#include <stop_token>

namespace xiui
{
    auto SerializeProfileFile(const std::string& name, const settings_store& snapshot, std::string& error) -> std::string;
    auto ParseProfileFile(std::string_view bytes, std::string& error) -> std::optional<shared_profile>;
    auto SaveProfileFile(const std::filesystem::path& path, const std::string& name, const settings_store& snapshot, std::string& error, std::stop_token stop = {}) -> bool;
    auto ReadProfileFile(const std::filesystem::path& path, std::string& error) -> std::optional<shared_profile>;
}
