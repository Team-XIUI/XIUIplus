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
        uint32_t message{}, data{}, keys{}, physical{};
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
        auto Record(uint32_t message, uint32_t data, uint32_t keys, uint32_t physical, int x, int y, bool blocked, bool configDown) -> mouse_trace_entry&
        {
            const bool shift = (keys & MK_SHIFT) != 0;
            auto& event = this->events[shift][this->counts[shift]++ % this->events[shift].size()];
            event = {++this->total, message, data, keys, physical, x, y, blocked, shift, configDown, configDown};
            return event;
        }
    };

    inline constexpr uint32_t mouse_button_mask = MK_LBUTTON | MK_RBUTTON | MK_MBUTTON | MK_XBUTTON1 | MK_XBUTTON2;
    // Polling runs ahead of queued messages, so a polled release must persist before it replaces a missing one.
    inline constexpr uint64_t mouse_release_grace_ms = 250;

    struct mouse_input_stats
    {
        size_t ignored_releases{}, recovered_releases{}, repeated_presses{};
        bool move_flags{};
    };

    // Button messages own press/release state. Polling only recovers a release it can corroborate.
    class mouse_input
    {
    public:
        auto Reset(void) -> void
        {
            this->buttons_ = this->confirmed_ = this->releasing_ = this->ignored_ = this->lost_ = this->repeated_ = 0;
        }
        auto Normalize(const uint32_t message, const WPARAM data, const uint32_t physicalButtons, const uint32_t modifiers, const uint64_t now) -> WPARAM
        {
            this->lost_ = this->repeated_ = 0;
            const auto xButton = HIWORD(data) == XBUTTON1 ? MK_XBUTTON1 : HIWORD(data) == XBUTTON2 ? MK_XBUTTON2 : 0;
            uint32_t pressed{}, released{};
            switch (message)
            {
            case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: pressed = MK_LBUTTON; break;
            case WM_LBUTTONUP: released = MK_LBUTTON; break;
            case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK: pressed = MK_RBUTTON; break;
            case WM_RBUTTONUP: released = MK_RBUTTON; break;
            case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK: pressed = MK_MBUTTON; break;
            case WM_MBUTTONUP: released = MK_MBUTTON; break;
            case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK: pressed = xButton; break;
            case WM_XBUTTONUP: released = xButton; break;
            case WM_MOUSEMOVE: this->Reconcile(static_cast<uint32_t>(data) & mouse_button_mask, physicalButtons, now); break;
            }
            if (pressed != 0)
            {
                // Pressing a held button again means its release never arrived.
                this->repeated_ = this->buttons_ & pressed;
                if (this->repeated_ != 0) ++this->stats_.repeated_presses;
                this->buttons_ |= pressed;
                this->confirmed_ = (this->confirmed_ & ~pressed) | (physicalButtons & pressed);
                this->releasing_ &= ~pressed; this->ignored_ &= ~pressed;
            }
            if (released != 0) this->Forget(released);
            return (data & 0xFFFF0000u) | this->buttons_ | ((data | modifiers) & (MK_SHIFT | MK_CONTROL));
        }
        auto Buttons(void) const -> uint32_t { return this->buttons_; }
        // Held buttons ended by corroborated polling during the last Normalize.
        auto Lost(void) const -> uint32_t { return this->lost_; }
        // Held buttons pressed again during the last Normalize.
        auto Repeated(void) const -> uint32_t { return this->repeated_; }
        auto Stats(void) const -> const mouse_input_stats& { return this->stats_; }
        auto ClearStats(void) -> void { this->stats_ = {.move_flags = this->stats_.move_flags}; }
    private:
        uint32_t buttons_{}, confirmed_{}, releasing_{}, ignored_{}, lost_{}, repeated_{};
        std::array<uint64_t, 5> released_at_{};
        mouse_input_stats stats_{};

        auto Forget(const uint32_t buttons) -> void
        {
            this->buttons_ &= ~buttons; this->confirmed_ &= ~buttons; this->releasing_ &= ~buttons; this->ignored_ &= ~buttons;
        }
        auto Reconcile(const uint32_t flags, const uint32_t physical, const uint64_t now) -> void
        {
            // Some hosts send zero data; held flags on movement are trusted only once a host has sent them.
            if ((flags & this->buttons_) != 0) this->stats_.move_flags = true;
            if (this->stats_.move_flags) this->lost_ = this->buttons_ & ~flags & ~physical;
            else
            {
                constexpr uint32_t bits[]{MK_LBUTTON, MK_RBUTTON, MK_MBUTTON, MK_XBUTTON1, MK_XBUTTON2};
                for (size_t index = 0; index < std::size(bits); ++index)
                {
                    const auto bit = bits[index];
                    if ((this->buttons_ & bit) == 0) continue;
                    if ((physical & bit) != 0) { this->confirmed_ |= bit; this->releasing_ &= ~bit; continue; }
                    // Polling that never saw this press cannot prove its release.
                    if ((this->confirmed_ & bit) == 0)
                    {
                        if ((this->ignored_ & bit) == 0) { this->ignored_ |= bit; ++this->stats_.ignored_releases; }
                        continue;
                    }
                    if ((this->releasing_ & bit) == 0) { this->releasing_ |= bit; this->released_at_[index] = now; }
                    else if (now - this->released_at_[index] >= mouse_release_grace_ms) this->lost_ |= bit;
                }
            }
            if (this->lost_ == 0) return;
            ++this->stats_.recovered_releases;
            this->Forget(this->lost_);
        }
    };

    inline auto PhysicalMouseButtons(void) -> uint32_t
    {
        // GetAsyncKeyState reports physical buttons; map them to the logical buttons messages use.
        const bool swapped = ::GetSystemMetrics(SM_SWAPBUTTON) != 0;
        const auto down = [](const int key) { return (::GetAsyncKeyState(key) & 0x8000) != 0; };
        uint32_t buttons{};
        if (down(VK_LBUTTON)) buttons |= swapped ? MK_RBUTTON : MK_LBUTTON;
        if (down(VK_RBUTTON)) buttons |= swapped ? MK_LBUTTON : MK_RBUTTON;
        if (down(VK_MBUTTON)) buttons |= MK_MBUTTON;
        if (down(VK_XBUTTON1)) buttons |= MK_XBUTTON1;
        if (down(VK_XBUTTON2)) buttons |= MK_XBUTTON2;
        return buttons;
    }

    inline auto PhysicalMouseModifiers(void) -> uint32_t
    {
        return (((::GetKeyState(VK_SHIFT) | ::GetAsyncKeyState(VK_SHIFT)) & 0x8000) != 0 ? MK_SHIFT : 0) |
            (((::GetKeyState(VK_CONTROL) | ::GetAsyncKeyState(VK_CONTROL)) & 0x8000) != 0 ? MK_CONTROL : 0);
    }
}
