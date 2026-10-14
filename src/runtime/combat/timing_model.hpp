#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace xiui
{
    struct attack_prediction
    {
        bool active{}, known{}, learned{};
        float ratio{}, seconds{}, interval{};
        std::string_view state{};
    };

    class attack_clock
    {
    public:
        auto Begin(double now, double delay, bool late = false) -> void;
        auto Round(double now, double estimate, uint64_t key, bool enemy = false) -> void;
        auto Hold(double now, unsigned reason) -> void;
        auto Release(double now, unsigned reason, double tail = 0) -> void;
        auto AddHold(double now, double seconds) -> void;
        auto Change(double estimate, uint64_t key, bool wait = false) -> void;
        auto Stall(std::string_view reason) -> void { this->stalled_ = reason; this->clean_ = false; }
        auto Predict(double now) const -> attack_prediction;
        auto Stop(void) -> void { *this = {}; }
        auto Interval(void) const -> double { return this->delay_; }
        auto LastSeen(void) const -> double { return this->seen_; }

    private:
        double start_{}, due_{}, seen_{}, frozen_{}, delay_{4};
        std::array<double, 6> gaps_{};
        size_t count_{}, next_{};
        uint64_t key_{};
        unsigned holds_{};
        bool active_{}, clean_{}, learned_{}, waiting_{};
        std::string_view stalled_{};
    };

    struct recovery_sample
    {
        uint32_t hp{}, hp_max{}, mp{}, mp_max{}, tp{};
        bool resting{}, blocked{}, joined_late{};
        float hp_base{10}, hp_step{1}, mp_base{12}, mp_step{1};
        uint64_t key{};
    };

    class recovery_clock
    {
    public:
        auto Update(const recovery_sample& sample, double now) -> void;
        auto Contaminate(double now) -> void { this->external_until_ = now + 1; }
        auto Predict(double now) const -> attack_prediction;
        auto SecondsToGoal(double now, float percent, bool hp = true, bool mp = true) const -> float;
        auto LastHP(void) const -> int { return this->hp_gain_; }
        auto LastMP(void) const -> int { return this->mp_gain_; }
        auto LastObservedTick(void) const -> double { return this->observed_tick_; }
        auto Full(void) const -> bool { return this->sample_.hp >= this->sample_.hp_max && this->sample_.mp >= this->sample_.mp_max; }
        auto Completion(void) const -> uint64_t { return this->completion_; }
        auto Stop(void) -> void { const auto completion = this->completion_; *this = {}; this->completion_ = completion; }

    private:
        recovery_sample sample_{};
        double started_{}, last_tick_{}, observed_tick_{}, due_{}, previous_gap_{}, last_gap_{}, span_{21.4}, external_until_{};
        int ticks_{}, hp_gain_{}, mp_gain_{};
        float learned_hp_{}, learned_mp_{};
        bool active_{}, filling_{}, late_{};
        uint64_t completion_{};
        auto NextGap(double tick) const -> double;
    };

    struct tp_prediction { int rounds{-1}, hits{-1}; float seconds{-1}, per_hit{}; bool learned{}; };

    class tp_clock
    {
    public:
        auto Reset(float perHit, float attacks, uint64_t key) -> void;
        auto Round(double now, unsigned hits, uint32_t tp, bool contaminated = false) -> void;
        auto Update(double now, uint32_t tp) -> void;
        auto Contaminate(double now, double seconds = 0.5) -> void;
        auto Predict(uint32_t tp, uint32_t goal, double delay, double next) const -> tp_prediction;

    private:
        float per_hit_{}, hits_per_round_{1}, other_per_round_{}, other_since_round_{};
        uint64_t key_{};
        uint32_t last_tp_{}, before_round_{};
        double pending_at_{}, quiet_until_{}, suppress_other_until_{}, last_round_{};
        unsigned pending_hits_{}, samples_{};
        bool initialized_{}, own_round_{};
    };
}
