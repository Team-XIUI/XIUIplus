#pragma once

#include "chat/chat_encoding.hpp"

namespace xiui
{
    template<typename Lookup>
    auto FindActionResource(const std::string& gameName, Lookup&& lookup) -> decltype(lookup(gameName.c_str(), uint32_t{}))
    {
        if (!gameName.empty())
            for (const uint32_t language : {2u, 1u, 0u})
                if (auto* resource = lookup(gameName.c_str(), language)) return resource;
        return nullptr;
    }

    inline auto ActionResourceName(const char* const* names, const uint32_t preferred = 2) -> std::string
    {
        for (const auto language : {preferred, 2u, 1u, 0u})
            if (names[language] != nullptr && names[language][0] != '\0') return FromGameText(names[language]);
        return {};
    }
}
