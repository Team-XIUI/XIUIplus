#include "Ashita.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace
{
    auto Check(const bool condition, const char* message) -> void
    {
        if (!condition) throw std::runtime_error{message};
    }
}

int main(const int argc, char** argv)
{
    try
    {
        Check(argc == 2, "expected the standalone plugin DLL path");
        const auto source = std::filesystem::absolute(argv[1]);
        const auto root = std::filesystem::temp_directory_path() / ("xiui-single-dll-" + std::to_string(::GetCurrentProcessId()));
        std::filesystem::create_directories(root / "plugins");
        struct cleanup
        {
            std::filesystem::path root, previous = std::filesystem::current_path();
            HMODULE module{};
            IPlugin* plugin{};
            void(__stdcall* destroy)(void*){};
            bool initialized{};
            ~cleanup()
            {
                if (initialized) plugin->Release();
                if (plugin != nullptr && destroy != nullptr) destroy(plugin);
                if (module != nullptr) ::FreeLibrary(module);
                std::error_code ignored;
                std::filesystem::current_path(previous, ignored);
                std::filesystem::remove_all(root, ignored);
            }
        } state{root};
        const auto installed = root / "plugins" / "XIUIplus.dll";
        std::filesystem::copy_file(source, installed);
        std::filesystem::current_path(root);
        Check(!std::filesystem::exists(root / "plugins" / "XIUIplus-runtime.dll"), "isolated install must contain only one DLL");
        state.module = ::LoadLibraryExW(installed.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        Check(state.module != nullptr, "load standalone plugin");
        const auto create = reinterpret_cast<IPlugin*(__stdcall*)(const char*)>(::GetProcAddress(state.module, "expCreatePlugin"));
        state.destroy = reinterpret_cast<void(__stdcall*)(void*)>(::GetProcAddress(state.module, "expDestroyPlugin"));
        Check(create != nullptr && state.destroy != nullptr && ::GetProcAddress(state.module, "expGetInterfaceVersion") != nullptr, "standard Ashita exports");
        for (const int id : {101, 201, 202, 203, 204, 301, 302, 303, 304})
            Check(::FindResourceW(state.module, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10)) != nullptr, "embedded assets, defaults, theme and notices");
        const auto oldNotices = root / "config/xiuinative-licenses";
        std::filesystem::create_directories(oldNotices);
        { std::ofstream file{oldNotices / "MinHook.txt", std::ios::binary}; file << "Generated notice from an earlier build"; }
        state.plugin = create("");
        Check(state.plugin != nullptr, "create Ashita plugin");
        state.initialized = state.plugin->Initialize(nullptr, nullptr, 0);
        Check(state.initialized, "initialize embedded runtime without a companion DLL");
        Check(std::strcmp(state.plugin->GetName(), "XIUI+") == 0 &&
            std::strcmp(state.plugin->GetLink(), "https://github.com/Shuu-37/xiui-plugin") == 0, "current product name and project link");
        for (const auto* command : {"/xiuiplus show", "/xiuip show", "/xiui+ show", "/xiui show", "/xiuin show", "/xiuinative show", "/xiui-native show"})
            Check(state.plugin->HandleCommand(0, command, true), "current commands and previous aliases remain available");
        Check(state.plugin->HandleCommand(0, "/xiuiplus save", true), "standalone command routing");
        state.plugin->Release();
        state.initialized = false;
        Check(std::filesystem::file_size(root / "config" / "XIUIplus" / "settings.ini") > 1000, "save complete embedded profile");
        Check(std::filesystem::file_size(root / "config" / "XIUIplus" / "theme.ini") > 100, "seed embedded theme");
        Check(!std::filesystem::exists(root / "config" / "XIUIplus" / "licenses") && !std::filesystem::exists(oldNotices),
            "retire extracted notices while keeping their DLL resources embedded");
        std::puts("Single DLL Ashita exports, isolated startup, assets, theme, defaults and saving passed.");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "single DLL check failed: %s\n", error.what());
        return 1;
    }
}
