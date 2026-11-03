#include "ashita_compat.hpp"
#include "mouse_input.hpp"

#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace
{
    auto Check(const bool condition, const char* message) -> void
    { if (!condition) throw std::runtime_error{message}; }

    auto MapHostImage(const std::filesystem::path& path) -> HMODULE
    {
        std::ifstream input{path, std::ios::binary};
        const std::vector<char> file{std::istreambuf_iterator<char>{input}, {}};
        Check(file.size() >= sizeof(IMAGE_DOS_HEADER), "read host image fixture");
        const auto& dos = *reinterpret_cast<const IMAGE_DOS_HEADER*>(file.data());
        Check(dos.e_magic == IMAGE_DOS_SIGNATURE && dos.e_lfanew > 0 && static_cast<size_t>(dos.e_lfanew) + sizeof(IMAGE_NT_HEADERS32) <= file.size(), "fixture has bounded PE headers");
        const auto& nt = *reinterpret_cast<const IMAGE_NT_HEADERS32*>(file.data() + dos.e_lfanew);
        Check(nt.Signature == IMAGE_NT_SIGNATURE && nt.OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC && nt.OptionalHeader.SizeOfHeaders <= file.size(), "fixture is an x86 image");
        auto* image = static_cast<char*>(::VirtualAlloc(nullptr, nt.OptionalHeader.SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
        Check(image != nullptr, "allocate host outside the Windows loader");
        struct cleanup { void* address; ~cleanup() { if (address) ::VirtualFree(address, 0, MEM_RELEASE); } } release{image};
        std::memcpy(image, file.data(), nt.OptionalHeader.SizeOfHeaders);
        const auto* sections = IMAGE_FIRST_SECTION(&nt);
        Check(reinterpret_cast<const char*>(sections + nt.FileHeader.NumberOfSections) <= file.data() + file.size(), "fixture section table is bounded");
        for (WORD index = 0; index < nt.FileHeader.NumberOfSections; ++index)
        {
            const auto& section = sections[index];
            Check(size_t{section.PointerToRawData} + section.SizeOfRawData <= file.size() &&
                size_t{section.VirtualAddress} + section.SizeOfRawData <= nt.OptionalHeader.SizeOfImage, "fixture section bytes are bounded");
            std::memcpy(image + section.VirtualAddress, file.data() + section.PointerToRawData, section.SizeOfRawData);
        }
        const auto delta = reinterpret_cast<uintptr_t>(image) - nt.OptionalHeader.ImageBase;
        const auto relocation = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
        Check(relocation.Size != 0 && size_t{relocation.VirtualAddress} + relocation.Size <= nt.OptionalHeader.SizeOfImage, "fixture has bounded relocations");
        for (DWORD at = 0; at < relocation.Size;)
        {
            const auto* block = reinterpret_cast<const IMAGE_BASE_RELOCATION*>(image + relocation.VirtualAddress + at);
            Check(relocation.Size - at >= sizeof(*block) && block->SizeOfBlock >= sizeof(*block) && block->SizeOfBlock <= relocation.Size - at, "fixture relocation block is bounded");
            const auto* entries = reinterpret_cast<const WORD*>(block + 1);
            for (DWORD index = 0; index < (block->SizeOfBlock - sizeof(*block)) / sizeof(WORD); ++index)
            {
                const auto type = entries[index] >> 12;
                if (type == IMAGE_REL_BASED_ABSOLUTE) continue;
                const auto rva = block->VirtualAddress + (entries[index] & 0xFFF);
                Check(type == IMAGE_REL_BASED_HIGHLOW && size_t{rva} + sizeof(DWORD) <= nt.OptionalHeader.SizeOfImage, "fixture uses bounded x86 relocations");
                *reinterpret_cast<DWORD*>(image + rva) += delta;
            }
            at += block->SizeOfBlock;
        }
        ::FlushInstructionCache(::GetCurrentProcess(), image, nt.OptionalHeader.SizeOfImage);
        release.address = nullptr;
        return reinterpret_cast<HMODULE>(image);
    }

    struct native_fixture
    {
        void** vtable{};
        mutable uint32_t value{1000};
        std::array<void*, 280> slots{};
        auto Number(void) const -> uint32_t { return this->value; }
        auto Indexed(uint32_t index) const -> uint32_t { return this->value + index; }
        auto Index(uint32_t index) const -> uint16_t { return static_cast<uint16_t>(this->value + index); }
        auto Byte(uint32_t index) const -> uint8_t { return static_cast<uint8_t>(index + 1); }
        auto Flag(void) const -> uint8_t { return 1; }
        auto Scalar(uint32_t index) const -> float { return static_cast<float>(this->value + index) + 0.25f; }
        auto Skill(uint32_t index) const -> Ashita::FFXI::combatskill_t { return {static_cast<uint16_t>(this->value + index)}; }
        auto Target(uint32_t index, bool force) const -> void { this->value = index + (force ? 100 : 0); }
        auto Type(uint32_t index, uint8_t type) const -> void { this->value = index + type; }
        template<typename Method> auto Bind(size_t slot, Method method) -> void
        {
            static_assert(sizeof(method) == sizeof(void*));
            this->slots.fill(nullptr);
            std::memcpy(&this->slots[slot], &method, sizeof(method));
            this->vtable = this->slots.data();
        }
    };

    auto CheckInterfaces(void) -> void
    {
        for (const bool legacy : {true, false})
        {
            native_fixture native;
            const xiui::ashita::party_view party{reinterpret_cast<const IParty*>(&native), legacy};
            native.Bind(legacy ? 18 : 19, &native_fixture::Indexed);
            Check(party->GetMemberHP(7) == 1007, "party getter follows the selected SDK layout");
            native.Bind(legacy ? 38 : 41, &native_fixture::Byte);
            Check(party->GetMemberIsActive(17) == 18, "party getters after the master-level additions retain their slot");
            native.Bind(36, &native_fixture::Byte);
            Check(party->GetMemberMasterLevel(9) == (legacy ? 0 : 10), "legacy party master level is unavailable rather than another virtual method");

            const xiui::ashita::player_view player{reinterpret_cast<const IPlayer*>(&native), legacy};
            native.Bind(legacy ? 34 : 33, &native_fixture::Number);
            Check(player->GetMasteryExp() == 1000, "mastery data accounts for the removed player method");
            native.Bind(legacy ? 36 : 35, &native_fixture::Skill);
            Check(player->GetCombatSkill(33).GetSkill() == 1033, "small struct returns preserve the x86 member-call ABI");
            native.Bind(legacy ? 53 : 52, &native_fixture::Number);
            Check(player->GetIsZoning() == 1000, "zoning reads the correct player method");
            native.Bind(77, &native_fixture::Number);
            Check(player->GetPetTP() == 1000, "player slots realign after the new submap method");

            const xiui::ashita::entity_view entity{reinterpret_cast<const IEntity*>(&native), legacy};
            native.Bind(59, &native_fixture::Indexed);
            Check(entity->GetRenderFlags1(3) == 1003, "seeking-party name flags retain their entity getter in both SDK layouts");
            native.Bind(legacy ? 114 : 115, &native_fixture::Index);
            Check(entity->GetPetTargetIndex(3) == 1003, "pet targeting accounts for the inserted entity method");
            native.Bind(legacy ? 120 : 121, &native_fixture::Scalar);
            Check(entity->GetModelSize(4) == 1004.25f, "entity float returns retain their ABI");
            native.Bind(legacy ? 174 : 175, &native_fixture::Type);
            entity->SetType(90, 2);
            Check(native.value == 92, "entity writes use the legacy setter slot");

            const xiui::ashita::target_view target{reinterpret_cast<const ITarget*>(&native), legacy};
            native.Bind(legacy ? 26 : 27, &native_fixture::Flag);
            Check(target->GetActionTargetActive() == 1, "native target ownership uses the selected SDK layout");
            native.Bind(legacy ? 41 : 42, &native_fixture::Number);
            Check(target->GetActionCallback() == 92, "native target callbacks retain their method position");
            native.Bind(legacy ? 69 : 70, &native_fixture::Target);
            target->SetTarget(200, true);
            Check(native.value == 300, "target selection preserves its arguments and calling convention");
        }
        Check(!xiui::ashita::View(static_cast<const IParty*>(nullptr)), "compatibility views preserve null availability");
    }

    auto CheckMouse(void) -> void
    {
        xiui::mouse_input mouse;
        Check(mouse.Normalize(WM_MOUSEMOVE, 0, MK_LBUTTON, 0) == 0, "polling a held button never creates a press");
        Check(mouse.Normalize(WM_LBUTTONDOWN, 0, 0, 0) == MK_LBUTTON, "zero-data callbacks still report a fresh left press");
        Check(mouse.Normalize(WM_MOUSEMOVE, 0, MK_LBUTTON, 0) == MK_LBUTTON, "zero-data movement retains a real held drag without Shift");
        Check(mouse.Normalize(WM_LBUTTONUP, 0, MK_LBUTTON, 0) == 0, "the release message wins over a stale physical snapshot");
        Check(mouse.Normalize(WM_LBUTTONDBLCLK, 0, 0, MK_SHIFT) == (MK_LBUTTON | MK_SHIFT), "double clicks and physical Shift remain separate");
        Check(mouse.Normalize(WM_MOUSEMOVE, 0, MK_LBUTTON, 0) == MK_LBUTTON, "releasing Shift does not release the pointer gesture");
        Check(mouse.Normalize(WM_MOUSEMOVE, 0, 0, 0) == 0, "a confirmed lost release can cancel the gesture");
        Check(mouse.Normalize(WM_RBUTTONDOWN, 0, 0, 0) == MK_RBUTTON, "camera button presses are tracked independently");
        const auto wheel = MAKEWPARAM(0, static_cast<WORD>(-WHEEL_DELTA));
        Check(mouse.Normalize(WM_MOUSEWHEEL, wheel, MK_RBUTTON, MK_CONTROL) == (wheel | MK_RBUTTON | MK_CONTROL), "wheel sign and held buttons survive normalization");
        Check(mouse.Normalize(WM_XBUTTONDOWN, MAKEWPARAM(0, XBUTTON2), MK_RBUTTON, 0) == (MAKEWPARAM(0, XBUTTON2) | MK_RBUTTON | MK_XBUTTON2), "X button identity stays in the high word");
        mouse.Reset();
        Check(mouse.Normalize(WM_MOUSEMOVE, 0, MK_RBUTTON | MK_XBUTTON2, 0) == 0, "suspension drains button history without adopting held input");
        xiui::mouse_button_trace trace;
        trace.Record(WM_LBUTTONDOWN, MK_LBUTTON, MK_LBUTTON, 10, 20, false, false);
        trace.Record(WM_LBUTTONUP, 0, 0, 10, 20, false, true);
        for (size_t index = 0; index < 20; ++index)
            trace.Record(WM_LBUTTONDOWN, MK_LBUTTON | MK_SHIFT, MK_LBUTTON | MK_SHIFT, 30, 40, false, false);
        Check(trace.counts[0] == 2 && trace.counts[1] == 20 && trace.events[0][0].sequence == 1 && trace.events[0][1].sequence == 2,
            "later Shift clicks cannot erase the plain click press/release evidence");
        trace.Clear();
        Check(trace.total == 0 && trace.counts[0] == 0 && trace.counts[1] == 0, "mouse trace can start a fresh comparison");
    }

    auto CheckPlugin(const char* path, const HMODULE host, const bool initialize) -> void
    {
        const auto module = ::LoadLibraryA(path);
        Check(module != nullptr, "load standalone plugin against an isolated host fixture");
        struct cleanup
        {
            HMODULE module;
            IPlugin* plugin{};
            void(__stdcall* destroy)(void*){};
            bool initialized{};
            std::filesystem::path root;
            std::filesystem::path previous{std::filesystem::current_path()};
            ~cleanup()
            {
                if (this->initialized) this->plugin->Release();
                if (this->plugin && this->destroy) this->destroy(this->plugin);
                ::FreeLibrary(this->module);
                std::error_code ignored;
                std::filesystem::current_path(this->previous, ignored);
                if (!this->root.empty() && this->root.parent_path() == std::filesystem::temp_directory_path()) std::filesystem::remove_all(this->root, ignored);
            }
        } release{module};
        const auto version = reinterpret_cast<double(__stdcall*)(void)>(::GetProcAddress(module, "expGetInterfaceVersion"));
        const auto create = reinterpret_cast<IPlugin*(__stdcall*)(const char*)>(::GetProcAddress(module, "expCreatePlugin"));
        const auto destroy = reinterpret_cast<void(__stdcall*)(void*)>(::GetProcAddress(module, "expDestroyPlugin"));
        Check(version && create && destroy, "standard plugin exports remain available");
        Check(version() == xiui::ashita::SelectedInterfaceVersion(), "plugin loader receives the selected supported interface");
        release.destroy = destroy;
        const auto plugin = release.plugin = create("");
        Check(plugin != nullptr, "create compatible plugin metadata");
        const auto objectVersion = plugin->GetInterfaceVersion();
        Check(objectVersion == version(), "plugin metadata agrees with the loader export");
        if (initialize)
        {
            const auto root = std::filesystem::temp_directory_path() /
                ("XIUIplus-host-startup-" + std::to_string(::GetCurrentProcessId()) + "-" + std::to_string(::GetTickCount64()));
            Check(std::filesystem::create_directory(root), "create an isolated startup directory");
            release.root = root;
            std::filesystem::current_path(root);
            const auto make = reinterpret_cast<void*(__stdcall*)(HMODULE, const char*)>(xiui::ashita::detail::HostExport(host, "GetTestCore", "_GetTestCore@8"));
            Check(make != nullptr, "fixture exposes a real core vtable from its own image");
            const auto install = root.string();
            const auto core = reinterpret_cast<IAshitaCore*>(make(::GetModuleHandleW(nullptr), install.c_str()));
            Check(core->GetHandle() != host, "startup reproduces a handle that does not identify the core DLL");
            release.initialized = plugin->Initialize(core, nullptr, 0);
            Check(release.initialized, "the actual standalone plugin initializes when GetHandle is not the core DLL");
            plugin->Release(); release.initialized = false;
        }
    }
}

int main(int argc, char** argv)
{
    struct fixture_lifetime
    {
        HMODULE host{}, plugin{};
        bool mapped{};
        std::filesystem::path alias;
        ~fixture_lifetime()
        {
            if (this->plugin) ::FreeLibrary(this->plugin);
            if (this->host) { if (this->mapped) ::VirtualFree(this->host, 0, MEM_RELEASE); else ::FreeLibrary(this->host); }
            if (!this->alias.empty()) { std::error_code ignored; std::filesystem::remove(this->alias, ignored); }
        }
    } fixture;
    try
    {
        Check(argc == 1 || argc == 3 || argc == 4, "optional arguments: host DLL, plugin DLL, --renamed / --mapped / --mapped-real / --core-handle / --facade");
        const bool renamed = argc == 4 && std::strcmp(argv[3], "--renamed") == 0;
        const bool real = argc == 4 && std::strcmp(argv[3], "--mapped-real") == 0;
        const bool mapped = real || (argc == 4 && std::strcmp(argv[3], "--mapped") == 0);
        const bool handle = argc == 4 && std::strcmp(argv[3], "--core-handle") == 0;
        const bool facade = argc == 4 && std::strcmp(argv[3], "--facade") == 0;
        Check(argc != 4 || renamed || mapped || handle || facade, "unknown compatibility mode");
        if (argc >= 3)
        {
            Check(xiui::ashita::HostInterfaceVersion() == 0, "no host is loaded before the late-discovery regression check");
            if (!handle)
            {
                fixture.plugin = ::LoadLibraryA(argv[2]);
                Check(fixture.plugin != nullptr, "load plugin before its host is discoverable");
                const auto probe = reinterpret_cast<double(__stdcall*)(void)>(::GetProcAddress(fixture.plugin, "expGetInterfaceVersion"));
                Check(probe != nullptr && probe() == ASHITA_INTERFACE_VERSION, "isolated loader probe preserves the build SDK fallback");
            }
            auto hostPath = std::filesystem::absolute(argv[1]);
            if (renamed)
            {
                const auto alias = std::filesystem::temp_directory_path() /
                    ("XIUIplus-renamed-core-" + std::to_string(::GetCurrentProcessId()) + "-" + std::to_string(::GetTickCount64()) + ".dll");
                std::filesystem::copy_file(hostPath, alias);
                fixture.alias = alias; hostPath = alias;
            }
            fixture.host = mapped ? MapHostImage(hostPath) : ::LoadLibraryW(hostPath.c_str());
            fixture.mapped = mapped;
            Check(fixture.host != nullptr, "load isolated host fixture");
            if (renamed || mapped) Check(::GetModuleHandleW(L"Ashita.dll") == nullptr, "core has no registered Ashita.dll basename");
            if (mapped)
            {
                HMODULE registered{};
                Check(!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCWSTR>(fixture.host), &registered), "mapped core is absent from the Windows loader list");
            }
            if (!real)
            {
                const auto make = reinterpret_cast<void*(__stdcall*)(HMODULE, const char*)>(xiui::ashita::detail::HostExport(fixture.host, "GetTestCore", "_GetTestCore@8"));
                Check(make != nullptr, "fixture core factory is discoverable");
                const auto core = reinterpret_cast<const IAshitaCore*>(make(nullptr, "."));
                Check(xiui::ashita::InitializeHost(core) && xiui::ashita::HostModule() == fixture.host,
                    "the core vtable identifies its image even when GetHandle is null");
                if (facade)
                {
                    Check(xiui::ashita::HostInterfaceVersion() == 0 && xiui::ashita::SelectedInterfaceVersion() == ASHITA_INTERFACE_VERSION,
                        "a facade uses the accepted build SDK without inventing an Ashita core version");
                    Check(xiui::ashita::InitializeHost(core, 4.16) && xiui::ashita::LegacyInterface(),
                        "an explicitly negotiated legacy interface selects legacy views without core exports");
                    Check(xiui::ashita::InitializeHost(core, 4.30) && !xiui::ashita::LegacyInterface() &&
                        !xiui::ashita::InitializeHost(core, 4.20) && xiui::ashita::SelectedInterfaceVersion() == 4.30,
                        "modern negotiation remains authoritative and unsupported requests cannot replace it");
                }
            }
            Check(xiui::ashita::HostModule() == fixture.host && (facade || xiui::ashita::HostInterfaceVersion() != 0), "core discovery recovers after an initial miss");
            if (real) std::printf("Mapped core interface: %.2f.\n", xiui::ashita::HostInterfaceVersion());
        }
        CheckInterfaces(); CheckMouse();
        if (argc >= 3) CheckPlugin(argv[2], fixture.host, !real);
        std::puts(real ? "Real core image exports and standalone interface negotiation passed; core startup was not invoked." :
            argc >= 3 ? "Ashita SDK dispatch, core-image discovery, standalone startup and mouse trace checks passed." :
            "Ashita 4.16/4.30 SDK dispatch and mouse normalization passed.");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Ashita compatibility check failed: %s\n", error.what());
        return 1;
    }
}
