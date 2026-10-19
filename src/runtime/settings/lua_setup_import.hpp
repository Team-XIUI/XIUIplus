#pragma once
#include "lua_profile_import.hpp"
#include <functional>

namespace xiui
{
    struct lua_source_file { std::filesystem::path path; std::string bytes, fingerprint; };
    struct lua_setup_profile { lua_profile_preview preview; std::string source_id; };
    struct lua_character_link { std::string character, profile; };
    struct lua_setup_preview
    {
        std::filesystem::path root;
        std::vector<lua_setup_profile> profiles;
        std::vector<lua_source_file> sources;
        std::vector<lua_character_link> characters;
        std::vector<std::string> notes;
        std::string error;
        auto Valid() const -> bool;
        auto Report(size_t selection = SIZE_MAX) const -> std::string;
    };
    auto LuaSourceFingerprint(std::string_view bytes) -> std::string;
    auto DiscoverLuaSetup(const std::filesystem::path& source, const settings_store& defaults,
        const std::function<void(lua_profile_preview&)>& normalize = {}) -> lua_setup_preview;
}
