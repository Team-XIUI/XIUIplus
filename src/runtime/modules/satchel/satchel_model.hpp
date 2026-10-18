#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace xiui
{
    inline constexpr std::array<const char*, 17> satchel_bag_names{
        "Inventory", "Safe", "Storage", "Temporary", "Locker", "Satchel", "Sack", "Case",
        "Wardrobe 1", "Safe 2", "Wardrobe 2", "Wardrobe 3", "Wardrobe 4", "Wardrobe 5", "Wardrobe 6", "Wardrobe 7", "Wardrobe 8"};
    inline constexpr std::array<uint8_t, 17> satchel_bag_order{0, 1, 9, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15, 16};

    struct satchel_item
    {
        uint16_t id{};
        uint8_t bag{}, slot{};
        uint32_t flags{}, count{}, price{};
        uint64_t generation{};
        bool equipped{}, read_only{};
        std::array<uint8_t, 28> extra{};
        auto operator==(const satchel_item&) const -> bool = default;
    };

    struct satchel_bag
    {
        uint32_t capacity{};
        bool ready{}, accessible{};
        std::vector<satchel_item> items{};
    };

    struct satchel_snapshot
    {
        std::string identity{}, name{};
        uint32_t character_id{}, gil{}, zone{};
        uint64_t generation{}, observed{};
        bool ready{}, read_only{};
        std::array<satchel_bag, 17> bags{};
    };

    inline auto SatchelFind(const satchel_snapshot& snapshot, const uint8_t bag, const uint8_t slot) -> const satchel_item*
    {
        if (bag >= snapshot.bags.size()) return nullptr;
        for (const auto& item : snapshot.bags[bag].items) if (item.slot == slot) return &item;
        return nullptr;
    }
}
