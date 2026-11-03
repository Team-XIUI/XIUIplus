#pragma once

#include <Windows.h>
#include <array>
#include <cstddef>
#include <cstdint>

namespace xiui
{
    struct mouse_trace_entry
    {
        size_t sequence{};
        uint32_t message{}, data{}, keys{};
        int x{}, y{};
        bool blocked{}, shift{}, config_down_before{}, config_down_after{};
        const char* owner{"pending"};
        bool consumed{};
    };

    struct mouse_button_trace
    {
        std::array<std::array<mouse_trace_entry, 6>, 2> events{};
        std::array<size_t, 2> counts{};
        size_t total{};
        auto Clear(void) -> void { this->events = {}; this->counts = {}; this->total = 0; }
        auto Record(uint32_t message, uint32_t data, uint32_t keys, int x, int y, bool blocked, bool configDown) -> mouse_trace_entry&
        {
            const bool shift = (keys & MK_SHIFT) != 0;
            auto& event = this->events[shift][this->counts[shift]++ % this->events[shift].size()];
            event = {++this->total, message, data, keys, x, y, blocked, shift, configDown, configDown};
            return event;
        }
    };

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
