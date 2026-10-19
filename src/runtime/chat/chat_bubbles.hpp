#pragma once

#include "chat_model.hpp"
#include <optional>

namespace xiui
{
    enum class bubble_channel : size_t { say, linkshell1, linkshell2, party, yell, shout, npc, count };
    inline constexpr const char* bubble_channel_keys[]{"say", "linkshell1", "linkshell2", "party", "yell", "shout", "npc"};
    inline constexpr const char* bubble_channel_names[]{"Say", "Linkshell 1", "Linkshell 2", "Party", "Yell", "Shout", "NPC dialogue"};
    struct bubble_message
    {
        bubble_channel channel{};
        std::string speaker;
        std::vector<chat_span> spans;
        bool continuation{};
    };
    auto BubbleChannel(uint32_t mode) -> std::optional<bubble_channel>;
    auto ParseBubbleMessage(const chat_line& line) -> std::optional<bubble_message>;
}
