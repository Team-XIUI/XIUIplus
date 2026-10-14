#include "experience_tracker.hpp"

#include <algorithm>
#include <cmath>

namespace xiui
{
    // Phoenix's published EXP requirements; client thresholds must match before level arithmetic.
    auto LevelRequirement(const uint32_t level) -> uint32_t
    {
        if (level == 0 || level > 99) return 0;
        if (level <= 5) return 250 + level * 250;
        if (level == 6) return 1750;
        if (level <= 22) return 2000 + (level - 7) * 200;
        if (level <= 50) return 5100 + (level - 23) * 100;
        if (level <= 61) return 8000 + (level - 51) * 1200;
        if (level <= 69) return 21500 + (level - 62) * 1500;
        if (level <= 75) return 34000 + (level - 70) * 2000;
        return 44500 + (level - 76) * 500;
    }

    auto LevelDistance(const uint32_t level, const uint32_t current, const uint32_t target) -> std::optional<uint64_t>
    {
        if (level == 0 || level > 99 || target > 75 || target == 0) return std::nullopt;
        if (target <= level) return 0;
        uint64_t total{};
        for (auto i = level; i < target; ++i) total += LevelRequirement(i);
        return total > current ? total - current : 0;
    }

    auto SyncPenalty(const uint32_t actual, const uint32_t effective, const uint32_t population) -> uint32_t
    {
        if (actual <= effective || actual - effective <= 10) return 0;
        const auto rate = population >= 2000 ? 25u : population >= 1000 ? 20u : population >= 500 ? 15u : 0u;
        return std::min(50u, (actual - effective) * rate / 10);
    }

    auto experience_chain::Observe(const uint32_t chain, const uint32_t effectiveLevel, const uint64_t now) -> void
    {
        if (chain == 0) return;
        constexpr uint32_t levels[]{9, 19, 29, 39, 49, 59, 255};
        constexpr uint32_t windows[][5]{{80, 60, 40, 30, 15}, {130, 110, 80, 60, 25}, {150, 120, 90, 60, 30},
            {200, 170, 130, 80, 40}, {290, 230, 170, 110, 50}, {300, 240, 180, 120, 60}, {300, 240, 180, 120, 60}};
        uint32_t seconds = 300;
        for (size_t i = 0; i < std::size(levels); ++i)
            if (levels[i] >= std::max(1u, effectiveLevel)) seconds = std::min(seconds, windows[i][std::min(chain, 4u)]);
        this->count = chain; this->best = std::max(this->best, chain);
        this->deadline = now + uint64_t{seconds} * 1000; this->estimated = true;
    }

    auto experience_tracker::Elapsed(const uint64_t now) const -> uint64_t
    {
        return this->elapsed_ + (this->clock_running_ && now >= this->resumed_ ? now - this->resumed_ : 0);
    }

    auto experience_tracker::Start(const uint64_t now, const uint32_t utc) -> void
    {
        if (this->started_) return;
        this->started_ = true; this->record_.utc = utc;
        this->resumed_ = now; this->clock_running_ = !this->paused_;
    }

    auto experience_tracker::Discontinuity(void) -> void
    {
        if (this->clock_running_) this->elapsed_ = this->Elapsed(this->clock_);
        this->clock_running_ = this->continuous_ = this->observed_ = false;
        this->revived_at_ = this->unmatched_award_ = 0; this->recovery_.clear(); this->chain_.Clear();
    }

    auto experience_tracker::Add(const progression_kind kind, const int64_t amount, const bool received, const uint64_t now) -> void
    {
        const auto second = this->Elapsed(now) / 1000;
        if (this->buckets_.empty() || this->buckets_.back().second != second) this->buckets_.push_back({second});
        while (!this->buckets_.empty() && this->buckets_.front().second + 1800 < second) this->buckets_.pop_front();
        auto& bucket = this->buckets_.back();
        (received ? bucket.received : bucket.retained)[ProgressionIndex(kind)] += amount;
    }

    auto experience_tracker::Progress(const progression_kind kind, const int64_t delta, const bool recovery, const uint64_t now, const uint32_t utc) -> void
    {
        if (delta == 0) return;
        this->Start(now, utc);
        auto& totals = this->record_.totals[ProgressionIndex(kind)];
        if (delta < 0) totals.lost += static_cast<uint64_t>(-delta);
        else
        {
            totals.gained += static_cast<uint64_t>(delta);
            if (recovery)
            {
                const auto matched = now >= this->award_at_ && now - this->award_at_ <= 2000 ? std::min<uint64_t>(delta, this->unmatched_award_) : 0;
                this->unmatched_award_ -= matched;
                if (static_cast<uint64_t>(delta) > matched) this->recovery_.push_back({static_cast<uint64_t>(delta) - matched, now});
            }
        }
        this->Add(kind, delta, false, now);
    }

    auto experience_tracker::Observe(const experience_sample& sample, const uint64_t now, const uint32_t utc) -> void
    {
        if (sample.player == 0 || sample.progress.job == 0) { this->Discontinuity(); return; }
        if (this->record_.player != sample.player || this->record_.job != sample.progress.job)
        {
            this->Archive(this->clock_);
            const auto paused = this->paused_;
            this->record_ = {}; this->elapsed_ = 0; this->started_ = this->clock_running_ = this->continuous_ = false;
            this->buckets_.clear(); this->recent_ = {}; this->chain_ = {}; this->paused_ = paused;
            this->window_origin_ = 0; this->recovery_.clear(); this->unmatched_award_ = 0;
            this->record_.player = sample.player; this->record_.job = sample.progress.job;
        }
        if (now < this->clock_ || now - this->clock_ > 2000 || sample.zone != this->sample_.zone) this->Discontinuity();
        if (this->started_ && !this->paused_ && !this->clock_running_) { this->resumed_ = now; this->clock_running_ = true; }
        while (!this->recovery_.empty() && now >= this->recovery_.front().time && now - this->recovery_.front().time > 2000)
        {
            auto& totals = this->record_.totals[0];
            totals.recovered += std::min(this->recovery_.front().amount, totals.lost > totals.recovered ? totals.lost - totals.recovered : 0);
            this->recovery_.pop_front();
        }
        const auto& before = this->sample_.progress;
        const auto& after = sample.progress;
        if (sample.dead) this->chain_.Clear();
        if (this->sample_.dead && !sample.dead) this->revived_at_ = now;
        if (this->continuous_ && !this->paused_)
        {
            int64_t exp = static_cast<int64_t>(after.exp) - before.exp;
            bool known = before.level == after.level;
            if (!known && after.level == before.level + 1 && before.exp_needed > 0) { exp += before.exp_needed; known = true; }
            else if (!known && before.level == after.level + 1 && after.exp_needed > 0) { exp -= after.exp_needed; known = true; }
            else if (!known && before.exp_needed == LevelRequirement(before.level) && after.exp_needed == LevelRequirement(after.level))
            {
                known = before.level > 0 && after.level > 0 && before.level <= 99 && after.level <= 99;
                for (auto level = std::min(before.level, after.level); known && level < std::max(before.level, after.level); ++level)
                    exp += (after.level > before.level ? 1 : -1) * static_cast<int64_t>(LevelRequirement(level));
            }
            if (known) this->Progress(progression_kind::experience, exp, sample.dead || this->sample_.dead ||
                (this->revived_at_ != 0 && now - this->revived_at_ <= 15000), now, utc);
            if (before.merit_max != 0 && after.merit_max != 0)
            {
                const auto merits = after.merits > before.merits ? after.merits - before.merits : 0;
                this->record_.merits += merits;
                const auto lp = static_cast<int64_t>(after.limit) - before.limit + int64_t{merits} * 10000;
                this->Progress(progression_kind::limit, lp, false, now, utc);
            }
            if (before.level >= 99 && after.level >= 99)
            {
                const auto oldPoints = before.job_points + before.spent, newPoints = after.job_points + after.spent;
                if (newPoints >= oldPoints)
                    this->Progress(progression_kind::capacity, static_cast<int64_t>(after.capacity) - before.capacity + int64_t{newPoints - oldPoints} * 30000, false, now, utc);
                if (before.mastery_job == after.job && after.mastery_job == after.job)
                {
                    auto delta = static_cast<int64_t>(after.mastery) - before.mastery;
                    bool knownMastery = before.master_level == after.master_level;
                    if (after.master_level == before.master_level + 1 && before.mastery_needed > 0) { delta += before.mastery_needed; knownMastery = true; }
                    else if (before.master_level == after.master_level + 1 && after.mastery_needed > 0) { delta -= after.mastery_needed; knownMastery = true; }
                    if (knownMastery) this->Progress(progression_kind::mastery, delta, false, now, utc);
                }
            }
        }
        this->sample_ = sample; this->clock_ = now; this->continuous_ = this->observed_ = true;
    }

    auto experience_tracker::Award(const progression_kind kind, const uint32_t amount, const uint32_t chain, const bool battle, const uint64_t now, const uint32_t utc) -> void
    {
        if (amount == 0 || amount > 0x7FFFFFFF || !this->observed_) return;
        const auto best = this->chain_.best;
        if (battle && (kind == progression_kind::experience || kind == progression_kind::limit)) this->chain_.Observe(chain, this->sample_.effective_level, now);
        if (this->paused_) { this->chain_.best = best; return; }
        this->Start(now, utc);
        if (kind == progression_kind::experience)
        {
            uint64_t remaining = amount;
            for (auto& candidate : this->recovery_)
                if (now >= candidate.time && now - candidate.time <= 2000)
                { const auto matched = std::min(remaining, candidate.amount); candidate.amount -= matched; remaining -= matched; }
            if (now < this->award_at_ || now - this->award_at_ > 2000) this->unmatched_award_ = 0;
            this->unmatched_award_ += remaining; this->award_at_ = now;
        }
        auto& totals = this->record_.totals[ProgressionIndex(kind)];
        totals.received += amount; ++totals.count; totals.last = amount;
        totals.minimum = totals.minimum == 0 ? amount : std::min(totals.minimum, amount); totals.maximum = std::max(totals.maximum, amount);
        auto& recent = this->recent_[ProgressionIndex(kind)];
        recent.push_back(amount); if (recent.size() > 100) recent.pop_front();
        this->Add(kind, amount, true, now);
    }

    auto experience_tracker::Pause(const bool paused, const uint64_t now) -> void
    {
        if (paused == this->paused_) return;
        this->elapsed_ = this->Elapsed(now); this->resumed_ = now;
        this->clock_running_ = !paused && this->started_ && this->continuous_;
        this->paused_ = paused;
        this->continuous_ = false; this->recovery_.clear(); this->unmatched_award_ = 0;
    }

    auto experience_tracker::Record(const uint64_t now) const -> experience_record
    {
        auto result = this->record_; result.elapsed = this->Elapsed(now); result.best_chain = this->chain_.best; return result;
    }

    auto experience_tracker::Archive(const uint64_t now) -> void
    {
        if (!this->started_) return;
        this->history_.push_front(this->Record(now));
        if (this->history_.size() > 10) this->history_.pop_back();
    }

    auto experience_tracker::Reset(const uint64_t now, const uint32_t utc) -> void
    {
        this->Archive(now);
        this->record_ = {}; this->record_.player = this->sample_.player; this->record_.job = this->sample_.progress.job;
        this->record_.utc = utc; this->elapsed_ = 0; this->resumed_ = now;
        this->started_ = this->clock_running_ = false; this->buckets_.clear(); this->recent_ = {};
        this->window_origin_ = 0; this->recovery_.clear(); this->unmatched_award_ = 0;
        this->chain_.best = 0;
        this->continuous_ = false;
    }

    auto experience_tracker::Restore(const experience_record& record, const bool paused) -> void
    {
        this->record_ = record; this->elapsed_ = record.elapsed;
        this->paused_ = paused; this->started_ = record.utc != 0;
        this->chain_.best = record.best_chain;
        this->window_origin_ = record.elapsed;
    }

    auto experience_tracker::Rate(const progression_kind kind, const uint32_t windowSeconds, const bool retained, const uint64_t now) const -> std::optional<double>
    {
        const auto elapsed = this->Elapsed(now);
        if (!this->started_ || elapsed < 60000) return std::nullopt;
        const auto& totals = this->Totals(kind);
        if (windowSeconds == 0)
            return static_cast<double>(retained ? totals.Net() : static_cast<int64_t>(totals.received)) * 3600000.0 / static_cast<double>(elapsed);
        const uint64_t window = std::min<uint64_t>(windowSeconds * 1000ull, elapsed >= this->window_origin_ ? elapsed - this->window_origin_ : 0);
        if (window < 60000) return std::nullopt;
        const auto first = (elapsed - window) / 1000, last = elapsed / 1000;
        int64_t sum{};
        for (const auto& bucket : this->buckets_)
            if (bucket.second >= first && bucket.second <= last)
                sum += (retained ? bucket.retained : bucket.received)[ProgressionIndex(kind)];
        return static_cast<double>(sum) * 3600000.0 / static_cast<double>(window);
    }

    auto experience_tracker::Recent(const progression_kind kind, const size_t maximum) const -> std::vector<uint32_t>
    {
        const auto& values = this->recent_[ProgressionIndex(kind)];
        return {values.end() - std::min(maximum, values.size()), values.end()};
    }
}
