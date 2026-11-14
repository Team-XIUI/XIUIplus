#include "settings/settings_store.hpp"

#include <filesystem>
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>

namespace
{
    auto ReadUtf8(const std::filesystem::path& path) -> std::string
    {
        std::ifstream file{path, std::ios::binary};
        if (!file) throw std::runtime_error{"Could not read the saved settings."};
        const std::string bytes{std::istreambuf_iterator<char>{file}, {}};
        if (!bytes.starts_with("\xFF\xFE")) return bytes;
        if (bytes.size() % 2 != 0) throw std::runtime_error{"Invalid UTF-16 settings."};
        std::wstring wide((bytes.size() - 2) / sizeof(wchar_t), L'\0');
        std::memcpy(wide.data(), bytes.data() + 2, wide.size() * sizeof(wchar_t));
        const int size = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
        if (size == 0 && !wide.empty()) throw std::runtime_error{"Invalid UTF-16 settings."};
        std::string text(size, '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), text.data(), size, nullptr, nullptr);
        return text;
    }
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: xiui_capture_defaults <saved-settings.ini> <output.ini>\n";
        return 2;
    }
    try
    {
        const auto source = xiui::settings_store::FromIniText(ReadUtf8(argv[1]));
        auto output = xiui::settings_store::FromIniText("");
        std::set<std::string> sections;
        size_t count{};
        source.VisitPreferences([&](const auto section, const auto key, const auto value) {
            const std::string name{section};
            if (::_stricmp(name.c_str(), "containers") == 0 || ::_strnicmp(name.c_str(), "containers.", 11) == 0 ||
                ::_stricmp(name.c_str(), "layout.container") == 0 || ::_strnicmp(name.c_str(), "layout.container.", 17) == 0) return;
            output.WriteString(name.c_str(), std::string{key}.c_str(), std::string{value});
            sections.insert(name); ++count;
        });
        if (count == 0) throw std::runtime_error{"Saved settings contain no preferences."};
        const std::filesystem::path destination{argv[2]}, staging{destination.string() + ".capture.tmp"};
        if (!destination.parent_path().empty()) std::filesystem::create_directories(destination.parent_path());
        {
            std::ofstream file{staging, std::ios::binary | std::ios::trunc};
            file << "; Captured XIUI+ defaults. Source: " << std::filesystem::path{argv[1]}.filename().string() << "\n"
                 << "; Regenerate with scripts/capture-defaults.ps1; local device, profile, session and retired demo data excluded.\n\n"
                 << output.ToIniText();
            file.close();
            if (!file) throw std::runtime_error{"Could not write captured defaults."};
        }
        if (!::MoveFileExW(staging.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            std::filesystem::remove(staging);
            throw std::runtime_error{"Could not publish captured defaults."};
        }
        std::cout << count << " preferences in " << sections.size() << " sections captured.\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
