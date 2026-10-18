#pragma once

#include <cstdint>
#include <array>
#include <type_traits>

namespace xiui
{
    enum class controller_backend : uint8_t { none, direct_input, xinput };
    enum class controller_capture_mode : uint32_t { none, navigation, exclusive };
    enum class controller_action : uint8_t
    {
        none, up, down, left, right, confirm, cancel, next_tab, previous_tab, cycle_window, actions, focus,
        crossbar_bank, crossbar_slot, palette_previous, palette_next
    };

    enum class crossbar_bank : uint8_t { none, left, right, left_right, right_left, left_double, right_double };

    struct controller_crossbar_mapping
    {
        // FFXI mapping values: -2 follows the effective game map; -1 is unbound.
        int16_t left_trigger{-2};
        int16_t right_trigger{-2};
        int16_t palette_modifier{-2};
        std::array<int16_t, 8> slots{-2, -2, -2, -2, -2, -2, -2, -2};

        auto operator==(const controller_crossbar_mapping&) const -> bool = default;
    };

    struct controller_crossbar_settings
    {
        bool enabled{};
        bool replace_native{};
        bool suppress_native_macros{};
        bool expanded{true};
        bool double_tap{};
        bool palette_cycle{true};
        bool palette_left_shoulder{};
        uint16_t trigger_press{30};
        uint16_t trigger_release{15};
        uint16_t double_tap_ms{300};
        uint16_t min_hold_ms{50};
        controller_crossbar_mapping xinput{};
        controller_crossbar_mapping direct_input{};

        auto operator==(const controller_crossbar_settings&) const -> bool = default;
    };

    inline constexpr uint32_t controller_capture_mask = 3;
    inline constexpr uint32_t controller_party_input = 4;
    inline constexpr uint32_t controller_revision_mask = 0xffff0000;

    constexpr auto ControllerEntryBit(const controller_action action) -> uint32_t
    {
        return action > controller_action::none && action <= controller_action::focus ?
            1u << (4 + static_cast<uint32_t>(action)) : 0;
    }

    constexpr auto FilterControllerMenu(uint32_t state, const bool menuOpen) -> uint32_t
    {
        if (!menuOpen) return state;
        state &= ~(ControllerEntryBit(controller_action::up) | ControllerEntryBit(controller_action::down));
        return (state & controller_party_input) != 0 ? state & ~controller_capture_mask : state;
    }

    struct controller_event
    {
        controller_action action{};
        controller_backend backend{};
        uint8_t user_index{};
        bool pressed{};
        bool repeated{};
        bool injected{};
        uint64_t timestamp{};
        crossbar_bank bank{};
        uint8_t slot{};
        uint8_t triggers{};
    };

    struct controller_status
    {
        controller_backend backend{};
        uint8_t user_index{};
        bool enumeration_disabled{};
        bool configuration_known{};
        bool gamepad_enabled{};
        bool connected{};
        bool connection_known{};
        bool received_input{};
        bool bindings_known{};
        bool suspended{};
        uint64_t last_input_at{};
        controller_crossbar_mapping crossbar_mapping{};
        bool crossbar_mapping_known{};
        bool crossbar_ready{};
    };

    static_assert(std::is_trivially_copyable_v<controller_event>);
    static_assert(std::is_trivially_copyable_v<controller_status>);
    static_assert(std::is_trivially_copyable_v<controller_crossbar_settings>);
}
