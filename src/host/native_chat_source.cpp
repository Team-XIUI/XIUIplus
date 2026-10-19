#include "native_chat_source.hpp"
#include "runtime_host.hpp"
#include "callback_gate.hpp"
#include "Memory.h"

#include <MinHook.h>
#include <cstring>

namespace
{
    struct native_metadata { uint32_t mode, color, serial; uint8_t remaining[48]; };
    static_assert(sizeof(native_metadata) == 0x3C);
    using add_window_fn = int32_t(__thiscall*)(void*, void*, const char*, const native_metadata*, bool, bool);
    using filter_fn = bool(__thiscall*)(void*, native_metadata, uint32_t);
    add_window_fn add_window{};
    filter_fn filter{};
    xiui::callback_gate<xiui::native_chat_source> capture_gate;
    struct capture_context { capture_context* previous{}; uint32_t windows{}; };
    thread_local capture_context* current_capture{};

    template<typename T>
    auto Read(const uintptr_t address, T& value) noexcept -> bool
    {
        if (address == 0) return false;
        __try { std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value)); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    auto ReadMessage(const char* text, char* output, const size_t capacity) noexcept -> bool
    {
        if (text == nullptr) return false;
        __try
        {
            size_t i{};
            for (; i + 1 < capacity && text[i] != '\0'; ++i) output[i] = text[i];
            output[i] = '\0';
            return i != 0;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    auto __fastcall Filter(void* self, void*, native_metadata metadata, const uint32_t route) -> bool
    {
        auto lease = capture_gate.Enter();
        const bool admitted = filter(self, metadata, route);
        if (admitted && current_capture != nullptr)
            current_capture->windows |= route == 0 ? 1u : route == 8 ? 2u : 0u;
        return admitted;
    }

    auto __fastcall AddWindow(void* self, void*, void* window, const char* text, const native_metadata* metadata, const bool indent, const bool lastFlag) -> int32_t
    {
        auto lease = capture_gate.Enter();
        xiui::chat_event event{};
        const bool hasText = lease && ReadMessage(text, event.text, sizeof(event.text));
        capture_context context{current_capture};
        current_capture = &context;
        const auto result = add_window(self, window, text, metadata, indent, lastFlag);
        current_capture = context.previous;
        if (lease && context.windows != 0)
        {
            native_metadata info{};
            if (hasText && Read(reinterpret_cast<uintptr_t>(metadata), info))
            {
                event.timestamp = ::GetTickCount64();
                event.sequence = info.serial;
                event.mode = info.mode & 0xFF;
                event.color = xiui::ChatColor(info.color);
                event.windows = context.windows;
                event.indent = indent;
                try { lease->Deliver(event); } catch (...) {}
            }
        }
        return result;
    }
}

namespace xiui
{
    native_chat_source::~native_chat_source(void) { this->Shutdown(); }

    auto native_chat_source::Find(const char* pattern) const -> uintptr_t
    {
        const auto first = Ashita::Memory::FindPattern(this->module_, this->module_size_, pattern, 0, 0);
        return first != 0 && Ashita::Memory::FindPattern(this->module_, this->module_size_, pattern, 0, 1) == 0 ? first : 0;
    }

    auto native_chat_source::Initialize(runtime_host& runtime) -> bool
    {
        this->Shutdown();
        this->module_ = reinterpret_cast<uintptr_t>(::GetModuleHandleA("FFXiMain.dll"));
        IMAGE_DOS_HEADER dos{};
        IMAGE_NT_HEADERS nt{};
        if (!Read(this->module_, dos) || dos.e_magic != IMAGE_DOS_SIGNATURE ||
            !Read(this->module_ + dos.e_lfanew, nt) || nt.Signature != IMAGE_NT_SIGNATURE) return false;
        this->module_size_ = nt.OptionalHeader.SizeOfImage;
        this->window_hook_ = this->Find("81EC6C0600005355568BD98B8C247C06000057C744242400000000C644241A1EC644241B01");
        this->filter_hook_ = this->Find("8BC18B4C2440565785C98D74240C743883F908741951B90F00000083EC3C8BFCF3A58BC8");
        this->palette_.Initialize(this->module_, this->module_size_);
        if (this->window_hook_ == 0 || this->filter_hook_ == 0) return false;
        const auto initialized = ::MH_Initialize();
        if (initialized != MH_OK && initialized != MH_ERROR_ALREADY_INITIALIZED) return false;
        this->minhook_owned_ = initialized == MH_OK;
        this->runtime_ = &runtime;
        if (::MH_CreateHook(reinterpret_cast<void*>(this->window_hook_), &AddWindow, reinterpret_cast<void**>(&add_window)) != MH_OK ||
            ::MH_CreateHook(reinterpret_cast<void*>(this->filter_hook_), &Filter, reinterpret_cast<void**>(&filter)) != MH_OK)
        { this->Shutdown(); return false; }
        capture_gate.Set(this);
        if (::MH_QueueEnableHook(reinterpret_cast<void*>(this->window_hook_)) != MH_OK ||
            ::MH_QueueEnableHook(reinterpret_cast<void*>(this->filter_hook_)) != MH_OK || ::MH_ApplyQueued() != MH_OK)
        { this->Shutdown(); return false; }
        this->enabled_ = true;
        return true;
    }

    auto native_chat_source::Shutdown(void) -> void
    {
        capture_gate.Close();
        if (this->window_hook_ != 0) ::MH_QueueDisableHook(reinterpret_cast<void*>(this->window_hook_));
        if (this->filter_hook_ != 0) ::MH_QueueDisableHook(reinterpret_cast<void*>(this->filter_hook_));
        if (this->window_hook_ != 0 || this->filter_hook_ != 0) ::MH_ApplyQueued();
        capture_gate.Wait();
        if (this->window_hook_ != 0) ::MH_RemoveHook(reinterpret_cast<void*>(this->window_hook_));
        if (this->filter_hook_ != 0) ::MH_RemoveHook(reinterpret_cast<void*>(this->filter_hook_));
        if (this->minhook_owned_) ::MH_Uninitialize();
        this->enabled_ = this->minhook_owned_ = false;
        this->window_hook_ = this->filter_hook_ = 0;
        this->palette_ = {};
        this->runtime_ = nullptr;
    }

    auto native_chat_source::Deliver(chat_event event) -> void
    {
        if (this->runtime_ != nullptr) this->runtime_->HandleChat(event);
    }

    auto native_chat_source::Palette(void) const -> chat_palette
    {
        return this->enabled_ ? this->palette_.Read() : chat_palette{};
    }
}
