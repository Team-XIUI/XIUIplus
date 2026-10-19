#pragma once

#include "lua_data.hpp"
#include "settings_store.hpp"

#include <filesystem>

namespace xiui
{
    enum class lua_import_severity { information, warning, error };
    struct lua_import_issue { lua_import_severity severity{}; std::string path, message; };
    struct lua_import_resource { std::filesystem::path source; std::string destination; };
    struct lua_profile_preview
    {
        std::string name, version;
        settings_store preferences{settings_store::FromIniText("")};
        settings_store local_input{settings_store::FromIniText("")};
        std::vector<lua_import_issue> issues{};
        std::vector<lua_import_resource> resources{};
        size_t converted{}, internal{}, unresolved{};
        auto Valid() const -> bool;
        auto Complete() const -> bool;
        auto Report() const -> std::string;
    };

    auto ConvertLuaProfile(const lua_data::value& document, const settings_store& nativeDefaults,
        std::string name, std::string_view version = "1.8.4") -> lua_profile_preview;
}
