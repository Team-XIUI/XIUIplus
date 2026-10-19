#include "chat_bubbles.hpp"

namespace
{
    auto Trim(std::string_view text) -> std::string_view
    {
        const auto first = text.find_first_not_of(" \t\r\n");
        return first == std::string_view::npos ? std::string_view{} : text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
    }
}

namespace xiui
{
    auto BubbleChannel(const uint32_t mode) -> std::optional<bubble_channel>
    {
        switch (mode & 255)
        {
        case 1: case 9: return bubble_channel::say;
        case 6: case 14: return bubble_channel::linkshell1;
        case 213: case 214: return bubble_channel::linkshell2;
        case 5: case 13: case 210: return bubble_channel::party;
        case 3: case 11: return bubble_channel::yell;
        case 2: case 10: return bubble_channel::shout;
        case 150: case 151: case 190: return bubble_channel::npc;
        default: return std::nullopt;
        }
    }

    auto ParseBubbleMessage(const chat_line& line) -> std::optional<bubble_message>
    {
        const auto channel = BubbleChannel(line.mode);
        if (!channel) return std::nullopt;
        std::string plain;
        for (const auto& span : line.spans) plain += span.text;
        auto body = Trim(plain);
        if (body.empty()) return std::nullopt;
        const bool npc = *channel == bubble_channel::npc;
        bubble_message result{*channel};
        if (!npc && body.starts_with('['))
        {
            const auto close = body.find(']');
            if (close != std::string_view::npos && close < 32)
            {
                const auto after = Trim(body.substr(close + 1));
                if (after.starts_with('<') || after.starts_with('(')) body = after;
            }
        }
        if (npc && plain.starts_with("  ")) result.continuation = true;
        else if (!npc && (body.starts_with('<') || body.starts_with('(') || body.starts_with('[')))
        {
            const auto close = body.find(body.front() == '<' ? '>' : body.front() == '(' ? ')' : ']');
            if (close == std::string_view::npos || close > 64) return std::nullopt;
            result.speaker = Trim(body.substr(1, close - 1));
            body = Trim(body.substr(close + 1));
            if (body.starts_with(':')) body = Trim(body.substr(1));
        }
        else if (const auto colon = body.find(':'); colon != std::string_view::npos && colon <= 64)
        {
            auto name = Trim(body.substr(0, colon));
            if (!npc) name = Trim(name.substr(0, name.find('[')));
            result.speaker = name;
            body = Trim(body.substr(colon + 1));
        }
        else if (!npc) return std::nullopt;
        else result.continuation = true;
        if (body.empty() || (!result.continuation && result.speaker.empty())) return std::nullopt;
        const auto first = static_cast<size_t>(body.data() - plain.data());
        const auto end = first + body.size();
        size_t at{};
        for (const auto& span : line.spans)
        {
            const auto begin = std::max(first, at), stop = std::min(end, at + span.text.size());
            if (stop > begin) result.spans.push_back({span.text.substr(begin - at, stop - begin), span.color_index});
            at += span.text.size();
        }
        return result;
    }
}
