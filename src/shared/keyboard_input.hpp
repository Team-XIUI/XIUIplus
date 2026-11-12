#pragma once

#include <cstdint>

namespace xiui
{
    inline constexpr uint32_t keyboard_capture_all = 1;
    inline constexpr uint32_t keyboard_capture_history = 2;
    inline constexpr uint32_t keyboard_capture_status = 4;
    inline constexpr uint32_t keyboard_capture_cycle = 8;
    // Numpad + is the native window cycle in every keyboard layout.
    inline constexpr uint32_t keyboard_capture_numpad_cycle = 16;

    constexpr auto FilterKeyboardCapture(const uint32_t state, const bool modified) -> uint32_t
    { return modified ? state & ~(keyboard_capture_cycle | keyboard_capture_numpad_cycle) : state; }

    constexpr auto KeyboardCapturesScan(const uint32_t state, const uint32_t scan) -> bool
    {
        if ((state & keyboard_capture_all) != 0) return true;
        if (scan == 0x21) return (state & (keyboard_capture_history | keyboard_capture_status | keyboard_capture_cycle)) != 0;
        if (scan == 0x4E) return (state & (keyboard_capture_history | keyboard_capture_status | keyboard_capture_numpad_cycle)) != 0;
        if (scan == 0x01 || scan == 0x1C || scan == 0x9C || scan == 0xC8 || scan == 0xD0 || scan == 0xCB || scan == 0xCD)
            return (state & (keyboard_capture_history | keyboard_capture_status)) != 0;
        if (scan == 0xC7 || scan == 0xCF || scan == 0xC9 || scan == 0xD1) return (state & keyboard_capture_history) != 0;
        return scan == 0x0F && (state & keyboard_capture_status) != 0;
    }
}
