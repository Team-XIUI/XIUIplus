#pragma once

#include "Ashita.h"
#include <array>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace xiui
{
    struct treasure_member_lot { uint32_t id{}; uint16_t lot{}; };
    struct treasure_selection { size_t slot{}; uint64_t generation{}; };
    struct treasure_snapshot
    {
        uint32_t item{}, count{}, drop{};
        uint16_t lot{}, winning_lot{};
        std::string winner{};
        std::array<treasure_member_lot, 18> members{};
    };
    struct treasure_entry : treasure_snapshot
    {
        uint64_t generation{}, expires{}, pending_until{};
        std::vector<std::string> history{};
    };
    struct treasure_action
    {
        uint8_t slot{};
        uint32_t item{}, drop{};
        uint64_t generation{};
        bool pass{};
    };

    class treasure_source
    {
    public:
        static constexpr size_t capacity = 10;
        auto Clear(void) -> void;
        auto BeginZone(void) -> void { this->Clear(); }
        auto Update(IAshitaCore* core, uint64_t now, bool force = false) -> void;
        auto Reconcile(const std::array<treasure_snapshot, capacity>& slots, bool ready, uint32_t player, uint64_t now) -> void;
        auto HandlePacket(uint16_t id, std::span<const uint8_t> data, uint64_t now) -> void;
        auto Entries(void) const -> const std::array<treasure_entry, capacity>& { return this->entries_; }
        auto Count(void) const -> size_t;
        auto Ready(void) const -> bool { return this->ready_; }
        auto MemberLot(size_t slot, uint64_t generation, uint32_t member) const -> std::optional<uint16_t>;
        auto Plan(size_t slot, uint64_t generation, bool pass, uint64_t now) const -> std::optional<treasure_action>;
        auto CaptureBatch(bool pass, uint64_t now) const -> std::vector<treasure_action>;
        auto Queue(const treasure_action& action, uint64_t now, const std::function<void(const treasure_action&)>& send) -> bool;
        auto QueueBatch(std::span<const treasure_action> actions, uint64_t now, const std::function<void(const treasure_action&)>& send) -> size_t;
        auto Act(IAshitaCore* core, size_t slot, uint64_t generation, bool pass, uint64_t now) -> bool;
        auto ActBatch(IAshitaCore* core, std::span<const treasure_action> actions, uint64_t now) -> size_t;
        auto Error(void) const -> const std::string& { return this->error_; }
    private:
        struct drop_event { uint32_t item{}, start{}; uint64_t serial{}, expires{}; };
        std::array<treasure_entry, capacity> entries_{};
        std::array<drop_event, capacity> drops_{};
        std::array<uint64_t, capacity> applied_{}, missing_until_{};
        uint64_t sequence_{}, next_update_{}, clock_at_{};
        uint32_t player_{}, clock_start_{};
        bool ready_{}, clock_known_{};
        std::string error_{};
        auto PrepareActions(IAshitaCore* core, uint64_t now) -> bool;
    };
}
