#pragma once

#include "satchel_model.hpp"
#include "satchel_native_order.hpp"
#include "modules/inventory/inventory_source.hpp"

#include <optional>
#include <span>

namespace xiui
{
    auto SatchelBagAccessible(uint8_t bag, bool inMogHouse) -> bool;

    class satchel_source
    {
    public:
        auto Update(IAshitaCore* core, const inventory_state& inventory, const std::optional<std::string>& identity, uint64_t now) -> void;
        auto HandlePacket(uint16_t id, std::span<const uint8_t> data) -> void;
        auto Reset(void) -> void;
        auto Snapshot(void) const -> const satchel_snapshot& { return this->snapshot_; }
        static auto ReadBag(const IInventory& inventory, uint8_t bag, bool ready, bool accessible) -> satchel_bag;

    private:
#if defined(XIUI_ENABLE_SATCHEL_TESTS)
        friend struct satchel_data_test;
#endif
        satchel_snapshot snapshot_{};
        satchel_native_order native_order_{};
        uint64_t next_scan_{}, generation_{};
        uint64_t next_identity_scan_{};
        uint64_t identity_retry_{1000};
        uint32_t server_character_{};
        std::optional<std::string> server_identity_{};
        bool syncing_{}, in_mog_house_{};

        auto Invalidate(void) -> void;
        auto Publish(satchel_snapshot next) -> void;
    };
}
