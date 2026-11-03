#include <cstdint>
#include <Windows.h>
#include <new>

namespace
{
    // Stable IAshitaCore prefix; managers are absent in the isolated startup check.
    struct test_core
    {
        HMODULE handle;
        const char* path;
        test_core(HMODULE hint, const char* root) : handle(hint), path(root) {}
        virtual auto GetHandle(void) const -> HMODULE { return this->handle; }
        virtual auto GetInstallPath(void) const -> const char* { return this->path; }
        virtual auto GetDirect3DDevice(void) const -> void* { return nullptr; }
        virtual auto GetProperties(void) const -> void* { return nullptr; }
        virtual auto GetChatManager(void) const -> void* { return nullptr; }
        virtual auto GetConfigurationManager(void) const -> void* { return nullptr; }
        virtual auto GetFontManager(void) const -> void* { return nullptr; }
        virtual auto GetGuiManager(void) const -> void* { return nullptr; }
        virtual auto GetInputManager(void) const -> void* { return nullptr; }
        virtual auto GetMemoryManager(void) const -> void* { return nullptr; }
        virtual auto GetOffsetManager(void) const -> void* { return nullptr; }
        virtual auto GetPacketManager(void) const -> void* { return nullptr; }
        virtual auto GetPluginManager(void) const -> void* { return nullptr; }
        virtual auto GetPolPluginManager(void) const -> void* { return nullptr; }
        virtual auto GetPointerManager(void) const -> void* { return nullptr; }
        virtual auto GetPrimitiveManager(void) const -> void* { return nullptr; }
        virtual auto GetResourceManager(void) const -> void* { return nullptr; }
    };
}

#if !defined(XIUI_TEST_ASHITA_FACADE)
extern "C" auto __stdcall GetInterfaceVersion(void) -> double { return XIUI_TEST_ASHITA_INTERFACE; }
extern "C" auto __stdcall GetAshitaVersion(void) -> uint64_t { return 4ull | (2ull << 16) | (3ull << 48); }
#endif
extern "C" auto __stdcall GetTestCore(HMODULE hint, const char* root) -> void*
{
    alignas(test_core) static unsigned char storage[sizeof(test_core)]{};
    return new (storage) test_core{hint, root};
}
