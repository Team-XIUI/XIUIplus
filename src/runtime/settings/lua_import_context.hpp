#pragma once

#include "lua_profile_import.hpp"

#include <map>
#include <set>
#include <span>

namespace xiui::lua_import
{
    struct conversion { std::string source, destination; };
    struct context
    {
        lua_data::value source;
        lua_profile_preview result;
        settings_store defaults{settings_store::FromIniText("")};
        std::map<std::string, std::string> handled;
        double scale{1};

        auto Get(std::string_view path) const -> const lua_data::value* { return this->source.Path(path); }
        auto Number(std::string_view path, double fallback = 0) const -> double;
        auto Boolean(std::string_view path, bool fallback = false) const -> bool;
        auto String(std::string_view path, std::string fallback = {}) const -> std::string;
        auto Mark(std::string path, std::string destination) -> void;
        auto Tree(std::string_view path, std::string destination) -> void;
        auto Ignore(std::string_view path, const char* reason) -> void;
        auto Map(std::string_view path, std::string_view section, const char* key, double multiplier = 1) -> void;
        auto Maps(std::string_view prefix, std::string_view section, std::initializer_list<std::pair<const char*, const char*>> fields) -> void;
        auto Bool(std::string_view path, std::string_view section, const char* key, bool invert = false) -> void;
        auto Color(std::string_view path, std::string_view section, const char* key) -> void;
        auto TextColor(std::string_view path, std::string_view section) -> void;
        auto TextSize(std::string_view path, std::string_view section, double multiplier = 1) -> void;
        auto Gradient(std::string_view path, std::string_view section, const char* top = "fill_top", const char* bottom = "fill_bottom") -> void;
        auto Choice(std::string_view path, std::string_view section, const char* key, std::initializer_list<const char*> values) -> void;
        auto Issue(lua_import_severity severity, std::string path, std::string message) -> void;
        auto Window(std::string_view prefix, std::string_view section, const char* theme = "backgroundTheme", const char* flag = "background") -> void;
        auto Finish() -> void;
        auto Supported(std::string_view section, std::string_view key) const -> bool;
    };

    auto ConvertCommon(context& data) -> void;
    auto ConvertParty(context& data) -> void;
    auto ConvertPet(context& data) -> void;
    auto ConvertActions(context& data) -> void;
    auto ConvertExtras(context& data) -> void;
}
