#include "modules/experience/experience_packets.hpp"

#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace
{
    auto Check(bool condition, const char* message) -> void { if (!condition) throw std::runtime_error{message}; }
    auto Sample(void) -> xiui::experience_sample
    {
        xiui::experience_sample sample{42, 100, 50};
        sample.progress.job = 1; sample.progress.level = 50; sample.progress.exp = 7500; sample.progress.exp_needed = 7800;
        sample.progress.merit_max = 30; return sample;
    }
    auto Advance(xiui::experience_tracker& tracker, const xiui::experience_sample& sample, uint64_t begin, uint64_t end) -> void
    { for (auto now = begin; now <= end; now += 1000) tracker.Observe(sample, now, 1700000000); }

    auto Sessions(void) -> void
    {
        using namespace xiui;
        experience_tracker tracker;
        auto sample = Sample(); tracker.Observe(sample, 1000, 1700000000);
        Check(!tracker.Started(), "idle time before the first reward does not lower rates");
        tracker.Award(progression_kind::experience, 400, 1, true, 1000, 1700000000);
        sample.progress.level = 51; sample.effective_level = 51; sample.progress.exp = 100; sample.progress.exp_needed = 8000;
        tracker.Observe(sample, 1250, 1700000000);
        Check(tracker.Totals(progression_kind::experience).received == 400 && tracker.Totals(progression_kind::experience).Net() == 400,
            "a reward crossing a level counts once in received and retained progress");
        Check(!tracker.Rate(progression_kind::experience, 300, false, 60000), "rates wait for sixty active seconds");
        Advance(tracker, sample, 2000, 61000);
        Check(std::abs(*tracker.Rate(progression_kind::experience, 300, false, 61000) - 24000) < 0.001,
            "received EXP per hour uses session time after warm-up");
        tracker.Pause(true, 61000);
        const auto pausedTime = tracker.Elapsed(61000);
        sample.progress.exp += 500; tracker.Observe(sample, 62000, 1700000000);
        tracker.Award(progression_kind::experience, 500, 2, true, 62000, 1700000000);
        Check(tracker.Totals(progression_kind::experience).received == 400 && tracker.Elapsed(70000) == pausedTime && tracker.Chain().count == 2,
            "pauses exclude gains and time while chain timers continue");
        tracker.Pause(false, 70000); tracker.Observe(sample, 70000, 1700000000);
        tracker.Award(progression_kind::experience, 200, 3, true, 71000, 1700000000);
        sample.progress.exp += 200; tracker.Observe(sample, 71000, 1700000000);
        const auto deadline = tracker.Chain().deadline;
        tracker.Reset(71000, 1700000000);
        Check(tracker.History().size() == 1 && tracker.History()[0].totals[0].received == 600 &&
            tracker.Chain().deadline == deadline && tracker.Chain().best == 0 && !tracker.Rate(progression_kind::experience, 300, false, 72000),
            "reset archives totals and clears rates without restarting the active chain");
        tracker.Award(progression_kind::experience, 100, 0, false, 72000, 1700000000);
        Advance(tracker, sample, 72000, 433000);
        Check(*tracker.Rate(progression_kind::experience, 300, false, 433000) == 0,
            "old rewards age out and an idle rolling rate falls to zero");
        tracker.Discontinuity(); sample.progress.exp += 1000;
        tracker.Observe(sample, 434000, 1700000000);
        Check(tracker.Totals(progression_kind::experience).Net() == 0, "zoning and observation gaps never reconstruct unobserved gains");
    }

    auto MeritsAndLosses(void) -> void
    {
        using namespace xiui;
        experience_tracker tracker;
        auto sample = Sample(); sample.progress.level = 75; sample.progress.exp_needed = 44000;
        sample.progress.limit = 9900; sample.progress.merits = 3;
        tracker.Observe(sample, 1000, 1700000000);
        tracker.Award(progression_kind::limit, 200, 1, true, 1000, 1700000000);
        sample.progress.limit = 100; sample.progress.merits = 4; tracker.Observe(sample, 1250, 1700000000);
        sample.progress.merits = 1; tracker.Observe(sample, 1500, 1700000000);
        Check(tracker.Totals(progression_kind::limit).Net() == 200 && tracker.Record(1500).merits == 1,
            "merit conversion handles LP rollover and spending never subtracts earned merits");
        sample.dead = true; sample.progress.exp -= 1000; tracker.Observe(sample, 1750, 1700000000);
        sample.dead = false; sample.progress.exp += 900; tracker.Observe(sample, 2000, 1700000000);
        Advance(tracker, sample, 2250, 4250);
        Check(tracker.Totals(progression_kind::experience).lost == 1000 && tracker.Totals(progression_kind::experience).recovered == 900,
            "death losses and observed recovery stay separate from reported EXP awards");
        sample.progress.exp += 200; tracker.Observe(sample, 4500, 1700000000);
        tracker.Award(progression_kind::experience, 200, 0, false, 4750, 1700000000);
        Advance(tracker, sample, 5000, 7000);
        Check(tracker.Totals(progression_kind::experience).recovered == 900, "delayed award packets cannot be misclassified as resurrection recovery");
        sample.progress.exp = 43999;
        Check(SelectProgression(sample.progress).kind == progression_kind::limit, "Phoenix's full level-75 EXP buffer overflows into limit display");
        sample.progress.level = 75; sample.effective_level = 30; sample.progress.limit_mode = true;
        Check(SelectProgression(sample.progress).kind == progression_kind::limit, "actual job level retains merit availability during Level Sync");
        Check(LevelDistance(50, 7500, 52) == 8300 && LevelDistance(99, 0, 75) == 0 && !LevelDistance(75, 0, 76) && SyncPenalty(75, 30, 1000) == 50 && SyncPenalty(50, 40, 2000) == 0,
            "level goals and the separately labeled Phoenix sync calculator use bounded rules");
    }

    auto PacketsAndChains(void) -> void
    {
        using namespace xiui;
        std::array<uint8_t, 28> packet{};
        const auto put = [&](size_t offset, uint32_t value, size_t bytes) { std::memcpy(packet.data() + offset, &value, bytes); };
        put(4, 42, 4); put(8, 42, 4); put(16, 125, 4); put(20, 3, 4); put(24, 253, 2);
        Check(DecodeExperienceAward(packet, 42)->chain == 3 && !DecodeExperienceAward(packet, 41), "awards retain chain data and require the local recipient");
        for (size_t i = 0; i < packet.size(); ++i) Check(!DecodeExperienceAward(std::span<const uint8_t>{packet}.first(i), 42), "truncated EXP packets fail closed");
        put(8, 99, 4); Check(!DecodeExperienceAward(packet, 42), "another recipient cannot contribute local rewards");
        put(8, 42, 4); put(16, 0xFFFFFFFF, 4); Check(!DecodeExperienceAward(packet, 42), "malformed amounts are rejected");
        experience_chain chain; chain.Observe(1, 50, 1000);
        Check(chain.Remaining(1000) == 240 && chain.Bonus() == 20, "Phoenix chain one uses the shortest eligible next-chain window");
        chain.Observe(5, 50, 2000);
        Check(chain.Remaining(2000) == 60 && chain.Bonus() == 50 && chain.Remaining(62000) == 0, "chain five has its capped bonus and an expiring estimate");
        auto sample = Sample();
        const std::string message = "MOM1|42|100|1|50|50|1|1700000000|0|0|0|1|500|50|120|1";
        std::vector<uint8_t> report(24 + message.size()); report[4] = 29; report[6] = 100;
        std::memcpy(report.data() + 8, "Momentum", 8); std::memcpy(report.data() + 23, message.data(), message.size());
        Check(DecodeMomentumReport(report, sample)->values[11] == 500, "validated server reports can supply otherwise unknown Dedication pools");
        report[4] = 5; Check(!DecodeMomentumReport(report, sample), "ordinary player chat cannot supply server-only status");
        report[4] = 29; sample.player = 99; Check(!DecodeMomentumReport(report, sample), "server reports for other characters are ignored");
    }
}

auto main(void) -> int
{
    try { Sessions(); MeritsAndLosses(); PacketsAndChains(); std::puts("EXP session/rate, pause/reset, merit, loss/recovery, goal, chain and packet checks passed."); return 0; }
    catch (const std::exception& error) { std::fprintf(stderr, "EXP check failed: %s\n", error.what()); return 1; }
}
