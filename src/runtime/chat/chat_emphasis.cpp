#include "chat_emphasis.hpp"

#include <algorithm>
#include <array>

namespace
{
    struct field { size_t begin{}, end{}; };

    // Native English message fields; other languages retain native inline emphasis.
    constexpr std::string_view messages[]{
        "{} hits {} for {n} points of damage.", "{} hits {} for {n} point of damage.",
        "{} misses {}.", "{} scores a critical hit!",
        "{} takes {n} points of damage.", "{} takes {n} point of damage.",
        "{} starts casting {} on {}.", "{} starts casting {}.",
        "{} begins casting {} on {}.", "{} begins casting {}.",
        "{} casts {} on {}.", "{} casts {}.",
        "{} uses {} on {}.", "{} uses {}.", "{} readies {}.",
        "{} recovers {n} HP.", "{} recovers {n} MP.",
        "{} defeats {}.", "{} is defeated by {}.",
        "{}'s attack misses {}.", "{}'s casting is interrupted.",
        "{} parries {}'s attack with his weapon.", "{} parries {}'s attack with her weapon.",
        "{} blocks {}'s attack with his shield.", "{} blocks {}'s attack with her shield.",
        "{} obtains: {}.", "{} obtains {}.", "Obtained key item: {}.", "Obtained: {}.",
        "You find {} on {}.", "{} finds {} on {}.",
        "{} learns the spell {}.", "{} learns \"{}\"!", "{} learns {}!",
        "Skillchain: {}.", "{} gains {n} experience points.", "{} gains {n} limit points."};

    auto Number(const std::string_view text) -> bool
    {
        return !text.empty() && text.front() >= '0' && text.front() <= '9' &&
            std::all_of(text.begin(), text.end(), [](const char c) { return (c >= '0' && c <= '9') || c == ',' || c == '.'; });
    }

    auto Match(const std::string_view text, const size_t start, const std::string_view pattern,
        std::array<field, 4>& fields, size_t& count) -> size_t
    {
        size_t at = start, token{}; count = 0;
        while (token < pattern.size())
        {
            if (pattern[token] != '{')
            {
                const auto end = pattern.find('{', token);
                const auto literal = pattern.substr(token, end - token);
                if (!text.substr(at).starts_with(literal)) return start;
                at += literal.size(); token += literal.size();
                continue;
            }
            const auto close = pattern.find('}', token);
            const auto next = pattern.find('{', close + 1);
            const auto literal = pattern.substr(close + 1, next == std::string_view::npos ? next : next - close - 1);
            const auto end = text.find(literal, at);
            if (end == std::string_view::npos || end <= at || end - at > 256 || count == fields.size()) return start;
            const auto value = text.substr(at, end - at);
            if (value.find_first_of("\r\n.!?") != std::string_view::npos ||
                (pattern[token + 1] == 'n' && !Number(value))) return start;
            fields[count++] = {at, end};
            at = end; token = close + 1;
        }
        return at;
    }

    auto Mark(std::vector<bool>& emphasis, size_t begin, size_t end, const std::string_view text) -> void
    {
        while (begin < end && (text[begin] == ' ' || text[begin] == '\t')) ++begin;
        while (end > begin && (text[end - 1] == ' ' || text[end - 1] == '\t')) --end;
        std::fill(emphasis.begin() + begin, emphasis.begin() + end, true);
    }
}

namespace xiui
{
    auto EmphasizeChatText(std::vector<chat_span>& spans, const uint32_t mode) -> void
    {
        std::string text;
        for (const auto& span : spans) text += span.text;
        std::vector<bool> emphasis(text.size());
        const auto category = ChatCategory(mode);
        const auto channel = mode & 255;
        const bool npc = channel == 150 || channel == 151 || channel == 190;
        const bool messagesOnly = !npc && (category == chat_tab::combat || category == chat_tab::system);
        size_t offset{};
        for (const auto& span : spans)
        {
            // Palette 1 contains inline fields; palette 2 selects whole message/channel colors.
            if (span.color_index > 1 && span.color_index < 256 && span.color_index != 81 && span.text.size() < text.size())
                Mark(emphasis, offset, offset + span.text.size(), text);
            offset += span.text.size();
        }
        if (messagesOnly)
        {
            for (size_t at = 0; at < text.size();)
            {
                if (text[at] == ' ' || text[at] == '\n' || text[at] == '\t') { ++at; continue; }
                size_t matched = at;
                for (const auto pattern : messages)
                {
                    std::array<field, 4> fields{}; size_t count{};
                    matched = Match(text, at, pattern, fields, count);
                    if (matched == at) continue;
                    for (size_t i = 0; i < count; ++i) Mark(emphasis, fields[i].begin, fields[i].end, text);
                    break;
                }
                if (matched > at) { at = matched; continue; }
                const auto end = text.find_first_of(".!?\n", at);
                at = end == std::string::npos ? text.size() : end + 1;
            }
            for (size_t at = 0; at < text.size(); ++at)
            {
                const auto c = static_cast<unsigned char>(text[at]);
                if (c < '0' || c > '9') continue;
                const auto begin = at;
                while (at + 1 < text.size() && ((text[at + 1] >= '0' && text[at + 1] <= '9') ||
                    ((text[at + 1] == ',' || text[at + 1] == '.') && at + 2 < text.size() && text[at + 2] >= '0' && text[at + 2] <= '9'))) ++at;
                const auto word = [](const unsigned char c) { return c >= 128 || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; };
                if ((begin == 0 || !word(text[begin - 1])) && (at + 1 == text.size() || !word(text[at + 1])))
                    Mark(emphasis, begin, at + 1, text);
            }
        }
        else
        {
            // Native speaker prefixes are outside the authored message body.
            auto begin = text.find_first_not_of(" \t");
            if (begin != std::string::npos)
            {
                if (text.compare(begin, 2, ">>") == 0) begin += 2;
                if (text[begin] == '[')
                {
                    const auto close = text.find(']', begin);
                    const auto after = close == std::string::npos ? close : text.find_first_not_of(" \t", close + 1);
                    if (after != std::string::npos && (text[after] == '(' || text[after] == '<')) begin = after;
                }
                const char open = text[begin];
                const bool enclosed = open == '(' || open == '<' || open == '[';
                const auto end = enclosed ? text.find(open == '(' ? ')' : open == '<' ? '>' : ']', ++begin) : text.find_first_of(":>", begin);
                if (end != std::string::npos && end - begin <= 64)
                {
                    auto stop = text.find_first_of(")]<[", begin);
                    Mark(emphasis, begin, std::min(end, stop), text);
                }
            }
        }
        std::vector<chat_span> result;
        offset = 0;
        for (const auto& span : spans)
        {
            for (size_t begin = 0; begin < span.text.size();)
            {
                size_t end = begin + 1;
                const bool bold = emphasis[offset + begin];
                while (end < span.text.size() && emphasis[offset + end] == bold) ++end;
                result.push_back({span.text.substr(begin, end - begin), span.color_index, bold});
                begin = end;
            }
            offset += span.text.size();
        }
        spans = std::move(result);
    }
}
