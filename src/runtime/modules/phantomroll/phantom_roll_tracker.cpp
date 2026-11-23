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

    auto phantom_roll_tracker::SeatFor(const uint16_t status) const -> const phantom_roll_seat*
    {
        for (const auto& seat : this->seats_) if (seat.Present() && !seat.busted && seat.status == status) return &seat;
        return nullptr;
    }

    auto phantom_roll_tracker::PartyVisible(const phantom_roll_party& roll) const -> bool
    { return roll.Present() && (roll.detached || this->SeatFor(roll.status) == nullptr); }

    auto phantom_roll_tracker::Observe(const combat_action& action, const uint32_t localId, const uint64_t now, const std::span<const uint32_t> party) -> bool
    {
        if (localId == 0 || action.category != 6 || action.target_count > action.targets.size()) return false;
        const bool local = action.actor == localId;
        const auto* seat = local ? this->ObserveLocal(action, localId, now) : nullptr;
        if (local && seat == nullptr) return false;
        return this->ObserveParty(action, localId, now, party, seat) || local;
    }

    auto phantom_roll_tracker::ObserveParty(const combat_action& action, const uint32_t localId, const uint64_t now, const std::span<const uint32_t> party, const phantom_roll_seat* seat) -> bool
    {
        const auto member = [&](const uint32_t id) { return std::find(party.begin(), party.end(), id) != party.end(); };
        const bool local = seat != nullptr;
        if (!local && !member(action.actor)) return false;
        // A target the roll did not apply to reports zero.
        uint32_t total{};
        for (size_t i = 0; i < action.target_count && total == 0; ++i)
            if ((member(action.targets[i].id) || (!local && action.targets[i].id == localId)) && action.targets[i].result_count != 0)
                total = action.targets[i].parameter;
        const bool doubleUp = action.parameter == 123;
        if (total == 0)
        {
            // A fresh roll that missed the party leaves their older copy on its own clock.
            if (local && !doubleUp)
                for (auto& entry : this->party_) if (entry.Present() && entry.status == seat->status) entry.detached = true;
            return false;
        }
        // In-range holders lose a busted roll; their status lists confirm it.
        if (total > 11) return false;
        if (doubleUp)
        {
            phantom_roll_party* latest{};
            for (auto& entry : this->party_)
            {
                if (!entry.Present() || entry.roller != action.actor || (local && entry.status != seat->status)) continue;
                if (latest == nullptr || entry.pending_at > latest->pending_at) latest = &entry;
            }
            if (latest == nullptr || now < latest->pending_at || now - latest->pending_at > 50000 || total <= latest->total) return false;
            latest->total = static_cast<uint8_t>(total);
            return true;
        }
        const auto* roll = PhantomRollByAbility(action.parameter, server_rules::retail);
        if (roll == nullptr) return false;
        auto entry = std::find_if(this->party_.begin(), this->party_.end(), [&](const auto& value) { return value.status == roll->status; });
        if (entry == this->party_.end()) entry = std::find_if(this->party_.begin(), this->party_.end(), [](const auto& value) { return !value.Present(); });
        if (entry == this->party_.end()) return false;
        *entry = {roll->ability, roll->status, static_cast<uint8_t>(total), 0, true, false, false, action.actor, now + 300000, 300000, now};
        return true;
    }

    auto phantom_roll_tracker::ObserveLocal(const combat_action& action, const uint32_t localId, const uint64_t now) -> const phantom_roll_seat*
    {
        uint32_t total{};
        for (size_t i = 0; i < action.target_count; ++i)
            if (action.targets[i].id == localId && action.targets[i].result_count != 0)
            { total = action.targets[i].parameter; break; }
        if (total < 1 || total > 17) return nullptr;
        const auto* roll = PhantomRollByAbility(action.parameter, server_rules::retail);
        if (roll == nullptr && action.parameter != 123) return nullptr;
        if (action.parameter == this->last_ability_ && total == this->last_total_ && now >= this->last_action_ && now - this->last_action_ < 1000) return nullptr;
        phantom_roll_seat* seat{};
        const bool doubleUp = action.parameter == 123;
        if (doubleUp)
        {
            if (this->double_up_until_ <= now) return nullptr;
            auto* latest = this->LatestOpen();
            if (latest == nullptr) return nullptr;
            seat = &this->seats_[static_cast<size_t>(latest - this->seats_.data())];
            if (total <= seat->total) return nullptr;
            roll = PhantomRollByAbility(seat->ability, server_rules::retail);
        }
        else seat = this->Claim(roll->status);
        if (seat == nullptr || roll == nullptr) return nullptr;
        *seat = {roll->ability, roll->status, static_cast<uint8_t>(total), total > 11, false, now + 300000, 300000, now, ++this->sequence_};
        this->last_action_ = now; this->last_ability_ = action.parameter; this->last_total_ = total;
        this->window_sequence_ = seat->busted ? 0 : seat->sequence;
        if (seat->busted) { this->double_up_until_ = 0; this->double_up_observed_ = false; }
        else if (!doubleUp)
        { this->double_up_until_ = now + 45000; this->double_up_pending_ = now; this->double_up_observed_ = false; }
        return seat;
    }

    auto phantom_roll_tracker::Reconcile(const std::span<const phantom_roll_status> statuses, const uint64_t now, const std::span<const phantom_roll_member> party) -> void
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
            used[i] = true;
            *empty = {roll != nullptr ? roll->ability : uint16_t{}, statuses[i].id, 0, statuses[i].id == 309, true,
                statuses[i].expires, statuses[i].expires > now ? statuses[i].expires - now : 0, now, 0};
        }
        const auto held = [](const phantom_roll_member& member, const uint16_t status) {
            return member.known && std::find(member.statuses.begin(), member.statuses.end(), status) != member.statuses.end();
        };
        const bool anyUnknown = std::any_of(party.begin(), party.end(), [](const auto& member) { return !member.known; });
        // Local rolls beyond the two seats, such as a second Corsair's.
        const auto spare = [&](const uint16_t status) {
            for (size_t i = 0; i < count; ++i) if (!used[i] && statuses[i].id == status) return &statuses[i];
            return static_cast<const phantom_roll_status*>(nullptr);
        };
        for (auto& entry : this->party_)
        {
            if (!entry.Present()) continue;
            entry.holders = static_cast<uint8_t>(std::count_if(party.begin(), party.end(), [&](const auto& member) { return held(member, entry.status); }));
            const auto* seat = entry.detached ? nullptr : this->SeatFor(entry.status);
            const auto* own = seat == nullptr ? spare(entry.status) : nullptr;
            entry.on_self = seat != nullptr || own != nullptr;
            if (seat != nullptr && seat->expires != 0)
            {
                // The party's copy shares the seat's clock until the seat is replaced.
                entry.expires = seat->expires; entry.duration = seat->duration; entry.estimated = !seat->observed;
                if (seat->total != 0) entry.total = seat->total;
            }
            else if (own != nullptr && own->expires != 0)
            {
                entry.expires = own->expires; entry.estimated = false;
                if (own->expires > now) entry.duration = std::max(entry.duration, own->expires - now);
            }
            // Members whose statuses are unavailable keep a timed roll until it runs out.
            const bool alive = entry.on_self || entry.holders != 0 || (anyUnknown && entry.roller != 0 && entry.expires > now);
            if (!alive && age(entry.pending_at) >= 5000) entry = {};
        }
        const auto adopt = [&](const uint16_t status, const uint64_t expires) {
            const auto* roll = PhantomRollByStatus(status, server_rules::retail);
            if (roll == nullptr || this->SeatFor(status) != nullptr) return;
            if (std::any_of(this->party_.begin(), this->party_.end(), [&](const auto& entry) { return entry.status == status; })) return;
            auto empty = std::find_if(this->party_.begin(), this->party_.end(), [](const auto& entry) { return !entry.Present(); });
            if (empty == this->party_.end()) return;
            *empty = {roll->ability, status, 0, 0, false, false, false, 0, expires, expires > now ? expires - now : 0, now};
        };
        for (size_t i = 0; i < count; ++i) if (!used[i]) adopt(statuses[i].id, statuses[i].expires);
        for (const auto& member : party) if (member.known) for (const auto status : member.statuses) adopt(status, 0);
        const auto doubleUp = std::find_if(statuses.begin(), statuses.end(), [](const auto& value) { return value.id == 308; });
        if (doubleUp != statuses.end())
        { this->double_up_until_ = doubleUp->expires; this->double_up_observed_ = true; }
        else if (this->double_up_observed_ || age(this->double_up_pending_) >= 3000) this->double_up_until_ = 0;
    }
}
