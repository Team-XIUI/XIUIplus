#pragma once

#include "config_layout.hpp"

#include <cctype>
#include <string>
#include <utility>
#include <vector>

namespace xiui::ui
{
    struct settings_search_entry
    {
        std::string module, label, path, description;
        size_t view{}, section{}, option{}, appearance_tab{};
        int display_view{};
        module_option_group group{};
        std::vector<std::pair<size_t, int>> selectors;
        std::string label_terms, path_terms, description_terms;
    };

    inline auto SettingsSearchText(const std::string_view text) -> std::string
    {
        std::string result;
        for (const unsigned char c : text)
            result += c >= 128 || std::isalnum(c) ? static_cast<char>(std::tolower(c)) : ' ';
        return result;
    }

    inline auto FindSettings(const std::vector<settings_search_entry>& entries, const std::string_view query) -> std::vector<size_t>
    {
        const auto normalized = SettingsSearchText(query);
        std::vector<std::string> words;
        for (size_t at = 0; at < normalized.size();)
        {
            at = normalized.find_first_not_of(' ', at);
            if (at == std::string::npos) break;
            const auto end = normalized.find(' ', at);
            words.push_back(normalized.substr(at, end == std::string::npos ? end : end - at));
            at = end == std::string::npos ? normalized.size() : end;
        }
        std::vector<std::pair<int, size_t>> ranked;
        if (words.empty()) return {};
        for (size_t index = 0; index < entries.size(); ++index)
        {
            const auto& entry = entries[index];
            int score{};
            for (const auto& word : words)
            {
                if (entry.label_terms.find(word) != std::string::npos) score += 8;
                else if (entry.path_terms.find(word) != std::string::npos) score += 4;
                else if (entry.description_terms.find(word) != std::string::npos) ++score;
                else { score = 0; break; }
            }
            if (score != 0) ranked.emplace_back(score, index);
        }
        std::stable_sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
        std::vector<size_t> result; result.reserve(ranked.size());
        for (const auto& [score, index] : ranked) result.push_back(index);
        return result;
    }

    struct settings_search_layout
    {
        rect input{}, panel{}, footer{};
        size_t rows{};
        explicit settings_search_layout(const config_layout& layout, const rect& window)
        {
            this->input = layout.SearchInput();
            this->rows = static_cast<size_t>(std::clamp(static_cast<int>((window.height - 76) / 44), 1, 7));
            const float width = std::min(600.0f, window.width - 16);
            this->panel = {std::clamp(this->input.x + this->input.width - width, window.x + 8, window.x + window.width - width - 8),
                this->input.y + this->input.height + 5, width, this->rows * 44.0f + 30};
            this->footer = {this->panel.x + 8, this->panel.y + this->panel.height - 25, width - 16, 20};
        }
        auto Row(const size_t index) const -> rect { return {this->panel.x + 4, this->panel.y + 4 + index * 44, this->panel.width - 8, 42}; }
    };
}
