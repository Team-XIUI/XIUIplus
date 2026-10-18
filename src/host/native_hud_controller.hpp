#pragma once

#include "Ashita.h"
#include "native_hud_options.hpp"
#include "native_menu_controller.hpp"
#include "modules/actionbars/native_macros.hpp"

#include <atomic>
#include <string>

namespace xiui
{
    class native_hud_controller
    {
    public:
        ~native_hud_controller(void);
        auto Initialize(IAshitaCore* core) -> void;
        auto Update(uint32_t hiddenMask, const native_menu_layout& layout = {}) -> void;
        auto UiState(void) const -> const native_ui_state& { return this->menus_.State(); }
        auto Shutdown(void) -> void;
        auto AvailableMask(void) const -> uint32_t { return this->available_; }
        auto Status(void) const -> std::string;
        auto ShouldHide(uintptr_t primitive) -> bool;
        static auto __stdcall InvokeMacro(void* context, uint8_t book, uint8_t set, uint8_t slot, BOOL alt) -> BOOL;

    private:
#if defined(XIUI_ENABLE_NATIVE_HUD_TESTS)
        friend struct native_hud_target_test;
#endif
        struct byte_patch
        {
            uintptr_t address{};
            uint8_t original{};
            uint8_t hidden{};
            bool owned{};
        };

        struct target_box_state
        {
            uintptr_t owner{}, primitive{};
            std::array<uint16_t, 4> coordinates{};
            uint8_t frame_visible{};
        };

        IAshitaCore* core_{};
        uintptr_t module_{};
        size_t module_size_{};
        std::array<uintptr_t, native_hud_options.size()> slots_{};
        std::array<uintptr_t, 4> macro_palette_slots_{};
        uintptr_t chat_input_slot_{};
        uint32_t available_{};
        std::atomic<uint32_t> hidden_{};
        std::atomic<uint64_t> skipped_{};
        byte_patch compass_{};
        byte_patch connection_{};
        target_box_state target_box_{};
        native_menu_controller menus_{};
        uintptr_t hook_site_{};
        uintptr_t hook_exit_{};
        bool hook_created_{};
        bool hook_enabled_{};
        bool minhook_owned_{};
        bool clock_hidden_{};
        bool macro_palette_available_{};
        native_macros native_macros_{};
        uintptr_t macro_entry_{};
        bool macro_hook_created_{}, macro_hook_enabled_{}, minhook_initialized_{};
        using macro_run_fn = void(__thiscall*)(void*, uint8_t, uint8_t);
        static std::atomic<macro_run_fn> macro_continuation_;
        static std::atomic_bool macro_suppressed_;

        static auto __fastcall MacroRunGuard(void* self, void*, uint8_t mode, uint8_t slot) -> void;
        auto InitializeHooks(void) -> bool;
        auto SetMacroHook(bool enabled) -> bool;
        auto Find(const char* pattern) const -> uintptr_t;
        auto FindMenu(const char* name) const -> uintptr_t;
        auto InModule(uintptr_t address, size_t size) const -> bool;
        auto SetHook(bool enabled) -> bool;
        auto ApplyPatch(byte_patch& patch, bool hidden) -> bool;
        auto HideTargetBox(uintptr_t owner, uintptr_t primitive) -> bool;
        auto RestoreTargetBox(void) -> void;
        auto SetClock(bool hidden) -> void;
        auto IsFishing(void) const -> bool;
        auto LoggedIn(void) const -> bool;
    };
} // namespace xiui
