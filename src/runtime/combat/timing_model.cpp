#include "timing_model.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace xiui
{
    auto attack_clock::Begin(const double now, const double delay, const bool late) -> void
    {
        *this = {}; this->active_ = true; this->delay_ = std::clamp(delay, 0.4, 30.0);
        this->start_ = this->seen_ = now; this->due_ = now + 2.8; this->waiting_ = late;
    }

    auto attack_clock::Change(const double estimate, const uint64_t key, const bool wait) -> void
    {
        if (this->key_ == key) return;
        this->key_ = key; this->count_ = this->next_ = 0; this->learned_ = this->clean_ = false;
        this->delay_ = std::clamp(estimate, 0.4, 30.0);
        this->waiting_ = this->waiting_ || wait;
    }

    auto attack_clock::Round(const double now, const double estimate, const uint64_t key, const bool enemy) -> void
    {
        if (!this->active_) this->Begin(now, estimate, true);
        this->Change(estimate, key);
        const double gap = now - this->start_;
        if (this->clean_ && this->holds_ == 0 && this->stalled_.empty() && gap >= 0.2 &&
            gap <= (enemy && !this->learned_ ? 16.7 : this->delay_ * 1.5 + 0.4))
        {
            this->gaps_[this->next_++ % this->gaps_.size()] = gap;
            this->count_ = std::min(this->count_ + 1, this->gaps_.size());
            if (this->count_ >= 3)
            {
                auto gaps = this->gaps_;
                std::sort(gaps.begin(), gaps.begin() + this->count_);
                const double middle = gaps[(this->count_ - 1) / 2];
                double sum{}; size_t accepted{};
                for (size_t i = 0; i < this->count_; ++i)
                    if (!enemy || std::abs(gaps[i] - middle) <= 0.5) { sum += gaps[i]; ++accepted; }
                if (accepted >= 3) { this->delay_ = std::clamp(sum / accepted, 0.4, 30.0); this->learned_ = true; }
            }
        }
        const bool wasStalled = !this->stalled_.empty();
        this->start_ = this->seen_ = now; this->due_ = now + this->delay_;
        this->clean_ = !wasStalled; this->stalled_ = {}; this->waiting_ = false;
        this->holds_ = 0;
    }

    auto attack_clock::Hold(const double now, const unsigned reason) -> void
    {
        if (!this->active_ || (this->holds_ & reason)) return;
        if (this->holds_ == 0) this->frozen_ = std::max(0.0, this->due_ - now);
        this->holds_ |= reason; this->clean_ = false; this->seen_ = now;
    }

    auto attack_clock::Release(const double now, const unsigned reason, const double tail) -> void
    {
        if ((this->holds_ & reason) == 0) return;
        this->holds_ &= ~reason; this->frozen_ += tail;
        if (this->holds_ == 0) this->due_ = now + this->frozen_;
        this->seen_ = now;
    }

    auto attack_clock::AddHold(const double now, const double seconds) -> void
    {
        if (!this->active_) return;
        if (this->holds_) this->frozen_ += seconds;
        else this->due_ = std::max(this->due_, now) + seconds;
        this->clean_ = false; this->seen_ = now;
    }

    auto attack_clock::Predict(const double now) const -> attack_prediction
    {
        attack_prediction result{this->active_, !this->waiting_, this->learned_, 0, 0, static_cast<float>(this->delay_)};
        if (!this->active_) return result;
        result.seconds = static_cast<float>(this->holds_ ? this->frozen_ : std::max(0.0, this->due_ - now));
        result.ratio = std::clamp(1.0f - result.seconds / static_cast<float>(std::max(0.1, this->due_ - this->start_)), 0.0f, 1.0f);
        if (this->holds_ & 8) result.state = "Disabled";
        else if (this->holds_ & 1) result.state = "Casting";
        else if (this->holds_ & 2) result.state = "Ranged attack";
        else if (this->holds_ & 4) result.state = "Using item";
        else if (!this->stalled_.empty()) result.state = this->stalled_;
        else if (this->waiting_ || now > this->due_ + this->delay_) result.state = "Waiting for attack";
        else if (result.seconds <= 0) result.state = "Ready";
        return result;
    }

    auto recovery_clock::NextGap(const double tick) const -> double
    {
        if (this->previous_gap_ == 9 && this->last_gap_ == 9) return 12;
        if (this->last_gap_ == 12 || this->previous_gap_ != 0) return 9;
        const auto phase = std::remainder(tick - this->started_ + 0.25, 10.0);
        return phase >= 1 ? 9 : 12;
    }

    auto recovery_clock::Update(const recovery_sample& sample, const double now) -> void
    {
        if (!sample.resting || sample.hp_max == 0 || sample.hp == 0) { this->Stop(); return; }
        if (!this->active_)
        {
            this->active_ = true; this->sample_ = sample; this->started_ = now; this->due_ = now + 21.4;
            this->late_ = sample.joined_late;
            this->filling_ = !this->Full(); return;
        }
        const bool changed = sample.key != this->sample_.key || sample.hp_max != this->sample_.hp_max || sample.mp_max != this->sample_.mp_max;
        const int hp = static_cast<int>(sample.hp) - static_cast<int>(this->sample_.hp);
        const int mp = static_cast<int>(sample.mp) - static_cast<int>(this->sample_.mp);
        const bool tpLoss = sample.tp < this->sample_.tp;
        const bool window = this->last_tick_ == 0 ? this->late_ || (now - this->started_ >= 19.3 && now - this->started_ <= 23.8) :
            now - this->last_tick_ >= 7.8 && std::abs(now - this->due_) <= 1.4;
        if (changed) this->learned_hp_ = this->learned_mp_ = 0;
        if (!changed && !sample.blocked && now >= this->external_until_ && window && (mp >= 12 || tpLoss ||
            (hp >= 10 && (sample.mp_max == 0 || this->sample_.mp + 12 > sample.mp_max))))
        {
            if (this->last_tick_ != 0)
            {
                const auto gap = now - this->last_tick_;
                this->previous_gap_ = this->last_gap_;
                this->last_gap_ = std::abs(gap - 9) < std::abs(gap - 12) ? 9 : 12;
            }
            ++this->ticks_; this->late_ = false; this->hp_gain_ = std::max(0, hp); this->mp_gain_ = std::max(0, mp);
            if (hp > 0 && sample.hp < sample.hp_max) this->learned_hp_ = static_cast<float>(hp);
            if (mp > 0 && sample.mp < sample.mp_max) this->learned_mp_ = static_cast<float>(mp);
            this->last_tick_ = this->observed_tick_ = now; this->span_ = this->NextGap(now); this->due_ = now + this->span_;
        }
        if (sample.blocked && now >= this->due_)
        {
            ++this->ticks_; this->last_tick_ = this->due_;
            this->previous_gap_ = this->last_gap_; this->last_gap_ = this->span_ == 12 ? 12 : 9;
            this->span_ = this->NextGap(this->last_tick_); this->due_ += this->span_;
        }
        if (now > this->due_ + 3 && !this->Full())
        {
            this->last_tick_ = 0; this->started_ = now - 19.3; this->due_ = now + 2.1;
            this->late_ = true;
            this->previous_gap_ = this->last_gap_ = 0; this->learned_hp_ = this->learned_mp_ = 0;
        }
        this->sample_ = sample;
        if (this->filling_ && this->Full()) { ++this->completion_; this->filling_ = false; }
        if (!this->Full()) this->filling_ = true;
    }

    auto recovery_clock::Predict(const double now) const -> attack_prediction
    {
        attack_prediction result{this->active_, !this->late_, this->last_gap_ != 0, 0, static_cast<float>(std::max(0.0, this->due_ - now)), static_cast<float>(this->span_)};
        result.ratio = std::clamp(1 - result.seconds / std::max(0.1f, result.interval), 0.0f, 1.0f);
        if (this->sample_.blocked) result.state = "Recovery blocked";
        else if (this->late_) result.state = "Waiting for tick";
        else if (this->Full()) result.state = "Full";
        return result;
    }

    auto recovery_clock::SecondsToGoal(const double now, const float percent, const bool hp, const bool mp) const -> float
    {
        if (!this->active_ || this->sample_.blocked || this->late_) return -1;
        float missingHP = hp ? std::max(0.0f, this->sample_.hp_max * percent / 100 - this->sample_.hp) : 0;
        float missingMP = mp ? std::max(0.0f, this->sample_.mp_max * percent / 100 - this->sample_.mp) : 0;
        if (missingHP == 0 && missingMP == 0) return 0;
        float seconds = static_cast<float>(std::max(0.0, this->due_ - now));
        for (int tick = 0; tick < 1000; ++tick)
        {
            missingHP -= (this->learned_hp_ > 0 ? this->learned_hp_ + this->sample_.hp_step : this->sample_.hp_base + this->ticks_ * this->sample_.hp_step) + tick * this->sample_.hp_step;
            missingMP -= (this->learned_mp_ > 0 ? this->learned_mp_ + this->sample_.mp_step : this->sample_.mp_base + this->ticks_ * this->sample_.mp_step) + tick * this->sample_.mp_step;
            if (missingHP <= 0 && missingMP <= 0) return seconds;
            seconds += this->last_gap_ == 9 && this->previous_gap_ == 9 ? (tick % 3 == 0 ? 12 : 9) : (tick % 3 == 2 ? 12 : 9);
        }
        return -1;
    }

    auto tp_clock::Reset(const float perHit, const float attacks, const uint64_t key) -> void
    {
        if (this->key_ == key && this->per_hit_ > 0) return;
        *this = {}; this->key_ = key; this->per_hit_ = std::max(1.0f, perHit); this->hits_per_round_ = std::max(0.1f, attacks * 0.9f);
    }

    auto tp_clock::Round(const double now, const unsigned hits, const uint32_t tp, const bool contaminated) -> void
    {
        if (this->last_round_ != 0 && now >= this->suppress_other_until_ && now - this->last_round_ < 30)
            this->other_per_round_ += (this->other_since_round_ - this->other_per_round_) / 3;
        this->other_since_round_ = 0; this->last_round_ = now;
        this->hits_per_round_ += (hits - this->hits_per_round_) / 4;
        this->before_round_ = tp; this->pending_at_ = now; this->pending_hits_ = contaminated || now < this->quiet_until_ ? 0 : hits;
        this->own_round_ = true;
    }

    auto tp_clock::Update(const double now, const uint32_t tp) -> void
    {
        if (!this->initialized_) { this->last_tp_ = tp; this->initialized_ = true; }
        const int gained = static_cast<int>(tp) - static_cast<int>(this->last_tp_);
        if (this->pending_hits_ && now - this->pending_at_ <= 0.5 && tp > this->before_round_ && tp < 3000 && gained > 0)
        {
            const float perHit = static_cast<float>(tp - this->before_round_) / this->pending_hits_;
            if (perHit >= 10 && perHit <= 500) { this->per_hit_ += (perHit - this->per_hit_) / (this->samples_ ? 3 : 1); ++this->samples_; }
            this->pending_hits_ = 0;
        }
        else if (!this->own_round_ && gained > 0 && tp < 3000 && now >= this->suppress_other_until_)
            this->other_since_round_ += gained;
        if (gained < 0) { this->Contaminate(now); this->other_since_round_ = 0; this->last_round_ = 0; }
        if (now - this->pending_at_ > 0.5) { this->pending_hits_ = 0; this->own_round_ = false; }
        this->last_tp_ = tp;
    }

    auto tp_clock::Contaminate(const double now, const double seconds) -> void
    {
        this->quiet_until_ = std::max(this->quiet_until_, now + seconds); this->pending_hits_ = 0;
        if (seconds > 1) { this->suppress_other_until_ = std::max(this->suppress_other_until_, now + seconds); this->other_since_round_ = 0; }
    }

    auto tp_clock::Predict(const uint32_t tp, const uint32_t goal, const double delay, const double next) const -> tp_prediction
    {
        const auto remaining = static_cast<float>(goal > tp ? goal - tp : 0);
        tp_prediction result{}; result.per_hit = this->per_hit_; result.learned = this->samples_ > 0;
        if (this->per_hit_ <= 0) return result;
        result.hits = static_cast<int>(std::ceil(remaining / this->per_hit_));
        const auto roundTP = this->per_hit_ * this->hits_per_round_ + this->other_per_round_;
        if (roundTP > 0)
        {
            result.rounds = static_cast<int>(std::ceil(remaining / roundTP));
            result.seconds = result.rounds ? static_cast<float>(next + (result.rounds - 1) * delay) : 0;
        }
        return result;
    }
}
