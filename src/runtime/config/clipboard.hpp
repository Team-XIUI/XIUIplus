#pragma once

#include "chat/chat_encoding.hpp"

#include <optional>
#include <algorithm>
#include <cstring>

namespace xiui::ui
{
    inline auto CopyClipboardText(const HWND window, const std::string_view text) -> bool
    {
        const auto wide = FromUtf8(text);
        if (wide.empty() || !::OpenClipboard(window)) return false;
        bool copied{};
        const auto memory = ::GlobalAlloc(GMEM_MOVEABLE, (wide.size() + 1) * sizeof(wchar_t));
        if (memory != nullptr)
        {
            if (auto* buffer = ::GlobalLock(memory))
            {
                std::memcpy(buffer, wide.c_str(), (wide.size() + 1) * sizeof(wchar_t)); ::GlobalUnlock(memory);
                copied = ::EmptyClipboard() && ::SetClipboardData(CF_UNICODETEXT, memory) != nullptr;
            }
            if (!copied) ::GlobalFree(memory);
        }
        ::CloseClipboard();
        return copied;
    }

    inline auto ReadClipboardText(const HWND window, const size_t limit) -> std::optional<std::string>
    {
        if (!::OpenClipboard(window)) return std::nullopt;
        std::optional<std::string> result;
        if (const auto memory = ::GetClipboardData(CF_UNICODETEXT))
        {
            if (const auto* text = static_cast<const wchar_t*>(::GlobalLock(memory)))
            {
                const auto capacity = std::min<size_t>(::GlobalSize(memory) / sizeof(wchar_t), limit + 1);
                const auto length = wcsnlen_s(text, capacity);
                if (length < capacity) { auto value = ToUtf8({text, length}); if (value.size() <= limit) result = std::move(value); }
                ::GlobalUnlock(memory);
            }
        }
        ::CloseClipboard();
        return result;
    }
}
