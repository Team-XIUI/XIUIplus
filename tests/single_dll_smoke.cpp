#include "Ashita.h"

#include <cstdio>
#include <filesystem>
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
        const auto installed = root / "plugins" / "xiuinative.dll";
        std::filesystem::copy_file(source, installed);
        std::filesystem::current_path(root);
        Check(!std::filesystem::exists(root / "plugins" / "xiuinative-runtime.dll"), "isolated install must contain only one DLL");
        state.module = ::LoadLibraryExW(installed.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        Check(state.module != nullptr, "load standalone plugin");
        const auto create = reinterpret_cast<IPlugin*(__stdcall*)(const char*)>(::GetProcAddress(state.module, "expCreatePlugin"));
        state.destroy = reinterpret_cast<void(__stdcall*)(void*)>(::GetProcAddress(state.module, "expDestroyPlugin"));
        Check(create != nullptr && state.destroy != nullptr && ::GetProcAddress(state.module, "expGetInterfaceVersion") != nullptr, "standard Ashita exports");
        for (const int id : {101, 201, 202, 203, 204, 301, 302, 303, 304})
            Check(::FindResourceW(state.module, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10)) != nullptr, "embedded assets, defaults, theme and notices");
        state.plugin = create("");
        Check(state.plugin != nullptr, "create Ashita plugin");
        state.initialized = state.plugin->Initialize(nullptr, nullptr, 0);
        Check(state.initialized, "initialize embedded runtime without a companion DLL");
        Check(state.plugin->HandleCommand(0, "/xiui save", true), "standalone command routing");
        state.plugin->Release();
        state.initialized = false;
        Check(std::filesystem::file_size(root / "config" / "xiuinative.ini") > 1000, "save complete embedded profile");
        Check(std::filesystem::file_size(root / "config" / "xiuinative-theme.ini") > 100, "seed embedded theme");
        Check(std::filesystem::exists(root / "config" / "xiuinative-licenses" / "MinHook.txt"), "retain embedded notices");
        Check(std::filesystem::exists(root / "config" / "xiuinative-licenses" / "libwebp.txt") &&
            std::filesystem::exists(root / "config" / "xiuinative-licenses" / "libwebp-PATENTS.txt") &&
            std::filesystem::exists(root / "config" / "xiuinative-licenses" / "miniz.txt"), "retain bundled decoder notices");
        Check(std::filesystem::exists(root / "config" / "xiuinative-licenses" / "XIUI-Notification-Icons.txt"), "retain notification artwork notice");
        std::puts("Single DLL Ashita exports, isolated startup, assets, theme, defaults and saving passed.");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "single DLL check failed: %s\n", error.what());
        return 1;
    }
}
