#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace xiui
{
    enum class experience_readout : size_t
    {
        rate, eta, last, received, retained, elapsed, average, recent_average, median, lowest, highest, count, lost, recovered,
        best_chain, chain, chain_timer, merit_rate, merits_earned, merits_free, lp_needed, merit_eta, buffer, goal, food, dedication,
        ring, sync, sync_penalty, status, exp_rate, lp_rate, exp_received, lp_received, job, dedication_pool, sync_plan,
        current_required, remaining, percent, count_readouts
    };
    inline constexpr size_t experience_readout_count = static_cast<size_t>(experience_readout::count_readouts);
    enum class experience_readout_group : uint8_t { session, combat, progress, bonuses };
    struct experience_readout_definition { const char* key; const char* name; const char* description; experience_readout_group group; };
    inline constexpr std::array<experience_readout_definition, experience_readout_count> experience_readouts{{
        {"rate", "Points / hour", "Hourly rate for the bar's active point type.", experience_readout_group::session},
        {"eta", "Time to next", "Estimated time to the next level or point at your current rate.", experience_readout_group::progress},
        {"last", "Last reward", "The most recent point award.", experience_readout_group::combat},
        {"received", "Session points gained", "Points awarded since this session began; separate from your current balance.", experience_readout_group::session},
        {"retained", "Session net points", "Session gains after observed losses and recovery.", experience_readout_group::session},
        {"elapsed", "Session time", "Active tracking time; excludes manual pauses.", experience_readout_group::session},
        {"average", "Average reward", "Average award across the entire session.", experience_readout_group::combat},
        {"recent_average", "Recent average", "Average of the recent rewards selected in Tracking.", experience_readout_group::combat},
        {"median", "Recent median", "Middle value among recent rewards.", experience_readout_group::combat},
        {"lowest", "Lowest recent reward", "Smallest award in your recent reward window.", experience_readout_group::combat},
        {"highest", "Highest recent reward", "Largest award in your recent reward window.", experience_readout_group::combat},
        {"count", "Rewards counted", "Number of awards recorded this session.", experience_readout_group::session},
        {"lost", "Points lost", "Observed point losses during this session.", experience_readout_group::session},
        {"recovered", "EXP recovered", "EXP recovered during this session.", experience_readout_group::session},
        {"best_chain", "Best chain", "Highest EXP chain reached this session.", experience_readout_group::combat},
        {"chain", "EXP chain", "Current chain and its EXP bonus.", experience_readout_group::combat},
        {"chain_timer", "Chain countdown", "Time left to continue the EXP chain.", experience_readout_group::combat},
        {"merit_rate", "Merits / hour", "LP gain rate expressed as merits per hour.", experience_readout_group::progress},
        {"merits_earned", "Merits earned", "Merits earned since the session began.", experience_readout_group::session},
        {"merits_free", "Merit slots free", "Remaining space in your merit-point storage.", experience_readout_group::progress},
        {"lp_needed", "LP to next merit", "LP still needed; repeats Remaining when the bar shows LP.", experience_readout_group::progress},
        {"merit_eta", "Time to next merit", "Merit ETA even when the bar shows another point type.", experience_readout_group::progress},
        {"buffer", "EXP buffer", "Current EXP balance; repeats Current / required in EXP mode.", experience_readout_group::progress},
        {"goal", "Goal progress", "Progress toward the target selected in Tracking.", experience_readout_group::progress},
        {"food", "Food timer", "Your current food status and known remaining duration.", experience_readout_group::bonuses},
        {"dedication", "Dedication timer", "Your current Dedication status and known duration.", experience_readout_group::bonuses},
        {"ring", "EXP ring", "EXP ring charges and known recast time.", experience_readout_group::bonuses},
        {"sync", "Level Sync", "Actual level and the current synced level.", experience_readout_group::bonuses},
        {"sync_penalty", "Applied sync penalty", "The applied penalty, only when supplied by the server.", experience_readout_group::bonuses},
        {"status", "Tracking status", "Whether tracking is active, paused or waiting for data.", experience_readout_group::session},
        {"exp_rate", "EXP / hour (always)", "EXP rate independent of the bar's active point type.", experience_readout_group::session},
        {"lp_rate", "LP / hour (always)", "LP rate independent of the bar's active point type.", experience_readout_group::session},
        {"exp_received", "Session EXP (always)", "EXP gained this session, regardless of the active bar mode.", experience_readout_group::session},
        {"lp_received", "Session LP (always)", "LP gained this session, regardless of the active bar mode.", experience_readout_group::session},
        {"job", "Job / subjob and levels", "Main job, subjob, actual levels and separate synced levels.", experience_readout_group::progress},
        {"dedication_pool", "Dedication bonus left", "Known remaining bonus EXP from Dedication.", experience_readout_group::bonuses},
        {"sync_plan", "Sync penalty calculator", "A planning estimate using Tracking settings; not an applied penalty.", experience_readout_group::bonuses},
        {"current_required", "Current / required", "Current points / required points. Adding it moves this value out of bar info.", experience_readout_group::progress},
        {"remaining", "Remaining", "Points left to the next level or point. Adding it moves this value out of bar info.", experience_readout_group::progress},
        {"percent", "Percent", "Completion percentage. Adding it moves this value out of bar info.", experience_readout_group::progress}}};

    inline auto DefaultExperienceReadouts(void) -> std::array<bool, experience_readout_count>
    {
        std::array<bool, experience_readout_count> result{};
        for (auto id : {experience_readout::job, experience_readout::rate, experience_readout::eta, experience_readout::received, experience_readout::chain, experience_readout::chain_timer})
            result[static_cast<size_t>(id)] = true;
        return result;
    }
    inline auto DefaultExperienceReadoutOrder(void) -> std::array<uint8_t, experience_readout_count>
    {
        std::array<uint8_t, experience_readout_count> result{};
        for (size_t i = 0; i < result.size(); ++i) result[i] = static_cast<uint8_t>(i);
        const auto job = static_cast<size_t>(experience_readout::job);
        for (size_t i = 0; i < job; ++i) ++result[i];
        result[job] = 0;
        return result;
    }
    struct experience_readout_text { std::string text{}; bool warning{}; experience_readout id{experience_readout::count_readouts}; };
}
