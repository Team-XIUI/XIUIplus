#pragma once

#include <filesystem>

namespace xiui
{
    struct storage_paths
    {
        std::filesystem::path root;
        auto Settings() const -> std::filesystem::path { return this->root / "settings.ini"; }
        auto Theme() const -> std::filesystem::path { return this->root / "theme.ini"; }
        auto Profiles() const -> std::filesystem::path { return this->root / "profiles"; }
        auto Inventories() const -> std::filesystem::path { return this->root / "inventories"; }
        auto Licenses() const -> std::filesystem::path { return this->root / "licenses"; }
        auto Sounds() const -> std::filesystem::path { return this->root / "sounds"; }
        auto Diagnostics() const -> std::filesystem::path { return this->root / "diagnostics"; }
    };

    auto PrepareStorage(const std::filesystem::path& install) -> storage_paths;
    auto AshitaConfigDirectory(const std::filesystem::path& settings) -> std::filesystem::path;
}
