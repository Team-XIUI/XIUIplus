#pragma once

#include "profile_share.hpp"

#include <filesystem>

namespace xiui
{
    auto SerializeProfileFile(const std::string& name, const settings_store& snapshot, std::string& error) -> std::string;
    auto ParseProfileFile(std::string_view bytes, std::string& error) -> std::optional<shared_profile>;
    auto SaveProfileFile(const std::filesystem::path& path, const std::string& name, const settings_store& snapshot, std::string& error) -> bool;
    auto ReadProfileFile(const std::filesystem::path& path, std::string& error) -> std::optional<shared_profile>;
}
