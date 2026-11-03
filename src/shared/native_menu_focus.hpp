#pragma once

#include <Windows.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace xiui
{
    template<typename T>
    inline auto ReadNativeMenuValue(const uintptr_t address, T& value) noexcept -> bool
    {
        if (address == 0) return false;
        __try { std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T)); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    inline auto NativeMenuName(const uintptr_t slot, std::array<char, 16>& name, uintptr_t* primitive = nullptr) -> bool
    {
        uintptr_t first{}, resource{};
        if (!ReadNativeMenuValue(slot, first)) return false;
        if (primitive != nullptr) *primitive = first;
        if (first == 0) return true;
        return ReadNativeMenuValue(first + 4, resource) && resource != 0 && ReadNativeMenuValue(resource + 0x46, name) &&
            std::all_of(name.begin(), name.end(), [](const unsigned char value) { return value == 0 || value == ' ' || (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9'); });
    }

    inline auto NativeMenuShortName(const std::array<char, 16>& name) -> std::string_view
    {
        std::string_view text{name.data(), name.size()};
        const auto end = text.find('\0');
        if (end != std::string_view::npos) text = text.substr(0, end);
        const auto last = text.find_last_not_of(' ');
        if (last == std::string_view::npos) return {};
        text = text.substr(0, last + 1);
        if (text.starts_with("menu "))
        {
            const auto start = text.find_first_not_of(' ', 4);
            if (start == std::string_view::npos) return {};
            text.remove_prefix(start);
        }
        return text;
    }

    inline auto CrossbarNativeMenuBlocked(const bool menuOpen, const uintptr_t allowedMenuSlot) -> bool
    {
        if (!menuOpen) return false;
        // Cached permission must still match the live focused menu.
        std::array<char, 16> name{};
        if (!NativeMenuName(allowedMenuSlot, name)) return true;
        const auto id = NativeMenuShortName(name);
        return id != "playermo" && id != "mcr1pall" && id != "mcr2pall" && id != "mcr1long" && id != "mcr2long";
    }
}
