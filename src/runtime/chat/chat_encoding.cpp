#include "chat_encoding.hpp"
#include "chat_data.hpp"
#include "chat_token.hpp"

#include <array>
#include <cstring>

namespace
{
    auto Decode(const std::string_view text, const UINT codepage) -> std::wstring
    {
        if (text.empty()) return {};
        const auto length = ::MultiByteToWideChar(codepage, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (length <= 0) return {};
        std::wstring result(length, L'\0');
        ::MultiByteToWideChar(codepage, 0, text.data(), static_cast<int>(text.size()), result.data(), length);
        return result;
    }
}

namespace xiui
{
    auto ToUtf8(const std::wstring_view text) -> std::string
    {
        if (text.empty()) return {};
        const auto length = ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        if (length <= 0) return {};
        std::string result(length, '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), length, nullptr, nullptr);
        return result;
    }

    auto FromUtf8(const std::string_view text) -> std::wstring { return Decode(text, CP_UTF8); }

    auto GameTextDialogueControl(const std::string_view text) -> dialogue_control
    {
        auto control = dialogue_control::absent;
        for (size_t at = 0; at < text.size(); ++at)
        {
            if (const auto token = ChatTokenSize(text, at); token != 0) { at += token - 1; continue; }
            const auto byte = static_cast<uint8_t>(text[at]);
            if ((byte == 0x1E || byte == 0x1F) && at + 1 < text.size()) ++at;
            else if (byte == 0x7F && at + 1 < text.size())
            {
                const auto code = static_cast<uint8_t>(text[++at]);
                if (code >= 0x31 && code <= 0x37)
                    control = code <= 0x33 || code == 0x37 ? dialogue_control::manual : dialogue_control::timed;
                if (code >= 0x34 && code <= 0x36 && at + 1 < text.size()) ++at;
            }
            else if (::IsDBCSLeadByteEx(932, byte) && at + 1 < text.size()) ++at;
        }
        return control;
    }

    auto FromGameText(const std::string_view text) -> std::string
    {
        std::string result;
        size_t start{};
        for (size_t i = 0; i < text.size(); ++i)
        {
            const auto byte = static_cast<uint8_t>(text[i]);
            if (byte == 0xEF && i + 1 < text.size() && (text[i + 1] == 0x27 || text[i + 1] == 0x28))
            {
                result += ToUtf8(Decode(text.substr(start, i - start), 932));
                result += text[i + 1] == 0x28 ? chat_cap_close : chat_cap_open;
                start = i + 2; ++i;
                continue;
            }
            if (byte == 0x7F && i + 1 < text.size())
            {
                result += ToUtf8(Decode(text.substr(start, i - start), 932));
                start = i + 2; ++i;
                continue;
            }
            if ((byte == 0x1E || byte == 0x1F) && i + 1 < text.size())
            {
                result += ToUtf8(Decode(text.substr(start, i - start), 932));
                result.append(text.substr(i, 2));
                start = i + 2;
                ++i;
            }
            else if (::IsDBCSLeadByteEx(932, byte) && i + 1 < text.size()) ++i;
        }
        result += ToUtf8(Decode(text.substr(start), 932));
        return result;
    }

    auto ToGameText(const std::string_view text, std::string& output) -> bool
    {
        output.clear();
        const auto append = [&output](const std::string_view plain) {
            if (plain.empty()) return true;
            const auto wide = FromUtf8(plain);
            if (wide.empty()) return false;
            BOOL substituted{};
            const auto length = ::WideCharToMultiByte(932, WC_NO_BEST_FIT_CHARS, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, &substituted);
            if (length <= 0 || substituted) return false;
            const auto start = output.size();
            output.resize(start + length);
            ::WideCharToMultiByte(932, WC_NO_BEST_FIT_CHARS, wide.data(), static_cast<int>(wide.size()), output.data() + start, length, nullptr, &substituted);
            return !substituted;
        };
        size_t start{};
        for (size_t at = 0; at < text.size();)
        {
            if (const auto token = ChatTokenSize(text, at); token != 0)
            {
                if (!append(text.substr(start, at - start))) return false;
                output.append(text.substr(at, token)); at += token; start = at;
            }
            else ++at;
        }
        return append(text.substr(start));
    }

    auto ExpandChatText(IChatManager* chat, const char* text) -> std::string
    {
        if (text == nullptr) return {};
        std::array<char, chat_text_capacity * 2> expanded{};
        if (chat != nullptr)
        {
            chat->ParseAutoTranslate(text, expanded.data(), static_cast<int32_t>(expanded.size()), true);
            expanded.back() = '\0';
        }
        return FromGameText(expanded[0] != '\0' ? expanded.data() : text);
    }
}
