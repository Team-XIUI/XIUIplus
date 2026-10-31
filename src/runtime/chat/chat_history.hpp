#pragma once

#include "design_system.hpp"

namespace xiui
{
    enum class chat_history_tab : size_t { window1, window2, say, tell, party, ls1, ls2, assist_e, assist_j, unity, shout, emotes, count };
    inline constexpr const char* chat_history_tabs[]{"Window 1", "Window 2", "Say", "Tell", "Party", "LS1", "LS2", "Assist E", "Assist J", "Unity", "Shout", "Emotes"};
    static_assert(std::size(chat_history_tabs) == static_cast<size_t>(chat_history_tab::count));

    inline auto ChatHistoryMatches(const size_t tab, const uint32_t raw, const uint32_t windows) -> bool
    {
        if (windows == 0) return false;
        const auto mode = raw & 255;
        switch (static_cast<chat_history_tab>(tab))
        {
        case chat_history_tab::window1: return (windows & 1) != 0;
        case chat_history_tab::window2: return (windows & 2) != 0;
        case chat_history_tab::say: return mode == 1 || mode == 9;
        case chat_history_tab::tell: return mode == 4 || mode == 12;
        case chat_history_tab::party: return mode == 5 || mode == 13 || mode == 210;
        case chat_history_tab::ls1: return mode == 6 || mode == 14;
        case chat_history_tab::ls2: return mode == 213 || mode == 214;
        case chat_history_tab::assist_e: return mode == 221 || mode == 222;
        case chat_history_tab::assist_j: return mode == 219 || mode == 220;
        case chat_history_tab::unity: return mode == 211 || mode == 212;
        case chat_history_tab::shout: return mode == 2 || mode == 10;
        case chat_history_tab::emotes: return mode == 7 || mode == 15;
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
