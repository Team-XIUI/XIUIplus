#include "native_hud_controller.hpp"

#include "Memory.h"
#include <MinHook.h>

#include <bit>
#include <cstdio>
#include <cstring>

namespace
{
    std::atomic<xiui::native_hud_controller*> controller{};
    void* draw_continuation{};
    void* draw_exit{};
    constexpr size_t target_coordinates = offsetof(Ashita::FFXI::targetwindow_t, FrameChildrenOffsetX);
    constexpr uint16_t hidden_coordinate = 30000;
    constexpr size_t frame_visibility = 0x69;

    template<typename T>
    auto Read(const uintptr_t address, T& value) noexcept -> bool
    {
        if (address == 0)
            return false;
        __try { std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T)); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return true;
    }

    auto WriteByte(const uintptr_t address, const uint8_t value) noexcept -> bool
    {
        DWORD previous{};
        if (!::VirtualProtect(reinterpret_cast<void*>(address), 1, PAGE_EXECUTE_READWRITE, &previous))
            return false;
        bool written{};
        __try { *reinterpret_cast<volatile uint8_t*>(address) = value; written = true; }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        ::FlushInstructionCache(::GetCurrentProcess(), reinterpret_cast<void*>(address), 1);
        DWORD ignored{};
        return ::VirtualProtect(reinterpret_cast<void*>(address), 1, previous, &ignored) && written;
    }

    template<typename T>
    auto WriteData(const uintptr_t address, const T& value) noexcept -> bool
    {
        __try { std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(T)); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    auto __stdcall SuppressPrimitive(const uintptr_t primitive) noexcept -> BOOL
    {
        auto* active = controller.load(std::memory_order_acquire);
        return active != nullptr && active->ShouldHide(primitive) ? TRUE : FALSE;
    }

    __declspec(naked) auto PrimitiveDrawGuard(void) -> void
    {
        __asm {
            pushfd
            pushad
            push ebp
            call SuppressPrimitive
            test eax, eax
            jnz hidden
            popad
            popfd
            jmp dword ptr [draw_continuation]
        hidden:
            popad
            popfd
            jmp dword ptr [draw_exit]
        }
    }
}

namespace xiui
{
    native_hud_controller::~native_hud_controller(void) { this->Shutdown(); }

    auto native_hud_controller::InModule(const uintptr_t address, const size_t size) const -> bool
    {
        return this->module_ != 0 && address >= this->module_ && size <= this->module_size_ &&
            address - this->module_ <= this->module_size_ - size;
    }

    auto native_hud_controller::Find(const char* pattern) const -> uintptr_t
    {
        const auto first = Ashita::Memory::FindPattern(this->module_, this->module_size_, pattern, 0, 0);
        return first != 0 && Ashita::Memory::FindPattern(this->module_, this->module_size_, pattern, 0, 1) == 0 ? first : 0;
    }

    auto native_hud_controller::FindMenu(const char* name) const -> uintptr_t
    {
        char resource[17]{};
        std::snprintf(resource, sizeof(resource), "menu    %-8.8s", name);
        std::string pattern;
        for (size_t i = 0; i < 32; ++i)
        {
            char byte[3]{};
            std::snprintf(byte, sizeof(byte), "%02X", i < 16 ? static_cast<uint8_t>(resource[i]) : 0);
            pattern += byte;
        }
        const auto match = this->Find(pattern.c_str());
        uintptr_t slot{};
        return this->InModule(match, 36) && Read(match + 0x20, slot) && this->InModule(slot, 4) ? slot : 0;
    }

    auto native_hud_controller::Initialize(IAshitaCore* core) -> void
    {
        this->Shutdown();
        this->core_ = core;
        this->module_ = Ashita::Memory::GetModuleBase("FFXiMain.dll");
        this->module_size_ = Ashita::Memory::GetModuleSize("FFXiMain.dll");
        if (core == nullptr || this->module_ == 0)
            return;

        for (size_t index = 0; index < native_hud_options.size(); ++index)
            if (native_hud_options[index].resource != nullptr)
                this->slots_[index] = this->FindMenu(native_hud_options[index].resource);
        this->chat_input_slot_ = this->FindMenu("inline");
        const auto alliance = this->Find("A1????????8B0D????????89442424A1????????33DB89");
        if (alliance != 0)
        {
            Read(alliance + 1, this->slots_[static_cast<size_t>(native_hud_element::alliance1)]);
            Read(alliance + 7, this->slots_[static_cast<size_t>(native_hud_element::alliance2)]);
        }

        // Match the complete native setup and epilogue before installing a draw guard.
        const auto entry = this->Find("83EC24535556578BE9E8????????668B455633FF663BC77E054866894556");
        if (entry != 0 && this->InModule(entry, 0x1000))
        {
            constexpr char ending[] = "8A859B00000084C0740F8A44241284C074078BCDE8????????5F5E5D5B83C424C3";
            const auto exit = Ashita::Memory::FindPattern(entry, 0x1000, ending, 0, 0);
            if (exit > entry + 20 && Ashita::Memory::FindPattern(entry, 0x1000, ending, 0, 1) == 0)
            {
                this->hook_site_ = entry + 14;
                this->hook_exit_ = exit + 25;
                for (size_t index = 0; index < this->slots_.size(); ++index)
                    if (this->InModule(this->slots_[index], 4))
                        this->available_ |= uint32_t{1} << index;
            }
        }

        if (!this->InModule(this->chat_input_slot_, 4))
            this->available_ &= ~NativeHudBit(native_hud_element::chat1);

        const auto compass = this->Find("33C0668B81????????483DE3000000");
        uint8_t original{};
        if (compass != 0 && Read(compass + 0x24, original) && original == 1)
        {
            this->compass_ = {compass + 0x24, original, 0, false};
            this->available_ |= NativeHudBit(native_hud_element::compass);
        }
        constexpr char dispatcher[] =
            "83EC08568BF1578D4424088B4E0833FF5066897C240C66897C240E"
            "66897C241066897C2412E8????????8B46142BC7741B48752A"
            "8B4C240A8B54240851528D4E44E8????????5F5E83C408C3"
            "8B44240A8B4C240850518D4E1CE8????????5F5E83C408C3";
        const auto network = this->Find(dispatcher);
        if (network != 0 && Read(network + 0x32, original) && original == 0x75)
            this->connection_ = {network + 0x32, original, 0xEB, false};
        else
            this->available_ &= ~NativeHudBit(native_hud_element::connection);
        this->available_ |= NativeHudBit(native_hud_element::clock);
        if (this->hook_site_ != 0) this->menus_.Initialize(this->module_, this->module_size_);
    }

    auto native_hud_controller::SetHook(const bool enabled) -> bool
    {
        if (enabled == this->hook_enabled_)
            return true;
        if (enabled && !this->hook_created_)
        {
            if (this->hook_site_ == 0)
                return false;
            const auto initialized = ::MH_Initialize();
            if (initialized != MH_OK && initialized != MH_ERROR_ALREADY_INITIALIZED)
                return false;
            this->minhook_owned_ = initialized == MH_OK;
            draw_exit = reinterpret_cast<void*>(this->hook_exit_);
            if (::MH_CreateHook(reinterpret_cast<void*>(this->hook_site_), &PrimitiveDrawGuard, &draw_continuation) != MH_OK)
                return false;
            this->hook_created_ = true;
        }
        if (enabled)
        {
            controller.store(this, std::memory_order_release);
            if (::MH_EnableHook(reinterpret_cast<void*>(this->hook_site_)) != MH_OK)
                return false;
        }
        else
        {
            if (::MH_DisableHook(reinterpret_cast<void*>(this->hook_site_)) != MH_OK)
                return false;
            controller.store(nullptr, std::memory_order_release);
        }
        this->hook_enabled_ = enabled;
        return true;
    }

    auto native_hud_controller::ApplyPatch(byte_patch& patch, const bool hidden) -> bool
    {
        if (patch.address == 0)
            return !hidden;
        uint8_t current{};
        if (!Read(patch.address, current))
            return false;
        if (hidden)
        {
            if (patch.owned)
                return current == patch.hidden;
            if (current != patch.original)
                return false;
            patch.owned = true;
            return WriteByte(patch.address, patch.hidden);
        }
        if (patch.owned)
        {
            if (current != patch.hidden && current != patch.original)
                return false;
            if (current == patch.hidden && !WriteByte(patch.address, patch.original))
                return false;
            patch.owned = false;
        }
        return true;
    }

    auto native_hud_controller::LoggedIn(void) const -> bool
    {
        auto* memory = this->core_ != nullptr ? this->core_->GetMemoryManager() : nullptr;
        auto* player = memory != nullptr ? memory->GetPlayer() : nullptr;
        return player != nullptr && player->GetLoginStatus() == static_cast<uint8_t>(Ashita::FFXI::Enums::LoginStatus::LoggedIn);
    }

    auto native_hud_controller::IsFishing(void) const -> bool
    {
        auto* memory = this->core_->GetMemoryManager();
        auto* party = memory != nullptr ? memory->GetParty() : nullptr;
        auto* entity = memory != nullptr ? memory->GetEntity() : nullptr;
        const auto index = party != nullptr ? party->GetMemberTargetIndex(0) : 0;
        if (entity == nullptr || index == 0 || index >= entity->GetEntityMapSize())
            return true;
        const auto fishing = [](const uint32_t status) {
            return (status >= 38 && status <= 43) || (status >= 50 && status <= 53) || (status >= 56 && status <= 62);
        };
        return fishing(entity->GetStatus(index)) || fishing(entity->GetStatusServer(index));
    }

    auto native_hud_controller::SetClock(const bool hidden) -> void
    {
        if (this->clock_hidden_ == hidden)
            return;
        if (this->LoggedIn() && this->core_->GetChatManager() != nullptr)
            this->core_->GetChatManager()->QueueCommand(-1, hidden ? "/clock off" : "/clock on");
        this->clock_hidden_ = hidden;
    }

    auto native_hud_controller::Update(uint32_t hiddenMask, const native_menu_layout& layout) -> void
    {
        hiddenMask = this->LoggedIn() ? hiddenMask & this->available_ : 0;
        auto menuLayout = layout;
        menuLayout.enabled = menuLayout.enabled && this->LoggedIn();
        this->menus_.Update(menuLayout);
        if ((hiddenMask & NativeHudBit(native_hud_element::target)) == 0)
            this->RestoreTargetBox();
        if (hiddenMask != 0 && this->IsFishing())
            hiddenMask &= ~NativeHudBit(native_hud_element::party);
        const uint32_t windowMask = hiddenMask & ~(NativeHudBit(native_hud_element::compass) | NativeHudBit(native_hud_element::clock));
        this->hidden_.store(hiddenMask, std::memory_order_release);
        if (!this->SetHook(windowMask != 0 || (menuLayout.enabled && this->menus_.State().menu_available)))
        {
            this->hidden_.store(0, std::memory_order_release);
            this->RestoreTargetBox();
            this->menus_.DisableMovement();
            this->available_ &= NativeHudBit(native_hud_element::compass) | NativeHudBit(native_hud_element::clock);
        }
        if (!this->ApplyPatch(this->compass_, (hiddenMask & NativeHudBit(native_hud_element::compass)) != 0))
            this->available_ &= ~NativeHudBit(native_hud_element::compass);
        if (!this->ApplyPatch(this->connection_, (hiddenMask & NativeHudBit(native_hud_element::connection)) != 0))
            this->available_ &= ~NativeHudBit(native_hud_element::connection);
        this->SetClock((hiddenMask & NativeHudBit(native_hud_element::clock)) != 0);
    }

    auto native_hud_controller::ShouldHide(const uintptr_t primitive) -> bool
    {
        this->menus_.BeforeDraw(primitive);
        const auto mask = this->hidden_.load(std::memory_order_acquire);
        uintptr_t owner{};
        if (mask == 0 || !Read(primitive + 0x0C, owner) || owner == 0)
            return false;
        uintptr_t attached{};
        if (!Read(owner + 8, attached) || attached != primitive)
            return false;
        if ((mask & NativeHudBit(native_hud_element::chat1)) != 0)
        {
            uintptr_t inputOwner{};
            if (Read(this->chat_input_slot_, inputOwner) && inputOwner == owner)
            {
                this->skipped_.fetch_add(1, std::memory_order_relaxed);
                return true;
            }
        }
        for (size_t index = 0; index < this->slots_.size(); ++index)
        {
            if ((mask & (uint32_t{1} << index)) == 0 || this->slots_[index] == 0)
                continue;
            uintptr_t current{};
            if (Read(this->slots_[index], current) && current == owner)
            {
                if (index == static_cast<size_t>(native_hud_element::target))
                {
                    if (this->HideTargetBox(owner, primitive))
                        this->skipped_.fetch_add(1, std::memory_order_relaxed);
                    return false;
                }
                this->skipped_.fetch_add(1, std::memory_order_relaxed);
                return true;
            }
        }
        return false;
    }

    auto native_hud_controller::HideTargetBox(const uintptr_t owner, const uintptr_t primitive) -> bool
    {
        std::array<uint16_t, 4> coordinates{};
        uint8_t visible{};
        if (!Read(owner + target_coordinates, coordinates) || !Read(primitive + frame_visibility, visible) || visible > 1)
            return false;
        if (this->target_box_.owner != owner || this->target_box_.primitive != primitive)
            this->target_box_ = {owner, primitive, coordinates, visible};

        for (size_t i = 0; i < coordinates.size(); ++i)
            if (coordinates[i] != hidden_coordinate)
                this->target_box_.coordinates[i] = coordinates[i];
        if (visible != 0)
            this->target_box_.frame_visible = visible;

        // Hide frame artwork and box/icon contents; keep the +0x6A callback and both arrow anchors intact.
        constexpr std::array<uint16_t, 4> hidden{hidden_coordinate, hidden_coordinate, hidden_coordinate, hidden_coordinate};
        if ((coordinates == hidden || WriteData(owner + target_coordinates, hidden)) &&
            (visible == 0 || WriteData(primitive + frame_visibility, uint8_t{0})))
            return true;
        this->RestoreTargetBox();
        return false;
    }

    auto native_hud_controller::RestoreTargetBox(void) -> void
    {
        const auto state = this->target_box_;
        this->target_box_ = {};
        if (state.owner == 0)
            return;
        uintptr_t owner{}, primitive{};
        const auto slot = this->slots_[static_cast<size_t>(native_hud_element::target)];
        if (!Read(slot, owner) || owner != state.owner || !Read(owner + 8, primitive) || primitive != state.primitive)
            return;
        for (size_t i = 0; i < state.coordinates.size(); ++i)
        {
            uint16_t current{};
            const auto address = owner + target_coordinates + i * sizeof(uint16_t);
            if (Read(address, current) && current == hidden_coordinate)
                WriteData(address, state.coordinates[i]);
        }
        uint8_t visible{};
        if (Read(primitive + frame_visibility, visible) && visible == 0)
            WriteData(primitive + frame_visibility, state.frame_visible);
    }

    auto native_hud_controller::Status(void) const -> std::string
    {
        char text[192]{};
        std::snprintf(text, sizeof(text), "[XIUI Native] Game HUD: %u/%u controls supported; %u hidden; draw guard %s; %llu draws suppressed.",
            std::popcount(this->available_), static_cast<unsigned>(native_hud_options.size()), std::popcount(this->hidden_.load()),
            this->hook_enabled_ ? "active" : "idle", static_cast<unsigned long long>(this->skipped_.load()));
        return text;
    }

    auto native_hud_controller::Shutdown(void) -> void
    {
        this->hidden_.store(0, std::memory_order_release);
        const bool detached = this->SetHook(false);
        controller.store(nullptr, std::memory_order_release);
        this->RestoreTargetBox();
        this->menus_.Shutdown();
        if (!detached)
        {
            // Keep the pass-through guard valid if the hook cannot be detached.
            HMODULE pinned{};
            ::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                reinterpret_cast<const char*>(&PrimitiveDrawGuard), &pinned);
        }
        if (detached && this->hook_created_)
            ::MH_RemoveHook(reinterpret_cast<void*>(this->hook_site_));
        if (detached && this->minhook_owned_)
            ::MH_Uninitialize();
        this->ApplyPatch(this->compass_, false);
        this->ApplyPatch(this->connection_, false);
        this->SetClock(false);
        this->hook_created_ = this->hook_enabled_ = this->minhook_owned_ = false;
        this->hook_site_ = this->hook_exit_ = 0;
        this->available_ = 0;
        this->slots_.fill(0);
        this->chat_input_slot_ = 0;
        this->compass_ = {};
        this->connection_ = {};
        this->core_ = nullptr;
        this->module_ = this->module_size_ = 0;
    }
} // namespace xiui
