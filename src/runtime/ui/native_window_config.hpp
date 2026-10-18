#pragma once

#include <cstdint>
#include <optional>

namespace xiui
{
    struct native_chat_window_settings
    {
        bool reactive{};
        uint32_t minimum_lines{}, maximum_lines{}, resize_seconds{};
        auto operator==(const native_chat_window_settings&) const -> bool = default;
    };

    class native_window_config
    {
    public:
        auto Initialize(void) -> void;
        auto Type(void) const -> std::optional<uint8_t>;
        auto TimestampMode(void) const -> std::optional<uint8_t>;
        auto FullKeyboard(void) const -> std::optional<bool>;
        auto ChatWindow(bool secondary = false) const -> std::optional<native_chat_window_settings>;
        auto SetType(int type) const -> bool;
        auto ChatFilterValue(uint32_t id) const -> std::optional<uint32_t>;
        auto SetChatFilterValue(uint32_t id, uint32_t value) const -> bool;

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
