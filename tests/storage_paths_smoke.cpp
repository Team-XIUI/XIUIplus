#include "settings/storage_paths.hpp"
#include "settings/settings_store.hpp"

#include <fstream>
#include <cstdio>
#include <stdexcept>

namespace
{
    namespace fs = std::filesystem;
    auto Check(bool value, const char* message) -> void { if (!value) throw std::runtime_error{message}; }
    auto Write(const fs::path& path, std::string_view bytes) -> void
    {
        fs::create_directories(path.parent_path());
        std::ofstream file{path, std::ios::binary};
        file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        Check(bool(file), "write isolated migration fixture");
    }
    auto Read(const fs::path& path) -> std::string
    {
        std::ifstream file{path, std::ios::binary};
        Check(bool(file), "read migrated file");
        return {std::istreambuf_iterator<char>{file}, {}};
    }
}

int main()
{
    try
    {
        const auto parent = fs::weakly_canonical(fs::temp_directory_path());
        const auto root = parent / ("xiui-storage-" + std::to_string(::GetCurrentProcessId()) + "-" + std::to_string(::GetTickCount64()));
        fs::create_directory(root);
        struct cleanup
        {
            fs::path root, parent;
            ~cleanup()
            {
                std::error_code ignored;
                if (this->root.parent_path() == this->parent && this->root.filename().string().starts_with("xiui-storage-")) fs::remove_all(this->root, ignored);
            }
        } cleanupFiles{root, parent};
        const auto config = root / "config";
        const std::string settings = "[profiles]\r\nactive=profile-a\r\n[profiles.characters]\r\nworld-123=profile-a\r\n[layout.player]\r\nx=777\r\n[readycheck]\r\nsound_file=custom.wav\r\n";
        const std::string theme{"; custom theme\r\n[colors]\r\nprimary=FFAABBCC\r\n"};
        Write(config / "xiuinative.ini", settings);
        Write(config / "xiuinative-theme.ini", theme);
        Write(config / "xiuinative-profiles/profile-a.ini", "[profile]\nname=Personal\nversion=1\n[readycheck]\nsound_file=profile.wav\n");
        Write(config / "xiuinative-profiles/lua-batch/report.txt", "frozen report");
        Write(config / L"xiuinative-profiles/lua-batch/source-\u65E5\u672C.lua", "original source");
        Write(config / "xiuinative-inventories/world-123.ini", "inventory and order data");
        Write(config / "xiuinative-licenses/MinHook.txt", "runtime license");
        Write(root / "plugins/xiuinative-licenses/XIVParty.txt", "older development license");
        Write(root / "logs/xiui/capture.json", "diagnostic report");
        Write(config / "sounds/custom.wav", "custom sound"); Write(config / "sounds/profile.wav", "profile sound");
        const std::string owned = "lua-0123456789abcdef01234567.wav";
        Write(config / "sounds" / owned, "imported sound");
        Write(config / "sounds/another-addon.wav", "unrelated sound"); Write(config / "imgui.ini", "shared imgui");
        const auto paths = xiui::PrepareStorage(root);
        Check(paths.root == config / "xiuinative" && Read(paths.Settings()) == settings && Read(paths.Theme()) == theme, "preserve exact settings/theme bytes and selected character links");
        Check(Read(paths.Profiles() / "profile-a.ini").find("Personal") != std::string::npos && Read(paths.Profiles() / L"lua-batch/source-\u65E5\u672C.lua") == "original source", "preserve profile identities and Unicode source archive paths");
        Check(Read(paths.Inventories() / "world-123.ini") == "inventory and order data" && Read(paths.Licenses() / "XIVParty.txt") == "older development license" && Read(paths.Diagnostics() / "capture.json") == "diagnostic report", "consolidate inventory, licenses and diagnostics");
        Check(Read(paths.Sounds() / "custom.wav") == "custom sound" && Read(paths.Sounds() / "profile.wav") == "profile sound" && Read(paths.Sounds() / owned) == "imported sound", "move owned sounds and copy every referenced custom sound");
        Check(fs::exists(config / "sounds/custom.wav") && fs::exists(config / "sounds/another-addon.wav") && !fs::exists(config / "sounds" / owned) && Read(config / "imgui.ini") == "shared imgui", "preserve shared files owned by users and other addons");
        Check(!fs::exists(config / "xiuinative.ini") && !fs::exists(config / "xiuinative-profiles") && !fs::exists(config / "xiuinative-inventories") && !fs::exists(root / "logs/xiui"), "remove successfully migrated old locations");
        xiui::PrepareStorage(root);
        Check(Read(paths.Settings()) == settings && !fs::exists(paths.root / "legacy"), "repeat loads do not change already migrated data");
        Write(config / "xiuinative.ini", "older settings");
        Write(config / "xiuinative-profiles/profile-a.ini", "older profile");
        Write(config / "xiuinative-profiles/lua-more/report.txt", "newly discovered source");
        xiui::PrepareStorage(root);
        Check(Read(paths.Settings()) == settings && Read(paths.root / "legacy/xiuinative.ini") == "older settings" && Read(paths.root / "legacy/xiuinative-profiles/profile-a.ini") == "older profile" && Read(paths.Profiles() / "lua-more/report.txt") == "newly discovered source", "merge nonconflicting files and archive conflicts without replacing current data");
        Check(xiui::AshitaConfigDirectory(paths.Settings()) == config && xiui::AshitaConfigDirectory(config / "xiuinative.ini") == config, "legacy Lua discovery keeps using config/addons/xiui");

        const auto retry = root / "retry";
        Write(retry / "config/xiuinative.ini", settings); Write(retry / "config/xiuinative-theme.ini", theme);
        const auto file = ::CreateFileW((retry / "config/xiuinative-theme.ini").c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        Check(file != INVALID_HANDLE_VALUE, "lock migration fixture");
        bool failed{};
        try { xiui::PrepareStorage(retry); } catch (...) { failed = true; }
        ::CloseHandle(file);
        Check(failed && Read(retry / "config/xiuinative/settings.ini") == settings && Read(retry / "config/xiuinative-theme.ini") == theme, "interrupted migration preserves both moved and unmoved data");
        const auto resumed = xiui::PrepareStorage(retry);
        Check(Read(resumed.Settings()) == settings && Read(resumed.Theme()) == theme, "retry completes an interrupted migration without resetting settings");
        std::puts("XIUI storage consolidation, conflicts, shared resources and interrupted migration checks passed.");
        return 0;
    }
    catch (const std::exception& error) { std::fprintf(stderr, "Storage check: %s\n", error.what()); return 1; }
}
