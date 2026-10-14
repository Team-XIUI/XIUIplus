#pragma once

#include "progression.hpp"

#include <array>
#include <deque>
#include <optional>
#include <vector>

namespace xiui
{
    inline constexpr size_t progression_kind_count = 4;
    inline auto ProgressionIndex(progression_kind kind) -> size_t { return static_cast<size_t>(kind); }
    auto LevelRequirement(uint32_t level) -> uint32_t;
    auto LevelDistance(uint32_t level, uint32_t current, uint32_t target) -> std::optional<uint64_t>;
    auto SyncPenalty(uint32_t actual, uint32_t effective, uint32_t population) -> uint32_t;

    struct experience_sample
    {
        uint32_t player{}, zone{}, effective_level{};
        progression_input progress{};
        bool dead{};
        uint32_t sub_job{}, sub_level{}, effective_sub_level{};
    };

    struct experience_totals
    {
        uint64_t received{}, gained{}, lost{}, recovered{}, count{};
        uint32_t last{}, minimum{}, maximum{};
        auto Net(void) const -> int64_t { return static_cast<int64_t>(gained) - static_cast<int64_t>(lost); }
    };

    struct experience_record
    {
        uint32_t player{}, job{}, utc{}, best_chain{}, merits{};
        uint64_t elapsed{};
        std::array<experience_totals, progression_kind_count> totals{};
    };

    struct experience_chain
    {
        uint32_t count{}, best{};
        uint64_t deadline{};
        bool estimated{true};
        auto Remaining(uint64_t now) const -> uint32_t { return now < deadline ? static_cast<uint32_t>((deadline - now + 999) / 1000) : 0; }
        auto Bonus(void) const -> uint32_t { return count == 0 ? 0 : count == 1 ? 20 : count == 2 ? 25 : count == 3 ? 30 : count == 4 ? 40 : 50; }
        auto Observe(uint32_t chain, uint32_t effectiveLevel, uint64_t now) -> void;
        auto Clear(void) -> void { count = 0; deadline = 0; estimated = true; }
    };

    class experience_tracker
    {
    public:
        auto Observe(const experience_sample& sample, uint64_t now, uint32_t utc) -> void;
        auto Award(progression_kind kind, uint32_t amount, uint32_t chain, bool battle, uint64_t now, uint32_t utc) -> void;
        auto Discontinuity(void) -> void;
        auto Pause(bool paused, uint64_t now) -> void;
        auto Reset(uint64_t now, uint32_t utc) -> void;
        auto Restore(const experience_record& record, bool paused) -> void;
        auto Record(uint64_t now) const -> experience_record;
        auto Elapsed(uint64_t now) const -> uint64_t;
        auto Rate(progression_kind kind, uint32_t windowSeconds, bool retained, uint64_t now) const -> std::optional<double>;
        auto Recent(progression_kind kind, size_t maximum) const -> std::vector<uint32_t>;
        auto Started(void) const -> bool { return this->started_; }
        auto Paused(void) const -> bool { return this->paused_; }
        auto Sample(void) const -> const experience_sample& { return this->sample_; }
        auto Chain(void) -> experience_chain& { return this->chain_; }
        auto Chain(void) const -> const experience_chain& { return this->chain_; }
        auto History(void) -> std::deque<experience_record>& { return this->history_; }
        auto History(void) const -> const std::deque<experience_record>& { return this->history_; }
        auto Totals(progression_kind kind) const -> const experience_totals& { return this->record_.totals[ProgressionIndex(kind)]; }

    private:
        struct bucket
        {
            uint64_t second{};
            std::array<int64_t, progression_kind_count> received{}, retained{};
        };
        experience_sample sample_{};
        experience_record record_{};
        experience_chain chain_{};
        std::deque<bucket> buckets_{};
        std::deque<experience_record> history_{};
        std::array<std::deque<uint32_t>, progression_kind_count> recent_{};
        struct recovery_candidate { uint64_t amount{}, time{}; };
        std::deque<recovery_candidate> recovery_{};
        uint64_t elapsed_{}, clock_{}, resumed_{}, revived_at_{}, window_origin_{}, unmatched_award_{}, award_at_{};
        bool started_{}, paused_{}, continuous_{}, clock_running_{}, observed_{};
        auto Start(uint64_t now, uint32_t utc) -> void;
        auto Add(progression_kind kind, int64_t amount, bool received, uint64_t now) -> void;
        auto Progress(progression_kind kind, int64_t delta, bool recovery, uint64_t now, uint32_t utc) -> void;
        auto Archive(uint64_t now) -> void;
    };
}
