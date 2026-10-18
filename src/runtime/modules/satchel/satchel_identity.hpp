#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace xiui
{
    struct satchel_server_endpoint
    {
        // Address uses display order: 0x01020304 represents 1.2.3.4.
        uint32_t process{}, address{};
        uint16_t port{};
        bool established{};
    };

    auto SatchelStableIdentity(std::string_view identity, uint32_t character = 0) -> bool;
    auto SatchelServerIdentity(std::span<const satchel_server_endpoint> endpoints, uint32_t process, uint32_t character) -> std::optional<std::string>;
    auto ReadSatchelServerIdentity(uint32_t character) -> std::optional<std::string>;
}
