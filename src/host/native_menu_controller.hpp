#pragma once

#include "native_menu_data.hpp"

#include <array>
#include <cstddef>

namespace xiui
{
    class native_menu_controller
    {
    public:
        auto Initialize(uintptr_t module, size_t size) -> void;
        auto Update(const native_menu_layout& layout) -> void;
        auto BeforeDraw(uintptr_t primitive) -> void;
        auto ShouldHide(uintptr_t primitive) const -> bool;
        auto SetHudSlots(const std::array<uintptr_t, native_hud_window_count>& slots) -> void { this->hud_slots_ = slots; }
        auto SetEditing(bool editing) -> void { this->editing_ = editing; }
        auto SetDrawMethod(uintptr_t method) -> void { this->draw_method_ = method; }
        auto DisableMovement(void) -> void { this->Update({}); this->state_.menu_available = false; }
        auto Shutdown(void) -> void;
        auto State(void) const -> const native_ui_state& { return this->state_; }

    private:
#if defined(XIUI_ENABLE_NATIVE_HUD_TESTS)
        friend struct native_menu_test;
#endif
        struct menu
        {
            uintptr_t primitive{}, owner{}, resource{};
            std::array<int16_t, 2> original{}, applied{};
            bool moved{}, detail{};
            size_t window{native_window_count};
            bool open{true};
        };
        std::array<menu, 64> menus_{};
        size_t count_{};
        std::array<menu, native_window_count> windows_{};
        size_t window_count_{};
        std::array<uintptr_t, native_hud_window_count> hud_slots_{};
        std::array<uintptr_t, std::size(native_menu_windows)> dialog_slots_{};
        std::array<bool, native_window_count> drawn_{};
        std::array<bool, native_window_count> submitted_{}, last_submitted_{};
        std::array<std::array<int, 2>, native_window_count> origins_{};
        uintptr_t module_{}, manager_{}, config_slot_{}, reposition_{}, comment_draw_{}, draw_method_{};
        uintptr_t primitive_create_{}, primitive_destroy_{}, primitive_bind_{}, primitive_select_{}, resource_find_{}, main_draw_{}, main_record_{};
        uintptr_t equipment_draw_{}, frame_read_{}, currency_draw_{};
        std::array<uintptr_t, 3> cached_frame_init_{};
        alignas(4) std::array<uint8_t, 0xD4> editor_main_{};
        alignas(4) std::array<uint8_t, 0x28> editor_main_owner_{};
        bool editor_main_created_{};
        size_t module_size_{};
        native_ui_state state_{};
        native_menu_layout layout_{};
        int offset_x_{}, anchor_y_{};
        bool editing_{}, replay_pending_{}, replaying_{};

        auto Move(menu& value, bool enabled) -> void;
        auto UpdateWindows(void) -> void;
        auto DrawInactive(uintptr_t current) -> void;
        auto UpdateEditorMain(void) -> void;
        auto DestroyEditorMain(void) -> void;
        auto MoveWindow(menu& value, bool restore = false) -> void;
        auto UpdateOwnedContent(const menu& value) -> void;
        auto WindowIndex(uintptr_t owner, const std::array<char, 16>& name, size_t detailOwner) const -> size_t;
        auto IsAttached(const menu& value) const -> bool;
        auto Find(const char* pattern) const -> uintptr_t;
    };
}
