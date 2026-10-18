#include "satchel_identity.hpp"

#include <winsock2.h>
#include <iphlpapi.h>

#include <charconv>
#include <cstdio>
#include <vector>

#pragma comment(lib, "iphlpapi.lib")

namespace xiui
{
    namespace
    {
        auto Decimal(const std::string_view text, uint32_t& result) -> bool
        {
            const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
            return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
        }

        auto Address(const std::string_view text) -> bool
        {
            uint32_t address{};
            const auto parsed = std::from_chars(text.data(), text.data() + text.size(), address, 16);
            return text.size() == 8 && parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && address != 0 && address != UINT32_MAX;
        }
    }

    auto SatchelStableIdentity(const std::string_view identity, const uint32_t character) -> bool
    {
        uint32_t id{};
        if (identity.starts_with("world-"))
        {
            const auto split = identity.find('-', 6);
            uint32_t world{};
            return split != std::string_view::npos && Decimal(identity.substr(6, split - 6), world) && world <= 65535 &&
                Decimal(identity.substr(split + 1), id) && id != 0 && (character == 0 || character == id);
        }
        constexpr std::string_view prefix = "server-", separator = "-54231-";
        return identity.starts_with(prefix) && identity.size() > prefix.size() + 8 + separator.size() &&
            Address(identity.substr(prefix.size(), 8)) && identity.substr(prefix.size() + 8, separator.size()) == separator &&
            Decimal(identity.substr(prefix.size() + 8 + separator.size()), id) && id != 0 && (character == 0 || character == id);
    }

    auto SatchelServerIdentity(const std::span<const satchel_server_endpoint> endpoints, const uint32_t process, const uint32_t character) -> std::optional<std::string>
    {
        if (process == 0 || character == 0) return {};
        uint32_t matched{};
        for (const auto& auth : endpoints)
        {
            if (auth.process != process || !auth.established || auth.port != 54231 || auth.address == 0 || auth.address == UINT32_MAX) continue;
            for (const auto& data : endpoints)
            {
                if (data.process != process || !data.established || data.port != 54230 || data.address != auth.address) continue;
                if (matched != 0 && matched != auth.address) return {};
                matched = auth.address;
            }
        }
        if (matched == 0) return {};
        char address[9]{};
        std::snprintf(address, sizeof(address), "%08X", matched);
        return "server-" + std::string{address} + "-54231-" + std::to_string(character);
    }

    auto ReadSatchelServerIdentity(const uint32_t character) -> std::optional<std::string>
    {
        if (character == 0) return {};
        DWORD bytes{};
        if (::GetExtendedTcpTable(nullptr, &bytes, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) != ERROR_INSUFFICIENT_BUFFER ||
            bytes < sizeof(MIB_TCPTABLE_OWNER_PID) || bytes > 4 * 1024 * 1024) return {};
        std::vector<uint8_t> buffer;
        DWORD result = ERROR_INSUFFICIENT_BUFFER;
        for (int attempt = 0; attempt < 2 && result == ERROR_INSUFFICIENT_BUFFER; ++attempt)
        {
            if (bytes > 4 * 1024 * 1024) return {};
            buffer.resize(bytes);
            result = ::GetExtendedTcpTable(buffer.data(), &bytes, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
        }
        if (result != NO_ERROR || bytes < sizeof(DWORD) || bytes > buffer.size()) return {};
        const auto* table = reinterpret_cast<const MIB_TCPTABLE_OWNER_PID*>(buffer.data());
        if (table->dwNumEntries > (bytes - sizeof(DWORD)) / sizeof(MIB_TCPROW_OWNER_PID)) return {};
        const auto process = ::GetCurrentProcessId();
        std::vector<satchel_server_endpoint> endpoints;
        for (DWORD index = 0; index < table->dwNumEntries; ++index)
        {
            const auto& row = table->table[index];
            if (row.dwOwningPid != process || row.dwState != MIB_TCP_STATE_ESTAB) continue;
            const auto port = static_cast<uint16_t>(((row.dwRemotePort & 0xFF) << 8) | ((row.dwRemotePort >> 8) & 0xFF));
            if (port != 54230 && port != 54231) continue;
            const auto raw = row.dwRemoteAddr;
            const auto address = ((raw & 0xFF) << 24) | ((raw & 0xFF00) << 8) | ((raw >> 8) & 0xFF00) | ((raw >> 24) & 0xFF);
            endpoints.push_back({process, address, port, true});
        }
        return SatchelServerIdentity(endpoints, process, character);
    }
}
