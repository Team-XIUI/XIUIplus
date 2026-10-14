#include "native_window_config.hpp"

#include "Ashita.h"
#include "Memory.h"

#include <array>
#include <cstring>

namespace
{
    template<typename T>
    auto Read(const uintptr_t address, T& value) noexcept -> bool
    {
        if (address == 0) return false;
        __try { std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T)); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    auto Set(const uintptr_t function, const int value) noexcept -> bool
    {
        using setter_fn = int(__cdecl*)(uint32_t, int);
        __try { return reinterpret_cast<setter_fn>(function)(0x12, value) >= 0; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }
}

namespace xiui
{
    auto native_window_config::Initialize(void) -> void
    {
        *this = {};
        const auto base = Ashita::Memory::GetModuleBase("FFXiMain.dll");
        const auto size = Ashita::Memory::GetModuleSize("FFXiMain.dll");
        if (base == 0 || size == 0) return;
        constexpr char pattern[] = "8B0D????????85C974108B4424088B5424045052E8????????C383C8FFC3";
        const auto match = Ashita::Memory::FindPattern(base, size, pattern, 0, 0);
        uintptr_t slot{};
        if (match == 0 || Ashita::Memory::FindPattern(base, size, pattern, 0, 1) != 0 ||
            !Read(match + 2, slot) || slot < base || size < sizeof(uintptr_t) || slot - base > size - sizeof(uintptr_t)) return;
        this->config_slot_ = slot;
        this->setter_ = match;
    }

    auto native_window_config::Type(void) const -> std::optional<uint8_t>
    {
        uintptr_t config{}, table{};
        std::array<uint32_t, 8> record{};
        if (this->setter_ == 0 || !Read(this->config_slot_, config) || !Read(config + 4, table) ||
            table == 0 || !Read(table + 0x12 * 44, record) || record[1] != 0x12 || record[2] > 7 || record[6] != 0 || record[7] != 7) return std::nullopt;
        return static_cast<uint8_t>(record[2]);
    }

    auto native_window_config::TimestampMode(void) const -> std::optional<uint8_t>
    {
        uintptr_t config{}, table{};
        std::array<uint32_t, 8> record{};
        if (this->setter_ == 0 || !Read(this->config_slot_, config) || !Read(config + 4, table) || table == 0 ||
            !Read(table + 0xB5 * 44, record) || record[1] != 0xB5 || record[2] > 2 || record[6] != 0 || record[7] != 2) return std::nullopt;
        return static_cast<uint8_t>(record[2]);
    }

    auto native_window_config::SetType(const int type) const -> bool
    {
        const auto current = this->Type();
        if (!current || type < 0 || type > 7) return false;
        if (*current == type) return true;
        return Set(this->setter_, type) && this->Type() == type;
    }
}
