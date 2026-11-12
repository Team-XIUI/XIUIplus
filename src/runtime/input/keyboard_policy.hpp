#pragma once

#include <Windows.h>
#include <cstdint>
#include <optional>

namespace xiui
{
    inline auto KeyboardAllowsCharacterShortcuts(const std::optional<bool> fullKeyboard) -> bool
    { return fullKeyboard == false; }

    inline auto KeyboardTextKey(const WPARAM key) -> bool
    {
        return key == VK_SPACE || (key >= '0' && key <= '9') || (key >= 'A' && key <= 'Z') ||
            (key >= VK_NUMPAD0 && key <= VK_DIVIDE) || (key >= VK_OEM_1 && key <= 0xC2) ||
            (key >= VK_OEM_4 && key <= VK_OEM_102) || key == VK_PROCESSKEY || key == VK_PACKET;
    }

    inline auto KeyboardStartsChat(const std::optional<bool> fullKeyboard, const WPARAM key, const LPARAM flags) -> bool
    {
        if (fullKeyboard != true || (static_cast<uint32_t>(flags) & 0xE0000000u) != 0 ||
            (::GetKeyState(VK_CONTROL) & 0x8000) != 0 || (::GetKeyState(VK_MENU) & 0x8000) != 0 ||
            (::GetKeyState(VK_LWIN) & 0x8000) != 0 || (::GetKeyState(VK_RWIN) & 0x8000) != 0) return false;
        return key == VK_SPACE || (key >= '0' && key <= '9') || (key >= 'A' && key <= 'Z') ||
            (key >= VK_OEM_1 && key <= VK_OEM_3) || (key >= VK_OEM_4 && key <= VK_OEM_8) || key == VK_OEM_102;
    }

    // Top-row keys are VK '0'-'9' on every layout, but AZERTY types é, è, ç and à there.
    inline auto KeyboardTypesDigit(const WPARAM key, const LPARAM flags, const HKL layout = nullptr) -> bool
    {
        if (key < '0' || key > '9') return false;
        BYTE state[256]{};
        if (!::GetKeyboardState(state)) return true;
        state[key] |= 0x80;
        wchar_t text[4]{};
        const int count = ::ToUnicodeEx(static_cast<UINT>(key), (static_cast<uint32_t>(flags) >> 16) & 0xFF, state, text, 4, 4,
            layout != nullptr ? layout : ::GetKeyboardLayout(0));
        return count != 1 || (text[0] >= L'0' && text[0] <= L'9');
    }

    inline auto KeyboardAllowsGlobalBinding(const std::optional<bool> fullKeyboard, const WPARAM key, const LPARAM flags, const HKL layout = nullptr) -> bool
    {
        if ((::GetKeyState(VK_LWIN) & 0x8000) != 0 || (::GetKeyState(VK_RWIN) & 0x8000) != 0) return false;
        const bool commandModifier = (::GetKeyState(VK_CONTROL) & 0x8000) != 0 || (::GetKeyState(VK_MENU) & 0x8000) != 0 ||
            (static_cast<uint32_t>(flags) & 0x20000000u) != 0;
        // Full keyboard types a top-row letter instead of treating it as a number binding.
        const bool numberKey = key >= '0' && key <= '9' && (fullKeyboard != true || KeyboardTypesDigit(key, flags, layout));
        return KeyboardAllowsCharacterShortcuts(fullKeyboard) || commandModifier || !KeyboardTextKey(key) || numberKey;
    }
}
