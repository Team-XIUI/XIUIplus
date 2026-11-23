#pragma once

#include "combat/action_packet.hpp"

#include <array>
#include <span>

namespace xiui
{
    struct phantom_roll_status { uint16_t id{}; uint64_t expires{}; };
    struct phantom_roll_seat
    {
        uint16_t ability{}, status{};
        uint8_t total{};
        bool busted{}, observed{};
        uint64_t expires{}, duration{}, pending_at{}, sequence{};
        auto Present(void) const -> bool { return this->status != 0; }
    };

    // A roll held by main-party members, kept apart from the local player's two seats.
    struct phantom_roll_party
    {
        uint16_t ability{}, status{};
        uint8_t total{}, holders{};
        // detached: the local seat with this status is a later, separate roll.
        bool estimated{}, on_self{}, detached{};
        uint32_t roller{};
        uint64_t expires{}, duration{}, pending_at{};
        auto Present(void) const -> bool { return this->status != 0; }
    };
    // known is false while a member's status list is unavailable, such as in another zone.
    struct phantom_roll_member { uint32_t id{}; bool known{}; std::span<const uint16_t> statuses{}; };

    class phantom_roll_tracker
    {
    public:
        auto Clear(void) -> void { *this = {}; }
        // party lists the other main-party members; their rolls and the reach of local rolls become party rolls.
        auto Observe(const combat_action& action, uint32_t localId, uint64_t now, std::span<const uint32_t> party = {}) -> bool;
        auto Reconcile(std::span<const phantom_roll_status> statuses, uint64_t now, std::span<const phantom_roll_member> party = {}) -> void;
        auto PartyRolls(void) const -> const std::array<phantom_roll_party, 4>& { return this->party_; }
        // False while a seat already shows the same roll.
        auto PartyVisible(const phantom_roll_party& roll) const -> bool;
        auto Seats(void) const -> const std::array<phantom_roll_seat, 2>& { return this->seats_; }
        auto DoubleUpUntil(void) const -> uint64_t { return this->double_up_until_; }
        auto LatestOpen(void) const -> const phantom_roll_seat*;

    private:
        std::array<phantom_roll_seat, 2> seats_{};
        std::array<phantom_roll_party, 4> party_{};
        uint64_t sequence_{}, window_sequence_{}, double_up_until_{}, double_up_pending_{}, last_action_{};
        uint32_t last_ability_{}, last_total_{};
        bool double_up_observed_{};
        auto Claim(uint16_t status) -> phantom_roll_seat*;
        auto ObserveLocal(const combat_action& action, uint32_t localId, uint64_t now) -> const phantom_roll_seat*;
        auto ObserveParty(const combat_action& action, uint32_t localId, uint64_t now, std::span<const uint32_t> party, const phantom_roll_seat* seat) -> bool;
        auto SeatFor(uint16_t status) const -> const phantom_roll_seat*;
    };
}
