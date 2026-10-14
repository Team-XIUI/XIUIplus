#include "slider_value_input.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace xiui::ui
{
    auto slider_value_input::Format(const float value, const float step) -> std::string
    {
        int precision = 0;
        double scaled = step > 0 ? step : 0.000001;
        while (precision < 6 && std::abs(scaled - std::round(scaled)) > std::max(0.000001, std::abs(scaled) * 0.000001))
        { scaled *= 10; ++precision; }
        char text[48]{};
        std::snprintf(text, sizeof(text), "%.*f", precision, value);
        return text;
    }

    auto slider_value_input::Normalize(const float value, const float minimum, const float maximum, const float step) -> float
    {
        double result = std::clamp(value, minimum, maximum);
        if (step > 0) result = minimum + std::round((result - minimum) / step) * step;
        return static_cast<float>(std::clamp(result, static_cast<double>(minimum), static_cast<double>(maximum)));
    }

    auto slider_value_input::Open(const module_option& option) -> void
    {
        this->option_ = option;
        this->focused_ = option.available && option.kind == module_option_kind::slider && option.slider != nullptr;
        this->invalid_ = false;
        if (!this->focused_) return;
        this->editor_.Set(Format(*option.slider * option.display_scale, option.step * option.display_scale));
        this->editor_.SelectAll();
    }

    auto slider_value_input::Insert(std::string_view text) -> void
    {
        while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\r' || text.front() == '\n')) text.remove_prefix(1);
        while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r' || text.back() == '\n')) text.remove_suffix(1);
        if (text.empty() || !std::all_of(text.begin(), text.end(), [](const char c) {
            return (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E';
        })) return;
        const auto [begin, end] = this->editor_.Selection();
        if (this->editor_.Text().size() - (end - begin) + text.size() > 31) return;
        this->editor_.Insert(text);
        this->invalid_ = false;
    }

    auto slider_value_input::Value(void) -> std::optional<float>
    {
        auto text = std::string_view{this->editor_.Text()};
        if (text.starts_with('+')) text.remove_prefix(1);
        float value{};
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
        this->invalid_ = text.empty() || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || !std::isfinite(value);
        if (this->invalid_) return std::nullopt;
        return Normalize(value / this->option_.display_scale, this->option_.minimum, this->option_.maximum, this->option_.step);
    }

    auto slider_value_input::Copy(const HWND window) const -> bool
    {
        const auto text = this->editor_.SelectedText();
        if (text.empty() || !::OpenClipboard(window)) return false;
        bool copied = false;
        const auto memory = ::GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t));
        if (memory != nullptr)
        {
            if (auto* buffer = static_cast<wchar_t*>(::GlobalLock(memory)); buffer != nullptr)
            {
                for (size_t i = 0; i < text.size(); ++i) buffer[i] = static_cast<unsigned char>(text[i]);
                buffer[text.size()] = 0;
                ::GlobalUnlock(memory);
                ::EmptyClipboard();
                copied = ::SetClipboardData(CF_UNICODETEXT, memory) != nullptr;
                if (!copied) ::GlobalFree(memory);
            }
            else ::GlobalFree(memory);
        }
        ::CloseClipboard();
        return copied;
    }

    auto slider_value_input::Paste(const HWND window) -> void
    {
        if (!::OpenClipboard(window)) return;
        const auto memory = ::GetClipboardData(CF_UNICODETEXT);
        if (memory != nullptr)
        {
            if (const auto* buffer = static_cast<const wchar_t*>(::GlobalLock(memory)); buffer != nullptr)
            {
                const auto capacity = std::min<size_t>(::GlobalSize(memory) / sizeof(wchar_t), 64);
                std::string text;
                size_t index{};
                for (; index < capacity && buffer[index] != 0 && buffer[index] < 128; ++index) text += static_cast<char>(buffer[index]);
                if (index < capacity && buffer[index] == 0) this->Insert(text);
                ::GlobalUnlock(memory);
            }
        }
        ::CloseClipboard();
    }

    auto slider_value_input::HandleKey(const WPARAM key, const LPARAM flags, const HWND window) -> void
    {
        const bool control = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool shift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
        const bool alt = (::GetKeyState(VK_MENU) & 0x8000) != 0 || (static_cast<uint32_t>(flags) & 0x20000000u) != 0;
        this->invalid_ = false;
        if (alt) return;
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
            else if (key == 'C' || key == 'X')
            {
                if (this->Copy(window) && key == 'X') this->editor_.Erase(true);
            }
            else if (key == 'V') this->Paste(window);
            return;
        }
        BYTE state[256]{};
        ::GetKeyboardState(state);
        if (key < 256) state[key] |= 0x80;
        wchar_t text[8]{};
        const auto count = ::ToUnicodeEx(static_cast<UINT>(key), (static_cast<uint32_t>(flags) >> 16) & 0xFF, state, text, 8, 4, ::GetKeyboardLayout(0));
        std::string number;
        for (int i = 0; i < count; ++i) if (text[i] < 128) number += static_cast<char>(text[i]);
        this->Insert(number);
    }
}
