#include "phantom_roll_tracker.hpp"
#include "phantom_roll_data.hpp"

#include <algorithm>
#include <limits>

namespace xiui
{
    auto phantom_roll_tracker::LatestOpen(void) const -> const phantom_roll_seat*
    {
        const phantom_roll_seat* latest{};
        for (const auto& seat : this->seats_)
            if (seat.Present() && !seat.busted && seat.sequence != 0 && seat.sequence == this->window_sequence_) latest = &seat;
        return latest;
    }

    auto phantom_roll_tracker::Claim(const uint16_t status) -> phantom_roll_seat*
    {
        for (auto& seat : this->seats_) if (seat.Present() && !seat.busted && seat.status == status) return &seat;
        for (auto& seat : this->seats_) if (!seat.Present()) return &seat;
        phantom_roll_seat* oldest{};
        for (auto& seat : this->seats_)
            if (!seat.busted && (oldest == nullptr || seat.sequence < oldest->sequence)) oldest = &seat;
        return oldest;
    }

    auto phantom_roll_tracker::Observe(const combat_action& action, const uint32_t localId, const uint64_t now) -> bool
    {
        if (localId == 0 || action.actor != localId || action.category != 6 || action.target_count > action.targets.size()) return false;
        uint32_t total{};
        for (size_t i = 0; i < action.target_count; ++i)
            if (action.targets[i].id == localId && action.targets[i].result_count != 0)
            { total = action.targets[i].parameter; break; }
        if (total < 1 || total > 17) return false;
        const auto* roll = PhantomRollByAbility(action.parameter, server_rules::retail);
        if (roll == nullptr && action.parameter != 123) return false;
        if (action.parameter == this->last_ability_ && total == this->last_total_ && now >= this->last_action_ && now - this->last_action_ < 1000) return false;
        phantom_roll_seat* seat{};
        const bool doubleUp = action.parameter == 123;
        if (doubleUp)
        {
            if (this->double_up_until_ <= now) return false;
            auto* latest = this->LatestOpen();
            if (latest == nullptr) return false;
            seat = &this->seats_[static_cast<size_t>(latest - this->seats_.data())];
            if (total <= seat->total) return false;
            roll = PhantomRollByAbility(seat->ability, server_rules::retail);
        }
        else seat = this->Claim(roll->status);
        if (seat == nullptr || roll == nullptr) return false;
        *seat = {roll->ability, roll->status, static_cast<uint8_t>(total), total > 11, false, now + 300000, 300000, now, ++this->sequence_};
        this->last_action_ = now; this->last_ability_ = action.parameter; this->last_total_ = total;
        this->window_sequence_ = seat->busted ? 0 : seat->sequence;
        if (seat->busted) { this->double_up_until_ = 0; this->double_up_observed_ = false; }
        else if (!doubleUp)
        { this->double_up_until_ = now + 45000; this->double_up_pending_ = now; this->double_up_observed_ = false; }
        return true;
    }

    auto phantom_roll_tracker::Reconcile(const std::span<const phantom_roll_status> statuses, const uint64_t now) -> void
    {
        std::array<bool, 32> used{};
        std::array<bool, 2> matched{};
        const auto count = std::min(statuses.size(), used.size());
        const auto age = [now](const uint64_t at) { return now >= at ? now - at : uint64_t{3000}; };
        // Match each bust to its own native309 clock before recovering unknown seats.
        for (const bool observed : {true, false}) for (size_t index = 0; index < this->seats_.size(); ++index)
        {
            auto& seat = this->seats_[index];
            if (!seat.Present() || seat.observed != observed) continue;
            size_t match = count;
            uint64_t distance = std::numeric_limits<uint64_t>::max();
            for (size_t i = 0; i < count; ++i)
            {
                if (used[i] || statuses[i].id != (seat.busted ? 309 : seat.status)) continue;
                const auto delta = statuses[i].expires > seat.expires ? statuses[i].expires - seat.expires : seat.expires - statuses[i].expires;
                if (match == count || delta < distance) { match = i; distance = delta; }
            }
            if (match != count)
            {
                used[match] = true; matched[index] = true;
                seat.expires = statuses[match].expires;
                if (seat.expires > now) seat.duration = std::max(seat.duration, seat.expires - now);
            }
            else if (seat.observed || age(seat.pending_at) >= 3000) seat = {};
        }
        for (size_t i = 0; i < matched.size(); ++i) if (matched[i]) this->seats_[i].observed = true;
        for (size_t i = 0; i < count; ++i)
        {
            if (used[i]) continue;
            const auto* roll = PhantomRollByStatus(statuses[i].id, server_rules::retail);
            if (roll == nullptr && statuses[i].id != 309) continue;
            auto empty = std::find_if(this->seats_.begin(), this->seats_.end(), [](const auto& seat) { return !seat.Present(); });
            if (empty == this->seats_.end()) break;
            *empty = {roll != nullptr ? roll->ability : uint16_t{}, statuses[i].id, 0, statuses[i].id == 309, true,
                statuses[i].expires, statuses[i].expires > now ? statuses[i].expires - now : 0, now, 0};
        }
        const auto doubleUp = std::find_if(statuses.begin(), statuses.end(), [](const auto& value) { return value.id == 308; });
        if (doubleUp != statuses.end())
        { this->double_up_until_ = doubleUp->expires; this->double_up_observed_ = true; }
        else if (this->double_up_observed_ || age(this->double_up_pending_) >= 3000) this->double_up_until_ = 0;
    }
}
