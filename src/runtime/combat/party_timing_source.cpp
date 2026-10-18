#include "party_timing_source.hpp"

#include <algorithm>
#include <cmath>

namespace xiui
{
    auto party_timing_source::Update(const std::span<const party_member> members, const uint32_t player,
        const combat_timing_source* local, const uint64_t now) -> void
    {
        if (player == 0) { this->Clear(); return; }
        if (player != this->player_ || (this->previous_update_ != 0 && (now < this->previous_update_ || now - this->previous_update_ > 5000))) this->Clear();
        this->player_ = player; this->local_ = local;
        if (now < this->next_update_) return;
        this->previous_update_ = now; this->next_update_ = now + 100;
        const auto eligible = [player](const party_member& member) {
            return member.id != player && member.id != 0 && member.active && member.in_zone && member.entity_valid && member.status_known && member.hp > 0;
        };
        std::erase_if(this->members_, [&](const auto& entry) {
            return std::none_of(members.begin(), members.end(), [&](const auto& member) { return member.id == entry.first && eligible(member); });
        });
        const double clock = now / 1000.0;
        for (const auto& member : members)
        {
            if (!eligible(member)) continue;
            auto& observed = this->members_[member.id];
            if (observed.index != member.index || observed.zone != member.zone) observed = {};
            observed.index = member.index; observed.zone = member.zone;
            uint64_t effects{}; bool disabled{}, blocked{};
            for (size_t index = 0; index < std::min(member.buff_count, member.buffs.size()); ++index)
            {
                const auto buff = member.buffs[index];
                blocked = blocked || buff == 8 || buff == 20 || buff == 31;
                disabled = disabled || buff == 2 || buff == 7 || buff == 10 || buff == 19 || buff == 28 || buff == 159 || buff == 193;
                if (buff == 13 || buff == 33 || buff == 46 || buff == 126 || buff == 194 || buff == 214 || buff == 353)
                    effects ^= static_cast<uint64_t>(buff) * 1099511628211ull;
            }
            if (effects != observed.effects)
            {
                observed.attack.Stop(); observed.last_round = 0; observed.effects = effects;
            }
            observed.engaged = member.status == 1;
            if (!observed.engaged) { observed.attack.Stop(); observed.last_round = 0; }
            if (disabled) observed.attack.Hold(clock, 8); else observed.attack.Release(clock, 8);

            const bool hasMP = member.mp > 0 || PartyJobHasMP(member.main_job) || PartyJobHasMP(member.sub_job);
            observed.full = member.hp_percent == 100 && (!hasMP || member.mp_percent == 100);
            auto sample = observed.sample;
            sample.hp = member.hp; sample.mp = member.mp; sample.tp = member.tp;
            sample.resting = member.status == 33;
            sample.blocked = blocked;
            sample.joined_late = !observed.initialized || observed.resting;
            if (!observed.resting || !observed.recovery.Predict(clock).active)
            { sample.hp_max = member.hp_max; sample.mp_max = member.mp_max; }
            sample.hp_max = std::max(sample.hp_max, member.hp + (member.hp_percent < 100 ? 1u : 0u));
            sample.mp_max = std::max(sample.mp_max, member.mp + (hasMP && member.mp_percent < 100 ? 1u : 0u));
            if (member.hp_max == 0 || (hasMP && member.mp_max == 0)) observed.recovery.Stop();
            else observed.recovery.Update(sample, clock);
            observed.sample = sample; observed.resting = sample.resting; observed.initialized = true;
        }
    }

    auto party_timing_source::Observe(const combat_action& action, const uint64_t now) -> void
    {
        const double clock = now / 1000.0;
        if (action.category != 1)
            for (size_t index = 0; index < action.target_count; ++index)
                if (const auto found = this->members_.find(action.targets[index].id); found != this->members_.end()) found->second.recovery.Contaminate(clock);
        const auto found = this->members_.find(action.actor);
        if (found == this->members_.end() || !found->second.engaged) return;
        auto& observed = found->second;
        auto& timer = observed.attack;
        if (action.category == 1)
        {
            const auto prediction = timer.Predict(clock);
            const double gap = clock - observed.last_round;
            if (prediction.learned && (prediction.state.empty() || prediction.state == "Ready" || prediction.state == "Waiting for attack") &&
                std::abs(gap - prediction.interval) > std::max(0.75f, prediction.interval * 0.35f)) timer.Stop();
            timer.Round(clock, observed.interval, observed.effects, true);
            observed.last_round = clock;
            if (timer.Predict(clock).learned) observed.interval = static_cast<float>(timer.Interval());
        }
        else if (action.category == 3 || action.category == 6) timer.AddHold(clock, 2);
        else if (action.category == 7 || action.category == 8 || action.category == 9 || action.category == 12)
        {
            const unsigned reason = action.category == 12 ? 2 : action.category == 9 ? 4 : 1;
            if ((action.parameter & 0xFFFF) == 0x7073) timer.Release(clock, reason, 2);
            else timer.Hold(clock, reason);
        }
        else if (action.category == 2 || action.category == 4 || action.category == 5 || action.category == 11)
            timer.Release(clock, action.category == 2 ? 2 : action.category == 5 ? 4 : 1, 2);
    }

    auto party_timing_source::Swing(const uint32_t member, const double now) const -> attack_prediction
    {
        if (this->IsPlayer(member)) return this->local_ != nullptr ? this->local_->Player(now) : attack_prediction{};
        const auto found = this->members_.find(member);
        if (found == this->members_.end() || !found->second.engaged || now - found->second.last_round > 30) return {};
        const auto prediction = found->second.attack.Predict(now);
        return prediction.learned ? prediction : attack_prediction{};
    }

    auto party_timing_source::Resting(const uint32_t member, const double now) const -> attack_prediction
    {
        if (this->IsPlayer(member)) return this->local_ != nullptr ? this->local_->Recovery().Predict(now) : attack_prediction{};
        const auto found = this->members_.find(member);
        if (found == this->members_.end()) return {};
        const auto prediction = found->second.recovery.Predict(now);
        return prediction.known ? prediction : attack_prediction{};
    }

    auto party_timing_source::Full(const uint32_t member) const -> bool
    {
        if (this->IsPlayer(member)) return this->local_ != nullptr && this->local_->Recovery().Full();
        const auto found = this->members_.find(member);
        return found != this->members_.end() && found->second.full;
    }
}
