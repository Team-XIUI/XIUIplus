#include "native_window_config.hpp"

#include "Ashita.h"
#include "Memory.h"
#include "native_pattern.hpp"

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

    auto Set(const uintptr_t function, const uint32_t id, const int value) noexcept -> bool
    {
        using setter_fn = int(__cdecl*)(uint32_t, int);
        __try { return reinterpret_cast<setter_fn>(function)(id, value) >= 0; }
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
        const auto match = FindUniquePattern(base, size, pattern);
        uintptr_t slot{};
        if (match == 0 ||
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
        return Set(this->setter_, 0x12, type) && this->Type() == type;
    }

    auto native_window_config::FullKeyboard(void) const -> std::optional<bool>
    {
        const auto layout = this->KeyboardLayout();
        return layout ? std::optional{*layout == native_keyboard_layout::full} : std::nullopt;
    }

    auto native_window_config::KeyboardLayout(void) const -> std::optional<native_keyboard_layout>
    {
        uintptr_t config{}, table{};
        std::array<uint32_t, 8> record{};
        if (this->setter_ == 0 || !Read(this->config_slot_, config) || !Read(config + 4, table) || table == 0 ||
            !Read(table + 66 * 44, record) || record[1] != 66 || record[3] != 4 || record[6] != 0 || record[7] != 2 || record[2] > 2) return std::nullopt;
        return static_cast<native_keyboard_layout>(record[2]);
    }

    auto native_window_config::ChatWindowCount(void) const -> std::optional<unsigned>
    {
        uintptr_t config{}, table{};
        std::array<uint32_t, 8> record{};
        if (this->setter_ == 0 || !Read(this->config_slot_, config) || !Read(config + 4, table) || table == 0 ||
            !Read(table + 180 * 44, record) || record[1] != 180 || record[3] != 4 || record[6] != 0 || record[7] != 2 || record[2] > 2)
            return std::nullopt;
        return record[2] == 0 ? 1u : 2u;
    }

    auto native_window_config::ChatWindow(const bool secondary) const -> std::optional<native_chat_window_settings>
    {
        uintptr_t config{}, table{};
        if (this->setter_ == 0 || !Read(this->config_slot_, config) || !Read(config + 4, table) || table == 0) return std::nullopt;
        const auto value = [table](const uint32_t id, const int32_t minimum, const int32_t maximum) -> std::optional<int32_t> {
            std::array<int32_t, 8> record{};
            if (!Read(table + id * 44, record) || record[1] != static_cast<int32_t>(id) || record[3] != 4 ||
                record[6] != minimum || record[7] != maximum || record[2] < minimum || record[2] > maximum) return std::nullopt;
            return record[2];
        };
        const auto reactive = value(secondary ? 182 : 14, 0, 1);
        const auto maximum = value(secondary ? 183 : 15, 4, 20);
        const auto minimum = value(secondary ? 184 : 16, 0, 20);
        auto seconds = secondary ? value(198, -1, 30) : value(17, 3, 30);
        if (secondary && seconds == -1) seconds = value(17, 3, 30);
        if (!reactive || !maximum || !minimum || !seconds || *minimum > *maximum || *seconds < 3) return std::nullopt;
        return native_chat_window_settings{*reactive == 0, static_cast<uint32_t>(*minimum), static_cast<uint32_t>(*maximum), static_cast<uint32_t>(*seconds)};
    }

    auto native_window_config::ChatFilterValue(const uint32_t id) const -> std::optional<uint32_t>
    {
        if (!((id >= 50 && id <= 53) || id == 71 || (id >= 185 && id <= 188))) return std::nullopt;
        uintptr_t config{}, table{};
        std::array<uint32_t, 8> record{};
        const uint32_t maximum = id == 53 ? 3 : id == 71 ? 1 : UINT32_MAX;
        if (this->setter_ == 0 || !Read(this->config_slot_, config) || !Read(config + 4, table) || table == 0 ||
            !Read(table + id * 44, record) || record[1] != id || (record[3] != 4 && record[3] != 5) ||
            record[6] != 0 || record[7] != maximum || record[2] > maximum) return std::nullopt;
        return record[2];
    }

    auto native_window_config::SetChatFilterValue(const uint32_t id, const uint32_t value) const -> bool
    {
        const auto current = this->ChatFilterValue(id);
        if (!current || (id == 53 && value > 3) || (id == 71 && value > 1)) return false;
        if (*current == value) return true;
        int32_t native{}; std::memcpy(&native, &value, sizeof(native));
        return Set(this->setter_, id, native) && this->ChatFilterValue(id) == value;
    }
}
