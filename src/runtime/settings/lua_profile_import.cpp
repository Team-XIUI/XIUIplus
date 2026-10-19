#include "lua_import_context.hpp"
#include "lua_profile_schema.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <sstream>

namespace xiui::lua_import
{
    namespace
    {
        auto Walk(const lua_data::value& item, const std::string& path, const std::function<void(const std::string&, const lua_data::value&)>& visit) -> void
        {
            if (item.type != lua_data::value::kind::table || item.fields.empty()) { visit(path, item); return; }
            for (const auto& [key, child] : item.fields) Walk(child, path.empty() ? key : path + "." + key, visit);
        }
        auto Defaults(const std::string_view version) -> lua_data::value
        {
            lua_data::value root{lua_data::value::kind::table};
            const auto entries = version == "1.8.5" ? std::span<const lua_schema::field>{lua_schema::defaults_185} : std::span<const lua_schema::field>{lua_schema::defaults_184};
            for (const auto& entry : entries)
            {
                auto* table = &root; std::string_view path{entry.path};
                for (size_t start = 0; start < path.size();)
                {
                    const auto end = path.find('.', start);
                    const auto key = std::string{path.substr(start, end == path.npos ? path.size() - start : end - start)};
                    const auto keyType = !key.empty() && std::all_of(key.begin(), key.end(), [](const char c) { return c >= '0' && c <= '9'; }) ? lua_data::scalar_kind::number : lua_data::scalar_kind::string;
                    if (end == path.npos)
                    {
                        lua_data::value item{entry.type == 't' ? lua_data::value::kind::table : lua_data::value::kind::scalar, entry.value};
                        item.scalar_type = entry.type == 'b' ? lua_data::scalar_kind::boolean : entry.type == 'n' ? lua_data::scalar_kind::number : lua_data::scalar_kind::string;
                        table->Set(key, std::move(item), keyType); break;
                    }
                    auto found = std::find_if(table->fields.begin(), table->fields.end(), [&](const auto& pair) { return pair.first == key && pair.second.key_type == keyType; });
                    if (found == table->fields.end()) { table->Set(key, {lua_data::value::kind::table}, keyType); found = std::prev(table->fields.end()); }
                    table = &found->second; start = end + 1;
                }
            }
            return root;
        }
        auto Argb(const lua_data::value& input) -> D3DCOLOR
        {
            if (input.scalar_type == lua_data::scalar_kind::number) return static_cast<D3DCOLOR>(static_cast<uint64_t>(input.Number()));
            auto hex = input.text; if (hex.starts_with('#')) hex.erase(0, 1);
            char* end{}; const auto number = std::strtoul(hex.c_str(), &end, 16);
            if (*end != '\0' || (hex.size() != 6 && hex.size() != 8)) throw std::runtime_error{"Invalid Lua color"};
            return hex.size() == 6 ? 0xFF000000 | number : static_cast<D3DCOLOR>((number >> 8) | (number << 24));
        }
    }
    auto context::Number(const std::string_view path, const double fallback) const -> double { const auto* item = this->Get(path); return item ? item->Number(fallback) : fallback; }
    auto context::Boolean(const std::string_view path, const bool fallback) const -> bool { const auto* item = this->Get(path); return item ? item->Boolean(fallback) : fallback; }
    auto context::String(const std::string_view path, std::string fallback) const -> std::string { const auto* item = this->Get(path); return item ? item->String(std::move(fallback)) : fallback; }
    auto context::Mark(std::string path, std::string destination) -> void { this->handled[std::move(path)] = std::move(destination); }
    auto context::Tree(const std::string_view path, std::string destination) -> void
    { if (const auto* item = this->Get(path)) Walk(*item, std::string{path}, [&](const auto& child, const auto&) { this->Mark(child, destination); }); }
    auto context::Ignore(const std::string_view path, const char* reason) -> void { this->Tree(path, std::string{"internal: "} + reason); }
    auto context::Map(const std::string_view path, const std::string_view section, const char* key, const double multiplier) -> void
    {
        const auto* item = this->Get(path); if (!item || item->type != lua_data::value::kind::scalar) return;
        if (!this->Supported(section, key))
        { this->Mark(std::string{path}, "unsupported: no Native equivalent for " + std::string{section} + "." + key); return; }
        const std::string group{section};
        if (item->scalar_type == lua_data::scalar_kind::boolean) this->result.preferences.WriteBool(group.c_str(), key, item->Boolean());
        else if (item->scalar_type == lua_data::scalar_kind::number) this->result.preferences.WriteFloat(group.c_str(), key, static_cast<float>(item->Number() * multiplier));
        else this->result.preferences.WriteString(group.c_str(), key, item->text);
        this->Mark(std::string{path}, group + "." + key);
    }
    auto context::Maps(const std::string_view prefix, const std::string_view section, std::initializer_list<std::pair<const char*, const char*>> fields) -> void
    { for (const auto& [source, destination] : fields) this->Map(std::string{prefix} + source, section, destination); }
    auto context::Bool(const std::string_view path, const std::string_view section, const char* key, const bool invert) -> void
    {
        if (!this->Get(path)) return;
        if (!this->Supported(section, key))
        { this->Tree(path, "unsupported: no Native equivalent for " + std::string{section} + "." + key); return; }
        this->result.preferences.WriteBool(std::string{section}.c_str(), key, this->Boolean(path) != invert); this->Mark(std::string{path}, std::string{section} + "." + key);
    }
    auto context::Color(const std::string_view path, const std::string_view section, const char* key) -> void
    {
        const auto* item = this->Get(path); if (!item) return;
        if (!this->Supported(section, key))
        { this->Tree(path, "unsupported: no Native color equivalent for " + std::string{section} + "." + key); return; }
        try { this->result.preferences.WriteArgb(std::string{section}.c_str(), key, Argb(*item)); this->Mark(std::string{path}, std::string{section} + "." + key); }
        catch (const std::exception& error) { this->Issue(lua_import_severity::error, std::string{path}, error.what()); }
    }
    auto context::TextColor(const std::string_view path, const std::string_view section) -> void { this->Color(path, section, "top_color"); this->Color(path, section, "bottom_color"); }
    auto context::TextSize(const std::string_view path, const std::string_view section, const double multiplier) -> void { this->Map(path, section, "scale", multiplier * this->scale / 15.0); }
    auto context::Gradient(const std::string_view path, const std::string_view section, const char* top, const char* bottom) -> void
    {
        const auto prefix = std::string{path} + ".";
        this->Color(prefix + "start", section, top);
        this->Color(prefix + "stop", section, bottom);
        if (this->Get(prefix + "enabled"))
        {
            if (!this->Boolean(prefix + "enabled", true)) this->Color(prefix + "start", section, bottom);
            this->Mark(prefix + "enabled", std::string{section} + "." + top + "/" + bottom);
        }
    }
    auto context::Choice(const std::string_view path, const std::string_view section, const char* key, const std::initializer_list<const char*> values) -> void
    {
        if (!this->Get(path)) return;
        if (!this->Supported(section, key)) { this->Tree(path, "unsupported: no Native equivalent for " + std::string{section} + "." + key); return; }
        const auto selected = this->String(path); int index{};
        for (const auto* value : values)
        {
            if (::_stricmp(value, selected.c_str()) == 0)
            { this->result.preferences.WriteInt(std::string{section}.c_str(), key, index); this->Mark(std::string{path}, std::string{section} + "." + key); return; }
            ++index;
        }
        this->Tree(path, "unsupported: source choice " + selected);
    }
    auto context::Issue(const lua_import_severity severity, std::string path, std::string message) -> void
    { this->result.issues.push_back({severity, std::move(path), std::move(message)}); }
    auto context::Finish() -> void
    {
        auto filtered = this->defaults.Preferences("");
        this->result.preferences.VisitPreferences([&](const auto section, const auto key, const auto value) {
            if (this->Supported(section, key)) filtered.WriteString(std::string{section}.c_str(), std::string{key}.c_str(), std::string{value});
        });
        this->result.preferences = std::move(filtered);
        Walk(this->source, "", [&](const std::string& path, const auto&) {
            const auto found = this->handled.find(path);
            if (found == this->handled.end() || found->second.starts_with("unsupported:"))
            { ++this->result.unresolved; this->Issue(lua_import_severity::warning, path, found == this->handled.end() ? "Not imported: no supported Native equivalent." : found->second); }
            else if (found->second.starts_with("internal:")) { ++this->result.internal; this->Issue(lua_import_severity::information, path, found->second); }
            else { ++this->result.converted; this->Issue(lua_import_severity::information, path, "Converted: " + found->second); }
        });
    }
    auto context::Supported(const std::string_view section, const std::string_view key) const -> bool
    {
        if (section.starts_with("actionbars.palette.") || section.starts_with("actionbars.macro.")) return true;
        return this->defaults.ReadString(std::string{section}.c_str(), std::string{key}.c_str(), "\x01") != "\x01";
    }
    auto context::Window(const std::string_view prefix, const std::string_view section, const char* theme, const char* flag) -> void
    {
        const auto source = std::string{prefix} + theme;
        if (!this->Get(source)) return;
        const auto name = this->String(source);
        const std::string destination{section};
        if (this->Supported(section, flag)) this->result.preferences.WriteBool(destination.c_str(), flag, name != "-None-" && name != "None");
        else if ((name == "-None-" || name == "None") && this->Supported(section, "opacity")) this->result.preferences.WriteFloat(destination.c_str(), "opacity", 0);
        this->Mark(source, "unsupported: per-module skin '" + name + "'; Native keeps the game's Window Type");
        this->Maps(prefix, section, {{"backgroundOpacity", "opacity"}, {"bgOpacity", "opacity"}});
    }
}

namespace xiui
{
    auto lua_profile_preview::Valid() const -> bool
    { return std::none_of(this->issues.begin(), this->issues.end(), [](const auto& issue) { return issue.severity == lua_import_severity::error; }); }
    auto lua_profile_preview::Complete() const -> bool
    { return this->unresolved == 0 && std::none_of(this->issues.begin(), this->issues.end(), [](const auto& issue) { return issue.severity == lua_import_severity::error; }); }
    auto lua_profile_preview::Report() const -> std::string
    {
        std::ostringstream out;
        out << this->name << " (XIUI Lua " << this->version << ")\n" << this->converted << " converted fields; " << this->internal << " internal fields; " << this->unresolved << " unsupported fields.\n";
        for (const auto severity : {lua_import_severity::error, lua_import_severity::warning, lua_import_severity::information})
            for (const auto& issue : this->issues) if (issue.severity == severity) out << '\n' << issue.path << ": " << issue.message;
        return out.str();
    }
    auto ConvertLuaProfile(const lua_data::value& document, const settings_store& nativeDefaults, std::string name, const std::string_view version) -> lua_profile_preview
    {
        lua_import::context context{};
        context.source = lua_data::UserSettings(document);
        context.source.MergeDefaults(lua_import::Defaults(version));
        context.result.name = std::move(name); context.result.version = std::string{version};
        context.result.preferences = nativeDefaults.Preferences("");
        context.defaults = nativeDefaults.Preferences("");
        context.scale = context.Number("globalScale", 1);
        if (context.scale <= 0 || context.scale > 8) { context.Issue(lua_import_severity::error, "globalScale", "Invalid source scale."); return std::move(context.result); }
        lua_import::ConvertCommon(context); lua_import::ConvertParty(context); lua_import::ConvertPet(context);
        lua_import::ConvertActions(context); lua_import::ConvertExtras(context);
        context.Finish(); return std::move(context.result);
    }
}
