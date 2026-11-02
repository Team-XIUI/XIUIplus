#pragma once

#include "native_chat_input.hpp"
#include <algorithm>
#include <cctype>

namespace xiui
{
    struct chat_destination { std::string label; uint32_t mode{}; bool reply{}; };

    inline auto ChatDestinationTag(const chat_destination& destination) -> const char*
    {
        switch (destination.mode)
        {
        case 1: return "SAY";
        case 5: return "PTY";
        case 6: return "LS1";
        case 213: return "LS2";
        case 211: return "UNI";
        case 2: return "SH";
        case 3: return "YEL";
        case 4: return destination.reply ? "REP" : "TEL";
        case 7: return "EMO";
        case 221: return "ASE";
        case 219: return "ASJ";
        default: return destination.label.starts_with("Command:") ? "CMD" : "GME";
        }
    }

    inline auto DescribeChatDestination(std::string_view command, const std::string_view reply = {}) -> chat_destination
    {
        const auto start = command.find_first_not_of(" \t");
        if (start == std::string_view::npos) return {"Game"};
        command.remove_prefix(start);
        const auto end = command.find_first_of(" \t");
        std::string name{command.substr(0, end)};
        std::transform(name.begin(), name.end(), name.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (name == "/s" || name == "/say") return {"Say", 1};
        if (name == "/p" || name == "/party") return {"Party", 5};
        if (name == "/l" || name == "/linkshell") return {"Linkshell 1", 6};
        if (name == "/l2" || name == "/linkshell2") return {"Linkshell 2", 213};
        if (name == "/u" || name == "/unity") return {"Unity", 211};
        if (name == "/sh" || name == "/shout") return {"Shout", 2};
        if (name == "/y" || name == "/yell") return {"Yell", 3};
        if (name == "/em" || name == "/emote") return {"Emote", 7};
        if (name == "/ae" || name == "/assiste") return {"Assist E", 221};
        if (name == "/aj" || name == "/assistj") return {"Assist J", 219};
        if (name == "/r" || name == "/reply") return {IsTellRecipient(reply) ? "Tell: " + std::string{reply} : "Reply", 4, true};
        if (name == "/t" || name == "/tell")
        {
            const auto recipientStart = end == std::string_view::npos ? end : command.find_first_not_of(" \t", end);
            const auto recipient = recipientStart == std::string_view::npos ? std::string_view{} :
                command.substr(recipientStart, command.find_first_of(" \t", recipientStart) - recipientStart);
            return {IsTellRecipient(recipient) ? "Tell: " + std::string{recipient} : "Tell", 4};
        }
        return {name.starts_with('/') ? "Command: " + std::string{command.substr(0, end)} : "Game"};
    }
}
