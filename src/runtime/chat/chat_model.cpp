#include "chat_model.hpp"
#include "chat_encoding.hpp"
#include "chat_emphasis.hpp"

namespace
{
    auto IsAddonTimestamp(const xiui::chat_span& span) -> bool
    {
        if (span.color_index != 81) return false;
        const std::string_view text{span.text};
        if ((text.size() != 7 && text.size() != 10) || text.front() != '[' || text.back() != ']' || text[3] != ':') return false;
        const auto pair = [&](const size_t at, const int maximum) {
            return text[at] >= '0' && text[at] <= '9' && text[at + 1] >= '0' && text[at + 1] <= '9' &&
                (text[at] - '0') * 10 + text[at + 1] - '0' <= maximum;
        };
        return pair(1, 23) && pair(4, 59) && (text.size() == 7 || (text[6] == ':' && pair(7, 59)));
    }

    auto RemoveAddonTimestamps(std::vector<xiui::chat_span>& spans) -> void
    {
        bool lineStart = true, removeSpace = false;
        for (auto& span : spans)
        {
            if (removeSpace && !span.text.empty())
            {
                if (span.text.front() == ' ') span.text.erase(0, 1);
                removeSpace = false;
            }
            if (lineStart && IsAddonTimestamp(span))
            {
                span.text.clear();
                removeSpace = true;
                continue;
            }
            if (!span.text.empty()) lineStart = span.text.back() == '\n';
        }
        std::erase_if(spans, [](const auto& span) { return span.text.empty(); });
    }
}

namespace xiui
{
    auto ChatLocalSeconds(const uint64_t receivedTick, const uint64_t nowTick, const uint32_t localMilliseconds) -> uint32_t
    {
        constexpr uint32_t day = 86400000;
        const auto elapsed = nowTick - std::min(nowTick, receivedTick);
        return static_cast<uint32_t>((localMilliseconds + day - elapsed % day) % day) / 1000;
    }

    auto ChatTypeIndex(const uint32_t raw) -> size_t
    {
        switch (raw & 255)
        {
        case 12: return 0;
        case 4: return 1;
        case 1: case 9: return 2;
        case 5: case 13: case 210: return 3;
        case 6: case 14: return 4;
        case 213: case 214: case 215: case 216: return 5;
        case 2: case 10: return 6;
        case 3: case 11: return 7;
        case 211: case 212: return 8;
        case 7: case 15: return 9;
        default:
            switch (ChatCategory(raw))
            {
            case chat_tab::chat: return 10;
            case chat_tab::combat: return 11;
            default: return 12;
            }
        }
    }

    auto ChatCategory(const uint32_t raw) -> chat_tab
    {
        const auto mode = raw & 0xFF;
        switch (mode)
        {
        case 3: case 11: return chat_tab::chat;
        case 5: case 13: case 210: return chat_tab::party;
        case 4: case 12: return chat_tab::tell;
        case 6: case 14: case 213: case 214: case 215: case 216: return chat_tab::linkshell;
        case 1: case 2: case 7: case 9: case 10: case 15: case 208: case 211: case 212: case 219: case 220: case 221: case 222:
            return chat_tab::chat;
        default:
            return (mode >= 20 && mode <= 114) || (mode >= 162 && mode <= 189) ? chat_tab::combat : chat_tab::system;
        }
    }

    auto ChatTabMatches(const chat_tab tab, const uint32_t mode, const uint32_t windows) -> bool
    {
        if (windows == 0) return false;
        if (tab == chat_tab::all) return true;
        if (tab == chat_tab::window1) return (windows & 1) != 0;
        if (tab == chat_tab::window2) return (windows & 2) != 0;
        const auto category = ChatCategory(mode);
        return tab == category || (tab == chat_tab::chat &&
            (category == chat_tab::party || category == chat_tab::linkshell || category == chat_tab::tell));
    }

    auto ParseChatText(const std::string_view text) -> std::vector<chat_span>
    {
        std::vector<chat_span> spans;
        chat_span current;
        const auto flush = [&] { if (!current.text.empty()) { spans.push_back(current); current.text.clear(); } };
        for (size_t i = 0; i < text.size(); ++i)
        {
            const auto value = static_cast<uint8_t>(text[i]);
            if ((value == 0x1E || value == 0x1F) && i + 1 < text.size())
            {
                flush();
                const auto index = static_cast<uint8_t>(text[++i]);
                current.color_index = value == 0x1E && index == 1 ? -1 : index + (value == 0x1F ? 256 : 0);
            }
            else if (value == '\r' || value == 0x7F || value == 0x1E || value == 0x1F) continue;
            else if (value == '\n' || value == '\t' || value >= 0x20)
                current.text += text[i];
        }
        flush();
        return spans;
    }

    auto chat_model::Add(const chat_event& event, const std::string_view expanded) -> void
    {
        if (!this->lines_.empty() && this->lines_.back().sequence == event.sequence)
        {
            this->lines_.back().windows |= event.windows;
            ++this->revision_;
            return;
        }
        if (event.windows == 0) return;
        const auto mode = event.mode & 255;
        const bool npc = mode == 150 || mode == 151 || mode == 190;
        const auto control = GameTextDialogueControl(event.text);
        // Native NPC redraws have already consumed the continuation control.
        const bool prompt = npc && (control == dialogue_control::manual ||
            (mode == 190 && control == dialogue_control::absent));
        if (npc || mode == 142 || mode == 144)
            this->dialogue_prompt_sequence_ = prompt ? event.sequence : 0;
        if (expanded.empty()) return;
        auto spans = ParseChatText(expanded);
        RemoveAddonTimestamps(spans);
        EmphasizeChatText(spans, event.mode);
        if (spans.empty()) return;
        if (event.indent) spans.insert(spans.begin(), chat_span{"  ", -1});
        const auto tick = ::GetTickCount64();
        SYSTEMTIME clock{}; ::GetLocalTime(&clock);
        const auto milliseconds = ((clock.wHour * 60u + clock.wMinute) * 60u + clock.wSecond) * 1000u + clock.wMilliseconds;
        this->lines_.push_back({event.sequence, event.timestamp, event.mode, event.color, event.windows, std::move(spans), prompt,
            ChatLocalSeconds(event.timestamp, tick, milliseconds)});
        while (this->lines_.size() > chat_history_capacity) this->lines_.pop_front();
        ++this->revision_;
    }
} // namespace xiui
