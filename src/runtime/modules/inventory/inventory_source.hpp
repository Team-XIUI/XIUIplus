#pragma once

#include "Ashita.h"

#include <array>
#include <cstdint>
#include <span>

namespace xiui
{
    struct inventory_group
    {
        const char* id;
        const char* name;
        std::array<uint8_t, 8> bags;
        size_t count;
        float warning_slots, critical_slots;
    };

    inline constexpr std::array<inventory_group, 6> inventory_groups{{
        {"inventory", "Inventory", {0}, 1, 15, 29},
        {"inventory.satchel", "Satchel", {5}, 1, 15, 29},
        {"inventory.locker", "Locker", {4}, 1, 60, 75},
        {"inventory.safe", "Safe", {1, 9}, 2, 120, 150},
        {"inventory.storage", "Storage", {2}, 1, 60, 75},
        {"inventory.wardrobe", "Wardrobes", {8, 10, 11, 12, 13, 14, 15, 16}, 8, 400, 550}}};

    struct inventory_usage
    {
        uint32_t used{}, capacity{};
        bool ready{};
    };

    struct inventory_state
    {
        std::array<inventory_usage, 17> bags{};
        bool available{}, menu_open{};
    };

    auto InventoryTotal(const inventory_state& state, const inventory_group& group) -> inventory_usage;

    class inventory_source
    {
    public:
        auto Reset(void) -> void { *this = {}; }
        auto HandlePacket(uint16_t id, std::span<const uint8_t> data) -> void;
        auto Update(IAshitaCore* core, uint64_t now) -> void;
        auto State(void) const -> const inventory_state& { return this->state_; }
        static auto ReadBags(const IInventory& inventory) -> std::array<inventory_usage, 17>;

    private:
#if defined(XIUI_ENABLE_INVENTORY_TESTS)
        friend struct inventory_test;
#endif
        inventory_state state_{};
        uint32_t player_id_{}, zone_{};
        uint64_t next_scan_{};
        bool syncing_{};
    };
}
