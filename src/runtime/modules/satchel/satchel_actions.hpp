#pragma once

#include "satchel_model.hpp"

struct IAshitaCore;
struct IPacketManager;

namespace xiui
{
    enum class satchel_action : uint8_t { move, split, use, equip, drop, bazaar, sort };
    enum class satchel_action_outcome : uint8_t { none, pending, observed, unconfirmed, cancelled, rejected };

    struct satchel_action_request
    {
        satchel_action action{};
        std::string identity{};
        satchel_item item{};
        uint8_t destination{};
        uint32_t quantity{}, price{};
        int equipment_slot{-1};
        uint8_t destination_slot{0x52};
    };

    class satchel_actions
    {
    public:
        auto Submit(IAshitaCore* core, const satchel_snapshot& snapshot, satchel_action_request request, uint64_t now) -> bool;
        auto Update(IAshitaCore* core, const satchel_snapshot& snapshot, uint64_t now) -> void;
        auto Cancel(void) -> void;
        auto SetRestricted(bool restricted) -> void;
        auto Busy(void) const -> bool { return this->busy_; }
        auto Outcome(void) const -> satchel_action_outcome { return this->outcome_; }
        auto Status(void) const -> const std::string& { return this->status_; }
        auto Eligibility(IAshitaCore* core, const satchel_snapshot& snapshot, const satchel_action_request& request) const -> std::string;

    private:
        friend struct satchel_actions_test;
        struct resource
        {
            uint16_t type{}, flags{}, stack{1}, slots{}, targets{}, level{};
            uint32_t jobs{};
            uint8_t charges{}, job{}, job_level{};
        };
        struct plan
        {
            uint8_t destination_slot{0x52};
            int equipment_slot{-1};
            uint32_t quantity{};
        };
        static auto SnapshotEligibility(const satchel_snapshot& snapshot, const satchel_action_request& request) -> std::string;
        static auto Plan(const satchel_snapshot& snapshot, const satchel_action_request& request, const resource& item, plan& result) -> std::string;
        static auto Dispatch(IPacketManager& packets, const satchel_action_request& request, const plan& result,
            uint32_t character_id, uint32_t character_index) -> void;
        auto Prepare(IAshitaCore* core, const satchel_snapshot& snapshot, const satchel_action_request& request,
            satchel_snapshot& live, plan& result) const -> std::string;
        auto Observe(const satchel_snapshot& snapshot, uint64_t now, bool equipment_ack = false) -> void;

        satchel_action_request pending_{};
        satchel_snapshot before_{};
        plan plan_{};
        uint64_t deadline_{};
        std::array<uint64_t, 17> next_sort_{};
        bool busy_{}, restricted_{};
        satchel_action_outcome outcome_{};
        std::string status_{};
    };
}
