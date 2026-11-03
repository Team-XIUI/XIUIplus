#pragma once

#include <Windows.h>
#include <cstdint>

namespace xiui
{
    inline constexpr uint32_t mouse_button_mask = MK_LBUTTON | MK_RBUTTON | MK_MBUTTON | MK_XBUTTON1 | MK_XBUTTON2;

    class mouse_input
    {
    public:
        auto Reset(void) -> void { this->buttons_ = 0; }
        auto Normalize(const uint32_t message, const WPARAM data, const uint32_t physicalButtons, const uint32_t modifiers) -> WPARAM
        {
            const auto xButton = HIWORD(data) == XBUTTON1 ? MK_XBUTTON1 : HIWORD(data) == XBUTTON2 ? MK_XBUTTON2 : 0;
            switch (message)
            {
            case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: this->buttons_ |= MK_LBUTTON; break;
            case WM_LBUTTONUP: this->buttons_ &= ~MK_LBUTTON; break;
            case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK: this->buttons_ |= MK_RBUTTON; break;
            case WM_RBUTTONUP: this->buttons_ &= ~MK_RBUTTON; break;
            case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK: this->buttons_ |= MK_MBUTTON; break;
            case WM_MBUTTONUP: this->buttons_ &= ~MK_MBUTTON; break;
            case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK: this->buttons_ |= xButton; break;
            case WM_XBUTTONUP: this->buttons_ &= ~xButton; break;
            case WM_MOUSEMOVE:
                // Polling may cancel a lost release, but never creates a press.
                this->buttons_ &= physicalButtons | (static_cast<uint32_t>(data) & mouse_button_mask);
                break;
            }
            return (data & 0xFFFF0000u) | this->buttons_ | ((data | modifiers) & (MK_SHIFT | MK_CONTROL));
        }
    private:
        uint32_t buttons_{};
    };

    inline auto PhysicalMouseButtons(void) -> uint32_t
    {
        uint32_t buttons{};
        constexpr int keys[]{VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2};
        for (const auto key : keys)
            if ((::GetAsyncKeyState(key) & 0x8000) != 0)
                buttons |= key == VK_LBUTTON ? MK_LBUTTON : key == VK_RBUTTON ? MK_RBUTTON :
                    key == VK_MBUTTON ? MK_MBUTTON : key == VK_XBUTTON1 ? MK_XBUTTON1 : MK_XBUTTON2;
        return buttons;
    }

    inline auto PhysicalMouseModifiers(void) -> uint32_t
    {
        return (((::GetKeyState(VK_SHIFT) | ::GetAsyncKeyState(VK_SHIFT)) & 0x8000) != 0 ? MK_SHIFT : 0) |
            (((::GetKeyState(VK_CONTROL) | ::GetAsyncKeyState(VK_CONTROL)) & 0x8000) != 0 ? MK_CONTROL : 0);
    }
}
