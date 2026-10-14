#pragma once

#include "notification_center.hpp"
#include "inventory_tracker.hpp"
#include "event_packets.hpp"
#include "Ashita.h"
#include <array>

namespace xiui
{
    class notification_source
    {
    public:
        auto Reset(uint64_t now) -> void;
        auto HandlePacket(IAshitaCore* core, notification_center& center, uint16_t id, std::span<const uint8_t> data, uint64_t now) -> void;
        auto Update(IAshitaCore* core, notification_center& center, uint64_t now) -> void;

    private:
        inventory_tracker inventory_{};
        uint32_t player_id_{};
        uint32_t zone_id_{};
        struct pool_notice { uint32_t item{}, count{}; uint64_t due{}; };
        std::array<pool_notice, 10> pool_{};
        key_item_tracker key_items_{};
        uint32_t gil_{};
        bool balances_ready_{};
        std::optional<uint64_t> party_invite_received_at_{};
        auto ResetBalances(void) -> void;
        auto UpdatePool(IAshitaCore& core, notification_center& center, uint64_t now) -> void;
        static auto ItemTotals(IInventory& inventory) -> item_totals;
    };
} // namespace xiui
