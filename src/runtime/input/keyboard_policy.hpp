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

    inline auto KeyboardAllowsGlobalBinding(const std::optional<bool> fullKeyboard, const WPARAM key, const LPARAM flags) -> bool
    {
        if ((::GetKeyState(VK_LWIN) & 0x8000) != 0 || (::GetKeyState(VK_RWIN) & 0x8000) != 0) return false;
        const bool commandModifier = (::GetKeyState(VK_CONTROL) & 0x8000) != 0 || (::GetKeyState(VK_MENU) & 0x8000) != 0 ||
            (static_cast<uint32_t>(flags) & 0x20000000u) != 0;
        return KeyboardAllowsCharacterShortcuts(fullKeyboard) || commandModifier || !KeyboardTextKey(key) || (key >= '0' && key <= '9');
    }
}
