#pragma once

#include "modules/module.hpp"
#include "ui/native_window_config.hpp"

namespace xiui
{
    class native_chat_filters
    {
    public:
        static constexpr size_t global_count = 55, window_count = 28;
        static constexpr size_t option_count = 1 + global_count + window_count;
        auto Initialize(native_window_config& config) -> void;
        auto Refresh(void) -> void;
        auto WindowHasRouting(bool secondary) const -> std::optional<bool>;
        auto Visible(size_t index, bool secondary = false) const -> bool;
        auto Option(size_t index, bool secondary = false) -> module_option;
        auto Apply(size_t index, bool secondary = false) -> bool;

    private:
#if defined(XIUI_ENABLE_CHAT_TESTS)
        friend struct chat_test;
#endif
        struct filter { uint8_t bit{}; int8_t hold{}; uint16_t text{}; };
        static_assert(sizeof(filter) == 4);
        native_window_config* config_{};
        std::array<filter, global_count> filters_{};
        std::array<std::string, global_count> names_{};
        std::array<int, global_count> values_{};
        std::array<std::array<bool, window_count>, 2> window_values_{};
        std::array<int, 2> groups_{1, 2};
        std::array<bool, 2> window_available_{};
        bool definitions_available_{}, global_available_{};
    };
}
