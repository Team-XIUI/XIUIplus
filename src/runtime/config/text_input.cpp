#include "text_input.hpp"
#include "chat/chat_encoding.hpp"

#include <algorithm>
#include <cstring>

namespace xiui::ui
{
    auto text_input::Insert(const std::string_view text) -> void
    {
        if (text.empty() || std::any_of(text.begin(), text.end(), [](const unsigned char c) { return c < 32 || c == 127; })) return;
        const auto [begin, end] = this->editor_.Selection();
        if (this->editor_.Text().size() - (end - begin) + text.size() <= this->limit_) this->editor_.Insert(text);
    }

    auto text_input::Copy(const HWND window) const -> bool
    {
        const auto text = FromUtf8(this->editor_.SelectedText());
        if (text.empty() || !::OpenClipboard(window)) return false;
        bool copied{};
        const auto memory = ::GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t));
        if (memory != nullptr)
        {
            if (auto* buffer = ::GlobalLock(memory))
            {
                std::memcpy(buffer, text.c_str(), (text.size() + 1) * sizeof(wchar_t));
                ::GlobalUnlock(memory);
                copied = ::EmptyClipboard() && ::SetClipboardData(CF_UNICODETEXT, memory) != nullptr;
            }
            if (!copied) ::GlobalFree(memory);
        }
        ::CloseClipboard();
        return copied;
    }

    auto text_input::Paste(const HWND window) -> void
    {
        if (!::OpenClipboard(window)) return;
        const auto memory = ::GetClipboardData(CF_UNICODETEXT);
        if (memory != nullptr)
        {
            if (const auto* text = static_cast<const wchar_t*>(::GlobalLock(memory)))
            {
                const auto capacity = std::min<size_t>(::GlobalSize(memory) / sizeof(wchar_t), 4096);
                const auto length = wcsnlen_s(text, capacity);
                if (length < capacity) this->Insert(ToUtf8({text, length}));
                ::GlobalUnlock(memory);
            }
        }
        ::CloseClipboard();
    }

    auto text_input::HandleKey(const WPARAM key, const LPARAM flags, const HWND window) -> void
    {
        const bool control = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool shift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if ((::GetKeyState(VK_MENU) & 0x8000) != 0 || (static_cast<uint32_t>(flags) & 0x20000000u) != 0) return;
        switch (key)
        {
        case VK_BACK: this->editor_.Erase(true, control); return;
        case VK_DELETE: this->editor_.Erase(false, control); return;
        case VK_LEFT: this->editor_.Move(-1, shift, control); return;
        case VK_RIGHT: this->editor_.Move(1, shift, control); return;
        case VK_HOME: this->editor_.Home(false, shift); return;
        case VK_END: this->editor_.Home(true, shift); return;
        default: break;
        }
        if (control)
        {
            if (key == 'A') this->editor_.SelectAll();
            else if (key == 'C' || key == 'X') { if (this->Copy(window) && key == 'X') this->editor_.Erase(true); }
            else if (key == 'V') this->Paste(window);
            return;
        }
        BYTE state[256]{}; ::GetKeyboardState(state);
        if (key < 256) state[key] |= 0x80;
        wchar_t text[8]{};
        const auto count = ::ToUnicodeEx(static_cast<UINT>(key), (static_cast<uint32_t>(flags) >> 16) & 0xFF, state, text, 8, 4, ::GetKeyboardLayout(0));
        if (count > 0) this->Insert(ToUtf8({text, static_cast<size_t>(count)}));
    }
}
