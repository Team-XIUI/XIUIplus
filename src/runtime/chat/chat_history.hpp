#pragma once

#include "design_system.hpp"

namespace xiui
{
    inline constexpr const char* chat_history_tabs[]{"Window 1", "Say", "Tell", "Party", "LS1", "LS2", "Assist E", "Assist J", "Unity", "Shout"};

    inline auto ChatHistoryMatches(const size_t tab, const uint32_t raw, const uint32_t windows) -> bool
    {
        if (windows == 0) return false;
        const auto mode = raw & 255;
        switch (tab)
        {
        case 0: return (windows & 1) != 0;
        case 1: return mode == 1 || mode == 9;
        case 2: return mode == 4 || mode == 12;
        case 3: return mode == 5 || mode == 13 || mode == 210;
        case 4: return mode == 6 || mode == 14;
        case 5: return mode == 213 || mode == 214;
        case 6: return mode == 221 || mode == 222;
        case 7: return mode == 219 || mode == 220;
        case 8: return mode == 211 || mode == 212;
        case 9: return mode == 2 || mode == 10;
        default: return false;
        }
    }

    struct chat_history_view
    {
        bool focused{}, expanded{};
        size_t tab{};
        std::array<size_t, std::size(chat_history_tabs)> scroll{};
        ui::rect viewport{0, 0, 1280, 720};
        ui::point pointer{-1, -1};
    };
}
