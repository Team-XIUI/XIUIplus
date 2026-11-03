#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace xiui::ui
{
    // Keep Latin words together and allow breaks between UTF-8 Japanese glyphs.
    template<typename Measure>
    auto WrapText(const std::string_view text, const float width, Measure measure) -> std::vector<std::string>
    {
        std::vector<std::string> lines;
        std::string line;
        bool space{};
        for (size_t at = 0; at < text.size();)
        {
            const auto begin = at;
            const auto byte = static_cast<unsigned char>(text[at++]);
            if (byte == '\n') { lines.push_back(std::move(line)); line.clear(); space = false; continue; }
            if (byte == ' ' || byte == '\t' || byte == '\r') { space = !line.empty(); continue; }
            if (byte >= 0x80)
                while (at < text.size() && (static_cast<unsigned char>(text[at]) & 0xC0) == 0x80) ++at;
            else
                while (at < text.size() && static_cast<unsigned char>(text[at]) > ' ' && static_cast<unsigned char>(text[at]) < 0x80) ++at;
            const std::string token{text.substr(begin, at - begin)};
            auto next = line + (space ? " " : "") + token;
            if (!line.empty() && measure(next.c_str()) > width)
            { lines.push_back(std::move(line)); line.clear(); next = token; }
            line = std::move(next); space = false;
        }
        if (!line.empty()) lines.push_back(std::move(line));
        return lines;
    }
}
