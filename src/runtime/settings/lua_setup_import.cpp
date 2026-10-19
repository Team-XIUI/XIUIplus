#include "lua_setup_import.hpp"
#include "chat/chat_encoding.hpp"
#include "modules/readycheck/ready_check_wav.hpp"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <cstdio>
#include <cmath>
#include <map>

namespace xiui
{
    auto LuaSourceFingerprint(const std::string_view bytes) -> std::string
    {
        uint64_t a = 14695981039346656037ull, b = 7809847782465536322ull;
        for (const unsigned char c : bytes) { a = (a ^ c) * 1099511628211ull; b = (b ^ c) * 14029467366897019727ull; }
        char text[40]{}; std::snprintf(text, sizeof(text), "%016llx%016llx", a, b); return text;
    }
    auto lua_setup_preview::Valid() const -> bool
    { return this->error.empty() && !this->profiles.empty() && std::all_of(this->profiles.begin(), this->profiles.end(), [](const auto& value) { return value.preview.Valid(); }); }
    auto lua_setup_preview::Report(const size_t selection) const -> std::string
    {
        std::ostringstream out;
        if (!this->error.empty()) out << "Cannot import: " << this->error << '\n';
        out << this->profiles.size() << " source profiles; " << this->sources.size() << " captured source files.\n";
        out << "Imports create new profiles. Current settings, Default and existing character links are preserved.\n";
        for (const auto& note : this->notes) out << "\nNote: " << note << '\n';
        for (size_t i = 0; i < this->profiles.size(); ++i)
            if (selection == SIZE_MAX || selection == i) out << "\n" << this->profiles[i].preview.Report() << '\n';
        return out.str();
    }
    auto DiscoverLuaSetup(const std::filesystem::path& source, const settings_store& defaults,
        const std::function<void(lua_profile_preview&)>& normalize) -> lua_setup_preview
    {
        lua_setup_preview result;
        try
        {
            size_t total{};
            std::map<std::filesystem::path, size_t> captured;
            const auto read = [&](const std::filesystem::path& path) -> std::string {
                const auto canonical = std::filesystem::weakly_canonical(path);
                if (const auto found = captured.find(canonical); found != captured.end()) return result.sources[found->second].bytes;
                if (result.sources.size() >= 1024) throw std::runtime_error{"The setup contains too many source files. Select a smaller source folder."};
                const auto size = std::filesystem::file_size(path);
                if (size > 16 * 1024 * 1024 || total + size > 64 * 1024 * 1024) throw std::runtime_error{"Source data exceeds the import limit."};
                std::ifstream file{path, std::ios::binary};
                if (!file) throw std::runtime_error{"Could not read " + ToUtf8(path.wstring())};
                std::string bytes{std::istreambuf_iterator<char>{file}, {}};
                if (bytes.size() != size) throw std::runtime_error{"A source file changed while reading. Scan again."};
                total += bytes.size();
                captured.emplace(canonical, result.sources.size());
                result.sources.push_back({path, bytes, LuaSourceFingerprint(bytes)});
                return bytes;
            };
            auto root = std::filesystem::weakly_canonical(source);
            const bool single = std::filesystem::is_regular_file(root) && root.filename() != "profilelist.lua";
            const auto selectedFile = root;
            if (std::filesystem::is_regular_file(root)) root = root.parent_path();
            if (std::filesystem::is_directory(root / "config" / "addons" / "xiui")) root /= "config/addons/xiui";
            else if (std::filesystem::is_directory(root / "xiui" / "profiles")) root /= "xiui";
            else if (std::filesystem::is_regular_file(root / "XIUI.lua") && std::filesystem::is_directory(root.parent_path().parent_path() / "config/addons/xiui"))
                root = root.parent_path().parent_path() / "config/addons/xiui";
            if (::_wcsicmp(root.filename().c_str(), L"profiles") == 0) root = root.parent_path();
            if (!std::filesystem::is_directory(root)) throw std::runtime_error{"Choose an XIUI settings folder or a saved Lua profile."};
            result.root = root;
            lua_data::value legacyPositions{lua_data::value::kind::table};
            for (const auto& path : {root / "imgui.ini", root.parent_path().parent_path() / "imgui.ini"})
                if (std::filesystem::is_regular_file(path))
                {
                    const auto text = read(path);
                    std::istringstream input{text}; std::string line, window;
                    while (std::getline(input, line))
                    {
                        if (line.starts_with("[Window]["))
                        {
                            const auto close = line.find(']', 9); window = close == std::string::npos ? "" : line.substr(9, close - 9);
                            static const std::set<std::string> known{"PlayerBar", "TargetBar", "PartyList", "PartyList2", "PartyList3", "EnemyList", "CastBar", "CastCost",
                                "PetBar", "PetBarTarget", "ExpBar", "GilTracker", "InventoryTracker", "SatchelTracker", "SafeTracker", "StorageTracker", "LockerTracker",
                                "WardrobeTracker", "Satchel", "TreasurePool", "MagicBurst", "BlueMagicLearned", "PhantomRoll", "Crossbar", "Notifications",
                                "Notifications_Group1", "Notifications_Group2", "Notifications_Group3", "Notifications_Group4", "Notifications_Group5", "Notifications_Group6",
                                "Hotbar1", "Hotbar2", "Hotbar3", "Hotbar4", "Hotbar5", "Hotbar6", "Hotbar7", "Hotbar8", "Hotbar9", "Hotbar10"};
                            if (!known.contains(window)) window.clear();
                        }
                        else if (!window.empty() && line.starts_with("Pos="))
                        {
                            float x{}, y{};
                            if (std::sscanf(line.c_str(), "Pos=%f,%f", &x, &y) == 2 && std::isfinite(x) && std::isfinite(y))
                            {
                                lua_data::value position{lua_data::value::kind::table};
                                position.Set("x", {lua_data::value::kind::scalar, std::to_string(x), {}, lua_data::scalar_kind::number});
                                position.Set("y", {lua_data::value::kind::scalar, std::to_string(y), {}, lua_data::scalar_kind::number});
                                legacyPositions.Set(window == "Notifications" ? "Notifications_Group1" : window, std::move(position));
                            }
                        }
                    }
                    result.notes.push_back("Legacy ImGui coordinates fill missing saved positions. Native adjusts out-of-screen placement when applied; no source resolution is assumed.");
                    break;
                }
            std::string version = "1.8.4";
            const auto listPath = root / "profiles" / "profilelist.lua";
            std::vector<std::filesystem::path> files;
            if (std::filesystem::is_regular_file(listPath))
            {
                const auto list = lua_data::reader{read(listPath)}.Parse();
                const auto* savedVersion = list.Find("version");
                if (savedVersion && savedVersion->String().starts_with("1.8.5")) version = "1.8.5";
                else if (savedVersion && !savedVersion->String().starts_with("1.8.4"))
                    result.notes.push_back("Unrecognized source version " + savedVersion->String() + "; using the 1.8.4 schema and reporting unmapped fields.");
                if (const auto* order = list.Find("order")) for (const auto& [key, value] : order->fields)
                {
                    const auto name = value.String();
                    if (name.empty() || name.find_first_of("/\\:") != std::string::npos || name == "." || name == "..") throw std::runtime_error{"Invalid profile filename in profilelist.lua."};
                    files.push_back(root / "profiles" / (FromUtf8(name) + L".lua"));
                }
            }
            else result.notes.push_back("No version manifest found; using the 1.8.4 defaults. A single file may omit character links and legacy positions.");
            if (single) files = {selectedFile};
            else
            {
                const auto folder = std::filesystem::is_directory(root / "profiles") ? root / "profiles" : root;
                std::vector<std::filesystem::path> extra;
                for (const auto& file : std::filesystem::directory_iterator(folder))
                    if (file.is_regular_file() && file.path().extension() == ".lua" && file.path().filename() != "profilelist.lua" &&
                        file.path().filename() != "inventory_cache.lua" && std::find(files.begin(), files.end(), file.path()) == files.end()) extra.push_back(file.path());
                std::sort(extra.begin(), extra.end()); files.insert(files.end(), extra.begin(), extra.end());
            }
            const auto convert = [&](const lua_data::value& document, const std::string& name) {
                if (result.profiles.size() >= 128) throw std::runtime_error{"The source contains more than 128 profiles."};
                auto prepared = lua_data::UserSettings(document);
                const auto* modern = prepared.Find("windowPositions");
                auto positions = modern != nullptr ? *modern : lua_data::value{lua_data::value::kind::table};
                for (const auto& [key, position] : legacyPositions.fields) if (!positions.Find(key)) positions.Set(key, position);
                if (!positions.fields.empty()) prepared.Set("windowPositions", std::move(positions));
                auto preview = ConvertLuaProfile(prepared, defaults, name, version);
                if (const auto* sound = prepared.Find("readyCheckSoundFile"); sound && !sound->String().empty())
                {
                    preview.preferences.WriteInt("readycheck", "sound_choice", 0);
                    const auto filename = std::filesystem::path{FromUtf8(sound->String())}.filename();
                    bool copied{};
                    if (ReadyCheckWavFilename(ToUtf8(filename.wstring())))
                        for (const auto& folder : {root / "sounds", root / "assets/sounds", root.parent_path().parent_path().parent_path() / "addons/xiui/assets/sounds"})
                        {
                            const auto resource = folder / filename;
                            if (!std::filesystem::is_regular_file(resource)) continue;
                            const auto bytes = read(resource);
                            if (bytes.size() > 8 * 1024 * 1024 || DecodeReadyCheckWav({reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size()}).samples.empty()) break;
                            const auto destination = "lua-" + LuaSourceFingerprint(bytes).substr(0, 24) + ".wav";
                            preview.resources.push_back({resource, destination});
                            preview.preferences.WriteString("readycheck", "sound_file", destination);
                            preview.preferences.WriteInt("readycheck", "sound_choice", 1);
                            preview.issues.push_back({lua_import_severity::information, "readyCheckSoundFile", "Compatible WAV will be copied to config/sounds/" + destination});
                            copied = true; break;
                        }
                    if (!copied) preview.issues.push_back({lua_import_severity::warning, "readyCheckSoundFile", "Custom sound is missing or unsupported; Native keeps its built-in chime."});
                }
                if (normalize) normalize(preview);
                auto identity = name + "\n" + version + "\n" + lua_data::ToLiteral(prepared);
                for (const auto& resource : preview.resources) identity += "\n" + resource.destination;
                const auto id = LuaSourceFingerprint(identity);
                if (std::none_of(result.profiles.begin(), result.profiles.end(), [&](const auto& profile) { return profile.source_id == id; }))
                    result.profiles.push_back({std::move(preview), id});
            };
            std::set<std::filesystem::path> visited;
            for (const auto& path : files)
            {
                if (!visited.insert(path).second) continue;
                if (!std::filesystem::is_regular_file(path)) throw std::runtime_error{"A listed profile is missing: " + ToUtf8(path.wstring())};
                const auto bytes = read(path);
                const auto document = lua_data::reader{bytes}.Parse();
                if (const auto* profiles = document.Find("profiles"))
                    for (const auto& [name, profile] : profiles->fields) convert(profile, name);
                else
                {
                    const auto& settings = lua_data::UserSettings(document);
                    if (settings.Find("currentProfile") && !settings.Find("showPlayerBar") && !document.Find("userSettings"))
                        throw std::runtime_error{"This file selects a profile; choose its XIUI folder to discover the profile settings."};
                    convert(document, ToUtf8(path.stem().wstring()));
                }
            }
            if (!single)
            {
                for (const auto& directory : std::filesystem::directory_iterator(root))
                {
                    if (!directory.is_directory() || directory.path().filename() == "profiles" || directory.path().filename() == "backups" || directory.is_symlink()) continue;
                    const auto settings = directory.path() / "settings.lua";
                    if (!std::filesystem::is_regular_file(settings)) continue;
                    const auto bytes = read(settings);
                    const auto inventory = directory.path() / "inventory_cache.lua";
                    if (std::filesystem::is_regular_file(inventory)) { read(inventory); result.notes.push_back(ToUtf8(directory.path().filename().wstring()) + " inventory cache is archived without replacing Native observations."); }
                    try
                    {
                        const auto document = lua_data::reader{bytes}.Parse();
                        if (const auto* profile = document.Find("currentProfile")) result.characters.push_back({ToUtf8(directory.path().filename().wstring()), profile->String()});
                        if (const auto* profiles = document.Find("profiles")) for (const auto& [name, profile] : profiles->fields) convert(profile, name);
                        if (document.Find("userSettings") || document.Find("showPlayerBar")) convert(document, "Legacy " + ToUtf8(directory.path().filename().wstring()));
                        if (document.Find("satchelInventoryCache")) result.notes.push_back("Historical inventory from " + ToUtf8(directory.path().filename().wstring()) + " is archived, not treated as a fresh Native observation.");
                    }
                    catch (const std::exception& error) { result.notes.push_back("Character metadata archived without conversion: " + std::string{error.what()}); }
                }
                for (const auto& path : {root / "inventory_cache.lua", root / "imgui.ini", root / "profiles" / "imgui.ini"})
                    if (std::filesystem::is_regular_file(path)) { read(path); result.notes.push_back(ToUtf8(path.filename().wstring()) + " is included in the source archive; historical data and unmatched positions remain separate from live Native state."); }
            }
            if (result.profiles.empty()) throw std::runtime_error{"No saved XIUI profiles were found in this location."};
            if (!result.characters.empty()) result.notes.push_back(std::to_string(result.characters.size()) + " character associations are recorded in the report; existing Native associations are preserved.");
            for (const auto& link : result.characters) result.notes.push_back("Source character " + link.character + " selected " + link.profile + ".");
        }
        catch (const std::exception& error) { result.error = error.what(); }
        return result;
    }
}
