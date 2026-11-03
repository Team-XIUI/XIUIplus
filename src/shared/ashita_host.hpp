#pragma once

#include "Ashita.h"
#include <TlHelp32.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <iterator>
#include <mutex>
#include <string>

namespace xiui::ashita
{
    namespace detail
    {
        inline auto ReadHostMemory(const uintptr_t address, void* output, const size_t size) -> bool
        {
            SIZE_T copied{};
            return address != 0 && ::ReadProcessMemory(::GetCurrentProcess(), reinterpret_cast<const void*>(address), output, size, &copied) && copied == size;
        }

        class host_image
        {
        public:
            explicit host_image(const HMODULE module) : base_(reinterpret_cast<uintptr_t>(module))
            {
                IMAGE_DOS_HEADER dos{};
                IMAGE_NT_HEADERS32 nt{};
                if (!ReadHostMemory(this->base_, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE ||
                    dos.e_lfanew < static_cast<LONG>(sizeof(dos)) || dos.e_lfanew > 0x100000 ||
                    !ReadHostMemory(this->base_ + dos.e_lfanew, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE ||
                    nt.FileHeader.Machine != IMAGE_FILE_MACHINE_I386 || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
                    nt.OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_EXPORT) return;
                this->size_ = nt.OptionalHeader.SizeOfImage;
                this->exports_ = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
                if (this->size_ > UINTPTR_MAX - this->base_ || this->exports_.Size < sizeof(IMAGE_EXPORT_DIRECTORY) ||
                    !this->Contains(this->exports_.VirtualAddress, this->exports_.Size)) this->size_ = 0;
            }

            auto Export(const char* name) const -> FARPROC
            {
                IMAGE_EXPORT_DIRECTORY directory{};
                if (!this->Read(this->exports_.VirtualAddress, directory) || directory.NumberOfNames > 65536 || directory.NumberOfFunctions > 65536 ||
                    !this->Contains(directory.AddressOfNames, size_t{directory.NumberOfNames} * sizeof(DWORD)) ||
                    !this->Contains(directory.AddressOfNameOrdinals, size_t{directory.NumberOfNames} * sizeof(WORD)) ||
                    !this->Contains(directory.AddressOfFunctions, size_t{directory.NumberOfFunctions} * sizeof(DWORD))) return nullptr;
                const auto length = std::strlen(name) + 1;
                for (DWORD index = 0; index < directory.NumberOfNames; ++index)
                {
                    DWORD nameRva{};
                    char candidate[64]{};
                    if (length > sizeof(candidate) || !this->Read(directory.AddressOfNames + index * sizeof(DWORD), nameRva) ||
                        !this->Contains(nameRva, length) || !ReadHostMemory(this->base_ + nameRva, candidate, length) ||
                        std::memcmp(candidate, name, length) != 0) continue;
                    WORD ordinal{};
                    DWORD functionRva{};
                    if (!this->Read(directory.AddressOfNameOrdinals + index * sizeof(WORD), ordinal) || ordinal >= directory.NumberOfFunctions ||
                        !this->Read(directory.AddressOfFunctions + ordinal * sizeof(DWORD), functionRva) || !this->Contains(functionRva, 1)) return nullptr;
                    // Forwarder strings are not callable code.
                    if (functionRva >= this->exports_.VirtualAddress && functionRva - this->exports_.VirtualAddress < this->exports_.Size) return nullptr;
                    MEMORY_BASIC_INFORMATION memory{};
                    if (!::VirtualQuery(reinterpret_cast<const void*>(this->base_ + functionRva), &memory, sizeof(memory)) || memory.State != MEM_COMMIT ||
                        (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
                        !(memory.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) return nullptr;
                    return reinterpret_cast<FARPROC>(this->base_ + functionRva);
                }
                return nullptr;
            }

        private:
            uintptr_t base_{};
            DWORD size_{};
            IMAGE_DATA_DIRECTORY exports_{};
            auto Contains(const DWORD rva, const size_t size) const -> bool
            { return rva != 0 && rva < this->size_ && size <= this->size_ - rva; }
            template<typename T> auto Read(const DWORD rva, T& value) const -> bool
            { return this->Contains(rva, sizeof(value)) && ReadHostMemory(this->base_ + rva, &value, sizeof(value)); }
        };

        struct host_api
        {
            std::atomic<HMODULE> module{};
            std::atomic<double> interface_version{};
            std::atomic<double> advertised_interface{};
            std::atomic<double> selected_interface{};
            std::atomic<bool> initialized{};
            std::mutex discovery;
        };
        inline auto HostApi(void) -> host_api& { static host_api api; return api; }
        inline auto HostExport(const HMODULE module, const char* name, const char* decorated) -> FARPROC
        {
            const host_image image{module};
            if (const auto address = image.Export(name)) return address;
            return image.Export(decorated);
        }
        inline auto BindHost(const HMODULE module) -> bool
        {
            const auto get = HostExport(module, "GetInterfaceVersion", "_GetInterfaceVersion@0");
            if (get == nullptr || HostExport(module, "GetAshitaVersion", "_GetAshitaVersion@0") == nullptr) return false;
            const auto version = reinterpret_cast<double(__stdcall*)(void)>(get)();
            if (!std::isfinite(version) || version < 4 || version >= 5) return false;
            auto& api = HostApi();
            api.module.store(module, std::memory_order_release);
            api.interface_version.store(version, std::memory_order_release);
            return true;
        }
        inline auto DiscoverHost(void) -> void
        {
            if (BindHost(::GetModuleHandleW(L"Ashita.dll"))) return;
            const auto snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, ::GetCurrentProcessId());
            if (snapshot != INVALID_HANDLE_VALUE)
            {
                MODULEENTRY32W entry{}; entry.dwSize = sizeof(entry);
                bool found{};
                if (::Module32FirstW(snapshot, &entry))
                    do { if ((found = BindHost(entry.hModule))) break; } while (::Module32NextW(snapshot, &entry));
                ::CloseHandle(snapshot);
                if (found) return;
            }
            // Manually mapped cores do not appear in the Windows loader's module list.
            MEMORY_BASIC_INFORMATION memory{};
            for (uintptr_t address = 0; ::VirtualQuery(reinterpret_cast<const void*>(address), &memory, sizeof(memory));)
            {
                if (memory.State == MEM_COMMIT && memory.AllocationBase == memory.BaseAddress &&
                    !(memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) && BindHost(static_cast<HMODULE>(memory.AllocationBase))) return;
                const auto next = reinterpret_cast<uintptr_t>(memory.BaseAddress) + memory.RegionSize;
                if (next <= address) break;
                address = next;
            }
        }
    }

    inline auto ModuleFromAddress(const void* address) -> HMODULE
    {
        MEMORY_BASIC_INFORMATION memory{};
        return address != nullptr && ::VirtualQuery(address, &memory, sizeof(memory)) && memory.State == MEM_COMMIT
            ? static_cast<HMODULE>(memory.AllocationBase) : nullptr;
    }
    inline auto HostInterfaceVersion(void) -> double
    {
        auto& api = detail::HostApi();
        if (const auto version = api.interface_version.load(std::memory_order_acquire); version != 0) return version;
        if (api.initialized.load(std::memory_order_acquire)) return api.interface_version.load(std::memory_order_acquire);
        std::lock_guard lock(api.discovery);
        if (!api.initialized.load(std::memory_order_acquire) && api.interface_version.load(std::memory_order_acquire) == 0) detail::DiscoverHost();
        return api.interface_version.load(std::memory_order_acquire);
    }
    inline auto HostModule(void) -> HMODULE
    { HostInterfaceVersion(); return detail::HostApi().module.load(std::memory_order_acquire); }
    inline auto SelectedInterfaceVersion(void) -> double
    {
        auto& api = detail::HostApi();
        if (const auto selected = api.selected_interface.load(std::memory_order_acquire); selected != 0) return selected;
        if (const auto advertised = api.advertised_interface.load(std::memory_order_acquire); advertised != 0) return advertised;
        return HostInterfaceVersion() == 4.16 ? 4.16 : ASHITA_INTERFACE_VERSION;
    }
    inline auto PluginInterfaceVersion(void) -> double
    {
        auto& api = detail::HostApi();
        if (api.initialized.load(std::memory_order_acquire)) return SelectedInterfaceVersion();
        const auto advertised = HostInterfaceVersion() == 4.16 ? 4.16 : ASHITA_INTERFACE_VERSION;
        api.advertised_interface.store(advertised, std::memory_order_release);
        return advertised;
    }
    inline auto InitializeHost(const IAshitaCore* core, const double requestedInterface = 0) -> bool
    {
        auto& api = detail::HostApi();
        std::lock_guard lock(api.discovery);
        uintptr_t table{}, method{};
        HMODULE image{};
        bool identified{};
        if (core != nullptr && detail::ReadHostMemory(reinterpret_cast<uintptr_t>(core), &table, sizeof(table)))
        {
            image = ModuleFromAddress(reinterpret_cast<const void*>(table));
            identified = detail::BindHost(image);
            if (!identified && detail::ReadHostMemory(table, &method, sizeof(method)))
            {
                if (const auto owner = ModuleFromAddress(reinterpret_cast<const void*>(method))) image = owner;
                identified = detail::BindHost(image);
            }
        }
        if (!identified && core != nullptr) identified = detail::BindHost(core->GetHandle());
        if (!identified && image != nullptr)
        {
            api.module.store(image, std::memory_order_release);
            api.interface_version.store(0, std::memory_order_release);
        }
        else if (!identified && api.interface_version.load(std::memory_order_acquire) == 0) detail::DiscoverHost();
        const auto version = api.interface_version.load(std::memory_order_acquire);
        if (version != 0 && version != 4.16 && version != 4.30) return false;
        auto selected = requestedInterface != 0 ? requestedInterface : api.advertised_interface.load(std::memory_order_acquire);
        if (selected == 0) selected = version != 0 ? version : ASHITA_INTERFACE_VERSION;
        if (selected != 4.16 && selected != 4.30) return false;
        // Compatibility hosts can supply the accepted plugin ABI without Ashita version exports.
        if (api.module.load(std::memory_order_acquire) == nullptr) api.module.store(image, std::memory_order_release);
        api.selected_interface.store(selected, std::memory_order_release);
        api.initialized.store(true, std::memory_order_release);
        return true;
    }
    inline auto LegacyInterface(void) -> bool { return SelectedInterfaceVersion() == 4.16; }
    inline auto HostVersion(void) -> ashitaversion_t
    {
        const auto get = reinterpret_cast<uint64_t(__stdcall*)(void)>(detail::HostExport(HostModule(), "GetAshitaVersion", "_GetAshitaVersion@0"));
        return ashitaversion_t{get != nullptr ? get() : 0};
    }
    inline auto HostModuleName(void) -> std::string
    {
        const auto module = HostModule();
        if (module == nullptr) return {};
        wchar_t path[32768]{};
        if (::GetModuleFileNameW(module, path, static_cast<DWORD>(std::size(path))) == 0)
        {
            char address[32]{};
            std::snprintf(address, sizeof(address), "%p", static_cast<void*>(module));
            return address;
        }
        const auto slash = std::wcsrchr(path, L'\\');
        const auto name = slash != nullptr ? slash + 1 : path;
        const auto size = ::WideCharToMultiByte(CP_UTF8, 0, name, -1, nullptr, 0, nullptr, nullptr);
        if (size <= 1) return {};
        std::string result(static_cast<size_t>(size), '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, name, -1, result.data(), size, nullptr, nullptr);
        result.pop_back();
        return result;
    }
}
