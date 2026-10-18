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

    class phantom_roll_tracker
    {
    public:
        auto Clear(void) -> void { *this = {}; }
        auto Observe(const combat_action& action, uint32_t localId, uint64_t now) -> bool;
        auto Reconcile(std::span<const phantom_roll_status> statuses, uint64_t now) -> void;
        auto Seats(void) const -> const std::array<phantom_roll_seat, 2>& { return this->seats_; }
        auto DoubleUpUntil(void) const -> uint64_t { return this->double_up_until_; }
        auto LatestOpen(void) const -> const phantom_roll_seat*;

    private:
        std::array<phantom_roll_seat, 2> seats_{};
        uint64_t sequence_{}, window_sequence_{}, double_up_until_{}, double_up_pending_{}, last_action_{};
        uint32_t last_ability_{}, last_total_{};
        bool double_up_observed_{};
        auto Claim(uint16_t status) -> phantom_roll_seat*;
    };
}
