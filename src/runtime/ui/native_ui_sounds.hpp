#pragma once

#include <array>
#include <cstdint>
#include "embedded_sound_player.hpp"

namespace xiui
{
    struct config_input_test;
    struct chat_test;
}

namespace xiui::ui
{
    enum class native_ui_sound : uint8_t { hover, select, menu_open, chatmon_tell, chatmon_talk, chatmon_emote, chatmon_examine, chatmon_invite, chat_send = select };

    class native_ui_sounds
    {
    public:
        auto Initialize(void) -> void;
        auto Shutdown(void) -> void { this->clips_.Stop(); }
        auto Play(native_ui_sound sound) const -> bool;
        auto MenuOpened(uint64_t now, bool audible = true) -> void;
        auto Hover(uint64_t target, uint64_t now, bool enabled = true) -> void;
        auto Select(uint64_t now) -> void;
        auto Available(void) const -> bool;

    private:
#if defined(XIUI_ENABLE_INPUT_TESTS)
        friend struct xiui::config_input_test;
#endif
#if defined(XIUI_ENABLE_CHAT_TESTS)
        friend struct xiui::chat_test;
#endif
        struct entry { uintptr_t function{}, cache_slot{}, resource_slot{}; };
        std::array<entry, 8> sounds_{};
        embedded_sound_player clips_{};
        uint64_t hovered_{};
        uint64_t suppress_hover_until_{};
        bool initial_hover_{true};
    };
}
