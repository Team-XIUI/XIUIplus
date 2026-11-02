#include "storage_paths.hpp"
#include "settings_store.hpp"
#include "modules/readycheck/ready_check_wav.hpp"

#include <set>
#include <memory>
#include <stdexcept>
#include <vector>

namespace xiui
{
    namespace
    {
        namespace fs = std::filesystem;

        struct migration_lock
        {
            HANDLE handle{};
            explicit migration_lock(const fs::path& root)
            {
                auto name = root.wstring();
                ::CharLowerBuffW(name.data(), static_cast<DWORD>(name.size()));
                uint64_t hash = 14695981039346656037ull;
                for (const auto c : name) hash = (hash ^ static_cast<uint16_t>(c)) * 1099511628211ull;
                const auto key = L"Local\\XIUI.Storage." + std::to_wstring(hash);
                this->handle = ::CreateMutexW(nullptr, FALSE, key.c_str());
                const auto status = this->handle != nullptr ? ::WaitForSingleObject(this->handle, 5000) : WAIT_FAILED;
                if (status != WAIT_OBJECT_0 && status != WAIT_ABANDONED)
                {
                    if (this->handle != nullptr) ::CloseHandle(this->handle);
                    throw std::runtime_error{"Could not lock XIUI storage migration. Reload XIUI after other clients finish loading."};
                }
            }
            ~migration_lock() { ::ReleaseMutex(this->handle); ::CloseHandle(this->handle); }
        };

        auto CheckPath(const fs::path& path, const fs::path& boundary) -> void
        {
            const auto attributes = ::GetFileAttributesW(path.c_str());
            if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
                throw std::runtime_error{"XIUI storage migration does not follow linked files or directories."};
            const auto relative = fs::weakly_canonical(path).lexically_relative(boundary);
            if (relative.empty() || relative.is_absolute() || *relative.begin() == "..")
                throw std::runtime_error{"XIUI storage migration path is outside its data directory."};
        }

        auto CheckTree(const fs::path& path, const fs::path& boundary) -> void
        {
            CheckPath(path, boundary);
            if (fs::is_directory(path))
                for (const auto& child : fs::recursive_directory_iterator{path}) CheckPath(child.path(), boundary);
        }

        auto Move(const fs::path& source, const fs::path& destination) -> void
        {
            if (!::MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH))
                throw std::runtime_error{"Could not migrate XIUI data. Close other XIUI clients and check directory permissions; migration will retry on the next load."};
        }

        auto Merge(const fs::path& source, const fs::path& destination, const fs::path& archive,
            const fs::path& sourceBoundary, const fs::path& targetBoundary) -> void
        {
            CheckPath(source, sourceBoundary);
            if (!fs::exists(source)) return;
            CheckPath(destination, targetBoundary);
            CheckTree(source, sourceBoundary);
            if (!fs::exists(destination))
            {
                fs::create_directories(destination.parent_path());
                Move(source, destination);
                return;
            }
            if (fs::is_directory(source) && fs::is_directory(destination))
            {
                std::vector<fs::path> children;
                for (const auto& child : fs::directory_iterator{source}) children.push_back(child.path());
                for (const auto& child : children)
                    Merge(child, destination / child.filename(), archive / child.filename(), sourceBoundary, targetBoundary);
                fs::remove(source);
                return;
            }
            auto preserved = archive;
            for (size_t suffix = 1; fs::exists(preserved); ++suffix) { preserved = archive; preserved += L"." + std::to_wstring(suffix); }
            CheckPath(preserved, targetBoundary);
            fs::create_directories(preserved.parent_path());
            Move(source, preserved);
        }

        auto ImportedSound(const std::wstring& name) -> bool
        {
            if (name.size() != 32 || name.substr(0, 4) != L"lua-" || name.substr(28) != L".wav") return false;
            return name.substr(4, 24).find_first_not_of(L"0123456789abcdef") == std::wstring::npos;
        }
    }

    auto PrepareStorage(const std::filesystem::path& install) -> storage_paths
    {
        const auto base = fs::weakly_canonical(fs::absolute(install));
        const auto config = base / "config";
        fs::create_directories(config);
        const auto configBoundary = fs::weakly_canonical(config);
        storage_paths paths{config / "xiuinative"};
        CheckPath(paths.root, configBoundary);
        const migration_lock lock{fs::weakly_canonical(paths.root)};
        fs::create_directories(paths.root);
        const auto boundary = fs::weakly_canonical(paths.root);
        const auto legacy = paths.root / "legacy";
        const auto marker = paths.root / "storage.version";
        CheckPath(marker, boundary);
        const bool migrateSounds = !fs::exists(marker) || fs::exists(config / "xiuinative.ini") || fs::exists(config / "xiuinative-profiles");
        for (const auto& [name, destination] : std::vector<std::pair<fs::path, fs::path>>{
            {"xiuinative.ini", paths.Settings()}, {"xiuinative-theme.ini", paths.Theme()},
            {"xiuinative-profiles", paths.Profiles()}, {"xiuinative-inventories", paths.Inventories()},
            {"xiuinative-licenses", paths.Licenses()}})
        {
            const bool file = name.extension() == ".ini";
            for (const auto& path : {config / name, destination})
                if (fs::exists(path) && (file ? !fs::is_regular_file(path) : !fs::is_directory(path)))
                    throw std::runtime_error{"An XIUI storage path has an unexpected file type. Existing data is unchanged at that path."};
            Merge(config / name, destination, legacy / name, configBoundary, boundary);
        }
        Merge(base / "plugins/xiuinative-licenses", paths.Licenses(), legacy / "plugin-licenses", base, boundary);
        Merge(base / "logs/xiui", paths.Diagnostics(), legacy / "diagnostics", base, boundary);

        const auto sharedSounds = config / "sounds";
        CheckPath(sharedSounds, configBoundary);
        if (migrateSounds && fs::is_directory(sharedSounds))
        {
            fs::create_directories(paths.Sounds());
            std::vector<fs::path> owned;
            for (const auto& file : fs::directory_iterator{sharedSounds})
                if (ImportedSound(file.path().filename().wstring())) owned.push_back(file.path());
            for (const auto& sound : owned) Merge(sound, paths.Sounds() / sound.filename(), legacy / "sounds" / sound.filename(), configBoundary, boundary);

            std::set<std::string> referenced;
            const auto soundFrom = [&](const fs::path& path) {
                const auto name = settings_store::CachedFile(path.string()).ReadString("readycheck", "sound_file");
                if (ReadyCheckWavFilename(name)) referenced.insert(name);
            };
            if (fs::is_regular_file(paths.Settings())) soundFrom(paths.Settings());
            if (fs::is_directory(paths.Profiles()))
            {
                CheckTree(paths.Profiles(), boundary);
                for (const auto& profile : fs::recursive_directory_iterator{paths.Profiles()})
                    if (profile.is_regular_file() && profile.path().extension() == ".ini") { CheckPath(profile.path(), boundary); soundFrom(profile.path()); }
            }
            for (const auto& name : referenced)
            {
                const auto source = sharedSounds / name, target = paths.Sounds() / name;
                CheckPath(source, configBoundary); CheckPath(target, boundary);
                if (fs::is_regular_file(source) && !fs::exists(target) && !::CopyFileW(source.c_str(), target.c_str(), TRUE))
                    throw std::runtime_error{"Could not migrate a custom XIUI sound. The source file is unchanged."};
            }
            if (fs::is_empty(sharedSounds)) fs::remove(sharedSounds);
        }
        for (const auto& directory : {paths.Profiles(), paths.Inventories(), paths.Sounds()})
        { CheckPath(directory, boundary); fs::create_directories(directory); }
        if (!fs::exists(marker))
        {
            const auto file = ::CreateFileW(marker.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == INVALID_HANDLE_VALUE) throw std::runtime_error{"Could not record the XIUI storage migration."};
            const std::unique_ptr<void, decltype(&::CloseHandle)> closeFile{file, &::CloseHandle};
            DWORD written{};
            if (!::WriteFile(file, "1\r\n", 3, &written, nullptr) || written != 3 || !::FlushFileBuffers(file))
                throw std::runtime_error{"Could not record the XIUI storage migration."};
        }
        return paths;
    }

    auto AshitaConfigDirectory(const std::filesystem::path& settings) -> std::filesystem::path
    {
        const auto parent = settings.parent_path();
        return parent.filename() == "xiuinative" && settings.filename() == "settings.ini" ? parent.parent_path() : parent;
    }
}
