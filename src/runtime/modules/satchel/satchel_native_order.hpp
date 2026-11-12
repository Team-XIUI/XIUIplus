#pragma once

#include "satchel_model.hpp"

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace xiui
{
    // Item and slot pairs in the order the native Item menu lists a bag.
    using satchel_native_entry = std::pair<uint16_t, uint8_t>;

    // Orders items like the native list. Unlisted or stale entries are ignored and unlisted items follow in slot order.
    auto SatchelApplyNativeOrder(std::vector<satchel_item>& items, std::span<const satchel_native_entry> order) -> void;

    // Reads the client's per-bag item list order (its Auto/Manual sort), which differs from server slot order.
    class satchel_native_order
    {
    public:
        auto Read(uint8_t bag) -> std::vector<satchel_native_entry>;

    private:
        auto Initialize(void) -> void;

        uintptr_t table_{}, enabled_{};
        bool initialized_{};
    };
}
