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
    struct treasure_snapshot
    {
        uint32_t item{}, count{}, drop{};
        uint16_t lot{}, winning_lot{};
        std::string winner{};
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
        auto BeginZone(void) -> void { this->ready_ = false; this->clock_known_ = false; this->next_update_ = 0; }
        auto Update(IAshitaCore* core, uint64_t now, bool force = false) -> void;
        auto Reconcile(const std::array<treasure_snapshot, capacity>& slots, bool ready, uint32_t player, uint64_t now) -> void;
        auto HandlePacket(uint16_t id, std::span<const uint8_t> data, uint64_t now) -> void;
        auto Entries(void) const -> const std::array<treasure_entry, capacity>& { return this->entries_; }
        auto Count(void) const -> size_t;
        auto Ready(void) const -> bool { return this->ready_; }
        auto Plan(size_t slot, uint64_t generation, bool pass, uint64_t now) const -> std::optional<treasure_action>;
        auto Queue(const treasure_action& action, uint64_t now, const std::function<void(const treasure_action&)>& send) -> bool;
        auto Act(IAshitaCore* core, size_t slot, uint64_t generation, bool pass, uint64_t now) -> bool;
        auto Error(void) const -> const std::string& { return this->error_; }
    private:
        struct drop_event { uint32_t item{}, start{}; uint64_t serial{}, expires{}; };
        std::array<treasure_entry, capacity> entries_{};
        std::array<drop_event, capacity> drops_{};
        std::array<uint64_t, capacity> applied_{};
        std::array<std::pair<uint32_t, uint32_t>, capacity> resolved_{};
        uint64_t sequence_{}, next_update_{}, clock_at_{};
        uint32_t player_{}, clock_start_{};
        bool ready_{}, clock_known_{};
        std::string error_{};
    };
}
