#include "experience_module.hpp"
#include "modules/party/party_source.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>

namespace
{
    auto Number(const int64_t amount, const bool abbreviated) -> std::string
    {
        char result[48]{};
        const auto magnitude = std::abs(static_cast<double>(amount));
        if (abbreviated && magnitude >= 1000000) std::snprintf(result, sizeof(result), "%.1fm", amount / 1000000.0);
        else if (abbreviated && magnitude >= 10000) std::snprintf(result, sizeof(result), "%.1fk", amount / 1000.0);
        else std::snprintf(result, sizeof(result), "%lld", amount);
        return result;
    }

    auto Duration(const uint64_t seconds) -> std::string
    {
        char result[48]{};
        if (seconds >= 3600) std::snprintf(result, sizeof(result), "%lluh %02llum", seconds / 3600, seconds / 60 % 60);
        else std::snprintf(result, sizeof(result), "%llu:%02llu", seconds / 60, seconds % 60);
        return result;
    }

    auto Estimate(const uint64_t points, const std::optional<double> rate) -> std::string
    {
        if (points == 0) return "Reached";
        if (!rate || *rate <= 0 || !std::isfinite(*rate)) return "--";
        const double seconds = static_cast<double>(points) * 3600 / *rate;
        if (seconds > 31536000) return "~>1 year";
        return "~" + Duration(static_cast<uint64_t>(std::ceil(seconds)));
    }
}

namespace xiui
{
    auto experience_module::BuildReadouts(const progression_state& progress, const bool preview) -> void
    {
        const auto& tracker = this->source_.tracker;
        const auto& totals = tracker.Totals(progress.kind);
        const auto record = tracker.Record(this->now_);
        const auto& chain = tracker.Chain();
        constexpr uint32_t windows[]{300, 900, 1800, 0};
        const auto window = windows[std::clamp(this->settings_.rate_window, 0, 3)];
        const auto rate = [&](const progression_kind kind) { return tracker.Rate(kind, window, this->settings_.rate_basis == 1, this->now_); };
        const auto number = [&](int64_t value) { return Number(value, this->settings_.short_numbers); };
        const auto rateText = [&](const std::optional<double> value) { return value ? number(static_cast<int64_t>(std::llround(*value))) : "--"; };
        const auto currentRate = rate(progress.kind), expRate = rate(progression_kind::experience), lpRate = rate(progression_kind::limit);
        const auto remaining = progress.needed > progress.current ? progress.needed - progress.current : 0;
        const bool meritKnown = this->progression_ready_ && this->input_.merit_max != 0;
        const bool meritFull = meritKnown && this->input_.merits >= this->input_.merit_max;
        const auto lpNeeded = this->input_.limit < 10000 ? 10000 - this->input_.limit : 0;
        auto recent = tracker.Recent(progress.kind, static_cast<size_t>(this->settings_.recent_count));
        std::sort(recent.begin(), recent.end());
        const uint64_t recentSum = std::accumulate(recent.begin(), recent.end(), uint64_t{});
        using reading = std::pair<std::string, std::string>;
        std::array<reading, experience_readout_count> values{};
        const auto put = [&](experience_readout id, std::string label, std::string value) { values[static_cast<size_t>(id)] = {std::move(label), std::move(value)}; };
        const char* unit = progress.kind == progression_kind::limit ? "LP" : progress.kind == progression_kind::capacity ? "CP" : progress.kind == progression_kind::mastery ? "EP" : "EXP";
        put(experience_readout::current_required, "", progress.available ? FormatProgressionInfo(progress, true, false, false, false, false) : "--");
        put(experience_readout::percent, "", progress.available ? FormatProgressionInfo(progress, false, true, false, false, false) : "--");
        put(experience_readout::remaining, "", progress.available ? this->settings_.label_position == 2 ? std::to_string(remaining) :
            FormatProgressionInfo(progress, false, false, true, false, false) : "--");
        put(experience_readout::rate, std::string{unit} + "/hr", rateText(currentRate));
        put(experience_readout::eta, progress.kind == progression_kind::experience && progress.level >= 75 ? "Buffer ETA" : "Next",
            !progress.available ? "--" : progress.kind == progression_kind::limit && meritFull ? "Spend merits" : Estimate(remaining, currentRate));
        put(experience_readout::last, "Last", totals.count ? number(totals.last) : "--");
        put(experience_readout::received, "Session gained", number(static_cast<int64_t>(totals.received)));
        put(experience_readout::retained, "Session net", number(totals.Net()));
        put(experience_readout::elapsed, "Session", Duration(record.elapsed / 1000));
        put(experience_readout::average, "Average", totals.count ? number(static_cast<int64_t>(totals.received / totals.count)) : "--");
        put(experience_readout::recent_average, "Recent avg", recent.empty() ? "--" : number(static_cast<int64_t>(recentSum / recent.size())));
        put(experience_readout::median, "Median", recent.empty() ? "--" : number(recent.size() % 2 ? recent[recent.size() / 2] :
            static_cast<int64_t>((uint64_t{recent[recent.size() / 2 - 1]} + recent[recent.size() / 2]) / 2)));
        put(experience_readout::lowest, "Low", recent.empty() ? "--" : number(recent.front()));
        put(experience_readout::highest, "High", recent.empty() ? "--" : number(recent.back()));
        put(experience_readout::count, "Rewards", number(static_cast<int64_t>(totals.count)));
        put(experience_readout::lost, "Lost", number(static_cast<int64_t>(totals.lost)));
        put(experience_readout::recovered, "Recovered", number(static_cast<int64_t>(tracker.Totals(progression_kind::experience).recovered)));
        put(experience_readout::best_chain, "Best chain", number(chain.best));
        put(experience_readout::chain, "Chain", chain.count ? "#" + std::to_string(chain.count) + " (+" + std::to_string(chain.Bonus()) + "%)" : "--");
        put(experience_readout::chain_timer, "Chain left", chain.Remaining(this->now_) ? std::string{chain.estimated ? "~" : ""} + Duration(chain.Remaining(this->now_)) : "--");
        char meritRate[32]{};
        if (lpRate) std::snprintf(meritRate, sizeof(meritRate), "%.2f", *lpRate / 10000);
        put(experience_readout::merit_rate, "Merits/hr", lpRate ? meritRate : "--");
        put(experience_readout::merits_earned, "Merits earned", number(record.merits));
        put(experience_readout::merits_free, "Merit slots", meritKnown ? number(this->input_.merit_max > this->input_.merits ? this->input_.merit_max - this->input_.merits : 0) : "--");
        put(experience_readout::lp_needed, "LP needed", !meritKnown ? "--" : meritFull ? "Spend merits" : number(lpNeeded));
        put(experience_readout::merit_eta, "Next merit", !meritKnown ? "--" : meritFull ? "Spend merits" : Estimate(lpNeeded, lpRate));
        put(experience_readout::buffer, "EXP buffer", this->progression_ready_ ? number(this->input_.exp) : "--");
        const auto goal = this->GoalRemaining();
        std::string goalText = "--";
        if (goal)
        {
            if (*goal == 0) goalText = "Reached";
            else
            {
                const bool merits = this->settings_.goal_type == 3 || this->settings_.goal_type == 4;
                const uint64_t points = merits ? *goal * 10000 - std::min<uint64_t>(*goal * 10000, this->input_.limit) : *goal;
                goalText = number(static_cast<int64_t>(*goal)) + " left (" + Estimate(points, merits ? lpRate : expRate) + ")";
            }
        }
        put(experience_readout::goal, "Goal", goalText);
        const auto effectText = [&](const experience_effect& effect) {
            const auto seconds = effect.Remaining(this->now_);
            return !this->source_.ready ? std::string{"--"} : !effect.active ? std::string{"None"} : seconds ? "~" + Duration(*seconds) : std::string{"Active (--)"};
        };
        put(experience_readout::food, "Food", effectText(this->source_.food));
        put(experience_readout::dedication, "Dedication", effectText(this->source_.dedication));
        std::string ring = "--";
        const auto& expRing = this->source_.ring;
        if (expRing.id != 0)
        {
            ring = expRing.name; ring += expRing.charges >= 0 ? " " + std::to_string(expRing.charges) + " charges" : " -- charges";
            ring += expRing.charges == 0 ? " (Empty)" : !expRing.recast ? " (--)" : *expRing.recast == 0 ? " (Ready)" : " (" + Duration(*expRing.recast) + ")";
        }
        put(experience_readout::ring, "Ring", ring);
        const auto& sample = tracker.Sample();
        put(experience_readout::sync, "Sync", !this->source_.ready ? "--" : this->source_.sync_active ?
            std::to_string(this->input_.level) + " > " + std::to_string(sample.effective_level) : "Off");
        put(experience_readout::sync_penalty, "Applied penalty", this->source_.sync_penalty ? std::to_string(*this->source_.sync_penalty) + "%" : "--");
        put(experience_readout::dedication_pool, "Bonus left", this->source_.dedication_pool ? number(*this->source_.dedication_pool) : "--");
        const auto planned = static_cast<uint32_t>(this->settings_.sync_level);
        put(experience_readout::sync_plan, "Planned penalty", !this->progression_ready_ || planned > this->input_.level ? "--" :
            "~" + std::to_string(SyncPenalty(this->input_.level, planned, static_cast<uint32_t>(this->settings_.sync_population))) + "%");
        std::string status = !this->source_.ready ? "Waiting for character" : tracker.Paused() ? "Paused" :
            !this->source_.no_exp_reason.empty() ? this->source_.no_exp_reason : meritFull && progress.kind == progression_kind::limit ? "Spend merits" : !currentRate ? "Collecting (1m)" : "Tracking";
        put(experience_readout::status, "Status", status);
        put(experience_readout::exp_rate, "EXP/hr", rateText(expRate)); put(experience_readout::lp_rate, "LP/hr", rateText(lpRate));
        put(experience_readout::exp_received, "EXP received", number(static_cast<int64_t>(tracker.Totals(progression_kind::experience).received)));
        put(experience_readout::lp_received, "LP received", number(static_cast<int64_t>(tracker.Totals(progression_kind::limit).received)));
        std::string jobs = "--";
        if (this->source_.ready)
        {
            jobs = std::string{PartyJobName(this->input_.job)} + std::to_string(this->input_.level);
            if (sample.sub_job > 0 && sample.sub_job <= 22) jobs += " / " + std::string{PartyJobName(sample.sub_job)} + std::to_string(sample.sub_level);
            if (this->source_.sync_active)
            {
                jobs += " (Sync " + std::to_string(sample.effective_level);
                if (sample.sub_job > 0 && sample.sub_job <= 22) jobs += "/" + std::to_string(sample.effective_sub_level);
                jobs += ")";
            }
        }
        put(experience_readout::job, "Job", jobs);
        if (preview)
        {
            put(experience_readout::rate, std::string{unit} + "/hr", "8,500"); put(experience_readout::eta, "Next", "~12:30");
            put(experience_readout::received, "Session gained", "12,750"); put(experience_readout::chain, "Chain", "#3 (+30%)"); put(experience_readout::chain_timer, "Chain left", "~0:45");
            if (!this->source_.ready) put(experience_readout::job, "Job", "WAR75 / NIN37");
        }
        auto order = DefaultExperienceReadoutOrder();
        std::sort(order.begin(), order.end(), [&](auto a, auto b) { return this->settings_.order[a] < this->settings_.order[b]; });
        this->readout_values_.clear();
        for (const auto index : order)
        {
            if (!this->settings_.readouts[index]) continue;
            const auto& [label, value] = values[index];
            const bool warning = index == static_cast<size_t>(experience_readout::chain_timer) && chain.Remaining(this->now_) != 0 &&
                chain.Remaining(this->now_) <= static_cast<uint32_t>(this->settings_.chain_warning);
            this->readout_values_.push_back({label.empty() || this->settings_.label_position == 2 ? value : this->settings_.label_position == 0 ? label + " " + value : value + " " + label,
                warning, static_cast<experience_readout>(index)});
        }
    }
}
