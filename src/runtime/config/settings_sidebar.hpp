#pragma once

#include "design_system.hpp"

#include <algorithm>
#include <string>
#include <unordered_map>

namespace xiui::ui
{
    struct settings_sidebar_animation
    {
        struct transition
        {
            float from{}, to{};
            uint64_t started{};
            auto Value(const uint64_t now) const -> float
            {
                const float time = std::clamp(static_cast<float>(now >= this->started ? now - this->started : 0) / 160.0f, 0.0f, 1.0f);
                const float ease = 1 - (1 - time) * (1 - time) * (1 - time);
                return this->from + (this->to - this->from) * ease;
            }
        };
        std::unordered_map<std::string, transition> pages;
        std::string active;
        float scroll{}, content_height{};
        bool reveal{};

        auto Select(const std::string& id, const uint64_t now) -> void
        {
            if (this->active == id) return;
            this->pages.try_emplace(id);
            for (auto& [key, value] : this->pages) value = {value.Value(now), key == id ? 1.0f : 0.0f, now};
            this->active = id;
            this->reveal = true;
        }
        auto Expansion(const std::string& id, const uint64_t now) const -> float
        {
            const auto found = this->pages.find(id);
            return found == this->pages.end() ? 0 : found->second.Value(now);
        }
    };

    struct settings_sidebar_row
    {
        std::string id;
        const char* label{};
        rect bounds{};
        size_t page{}, section{}, item{};
        bool child{}, selected{}, available{true};
        int subview{-1};
        const char* help{};
    };
}
