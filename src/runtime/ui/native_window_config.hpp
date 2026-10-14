#pragma once

#include <cstdint>
#include <optional>

namespace xiui
{
    class native_window_config
    {
    public:
        auto Initialize(void) -> void;
        auto Type(void) const -> std::optional<uint8_t>;
        auto TimestampMode(void) const -> std::optional<uint8_t>;
        auto SetType(int type) const -> bool;

    private:
#if defined(XIUI_ENABLE_INPUT_TESTS)
        friend struct config_input_test;
#endif
#if defined(XIUI_ENABLE_CHAT_TESTS)
        friend struct chat_test;
#endif
        uintptr_t config_slot_{}, setter_{};
    };
}
