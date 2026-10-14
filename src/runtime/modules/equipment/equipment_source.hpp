#pragma once

#include "modules/inventory/inventory_source.hpp"

#include <string>

namespace xiui
{
    inline constexpr std::array<uint8_t, 16> equipment_grid_order{0, 1, 2, 3, 4, 9, 11, 12, 5, 6, 13, 14, 15, 10, 7, 8};
    inline constexpr std::array<const char*, 16> equipment_slot_names{
        "Main", "Sub", "Range", "Ammo", "Head", "Body", "Hands", "Legs", "Feet", "Neck", "Waist", "Ear 1", "Ear 2", "Ring 1", "Ring 2", "Back"};

    struct equipped_item
    {
        uint16_t id{};
        uint8_t bag{}, index{};
        uint32_t count{};
        bool ready{};
        std::string name{};
    };

    class equipment_source
    {
    public:
        auto Reset(void) -> void { *this = {}; }
        auto Update(IAshitaCore* core, const inventory_state& inventory, uint64_t now) -> void;
        auto Items(void) const -> const std::array<equipped_item, 16>& { return this->items_; }
        auto Available(void) const -> bool { return this->available_; }
        static auto ReadSlot(const IInventory& inventory, uint32_t slot) -> equipped_item;

    private:
#if defined(XIUI_ENABLE_INVENTORY_TESTS)
        friend struct inventory_test;
#endif
        std::array<equipped_item, 16> items_{};
        uint64_t next_scan_{};
        bool available_{};
    };
}
