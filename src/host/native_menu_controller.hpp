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
            bool moved{};
        };
        std::array<menu, 64> menus_{};
        size_t count_{};
        uintptr_t module_{}, manager_{}, config_slot_{}, reposition_{};
        size_t module_size_{};
        native_ui_state state_{};
        native_menu_layout layout_{};
        int offset_x_{}, anchor_y_{};

        auto Move(menu& value, bool enabled) -> void;
        auto IsAttached(const menu& value) const -> bool;
        auto Find(const char* pattern) const -> uintptr_t;
    };
}
