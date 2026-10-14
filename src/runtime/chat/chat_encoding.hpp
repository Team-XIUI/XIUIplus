#pragma once

#include "Ashita.h"
#include <string>
#include <string_view>

namespace xiui
{
    auto ToUtf8(std::wstring_view text) -> std::string;
    auto FromUtf8(std::string_view text) -> std::wstring;
    auto FromGameText(std::string_view text) -> std::string;
    auto ToGameText(std::string_view text, std::string& output) -> bool;
    auto ExpandChatText(IChatManager* chat, const char* text) -> std::string;
}
