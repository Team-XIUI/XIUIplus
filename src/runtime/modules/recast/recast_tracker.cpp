#include "recast_tracker.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace xiui
{
    auto recast_entry::Remaining(const uint64_t now) const -> float
    { return this->ready || now >= this->expires ? 0 : static_cast<float>(this->expires - now) / 1000; }

    auto recast_entry::Progress(const uint64_t now) const -> float
    { return this->ready ? 1 : this->duration > 0 ? 1 - std::clamp(this->Remaining(now) / this->duration, 0.0f, 1.0f) : 0; }

    auto recast_entry::Visible(const uint64_t now, const float readySeconds) const -> bool
    { return !this->ready || (readySeconds > 0 && now < this->ready_at + static_cast<uint64_t>(readySeconds * 1000)); }

    auto recast_tracker::Observe(const recast_sample& sample, const uint64_t now) -> void
    {
        if (!std::isfinite(sample.remaining) || sample.remaining <= 0 || sample.remaining > 86400 || sample.name.empty()) return;
        auto found = std::find_if(this->entries_.begin(), this->entries_.end(), [&](const auto& entry) {
            return entry.kind == sample.kind && entry.id == sample.id;
        });
        if (found == this->entries_.end())
        {
            this->entries_.push_back({});
            found = this->entries_.end() - 1;
            found->kind = sample.kind; found->id = sample.id;
            found->started = now;
        }
        auto& entry = *found;
        const bool restarted = entry.ready || sample.remaining > entry.Remaining(now) + 0.5f;
        if (restarted) { entry.duration = 0; entry.started = now; }
        entry.duration = sample.charge_duration > 0 ? sample.charge_duration : std::max(entry.duration, sample.remaining);
        entry.name = sample.name; entry.charges = sample.charges; entry.maximum_charges = sample.maximum_charges;
        entry.expires = now + static_cast<uint64_t>(std::ceil(sample.remaining * 1000));
        entry.sampled = now; entry.ready = false; entry.ready_at = 0;
    }

    auto recast_tracker::FinishSample(const uint64_t now) -> void
    {
        for (auto& entry : this->entries_)
            if (entry.sampled != now && !entry.ready)
            {
                entry.ready = true; entry.ready_at = now;
                entry.charges = entry.maximum_charges;
            }
        std::erase_if(this->entries_, [now](const auto& entry) { return entry.ready && now >= entry.ready_at + 30000; });
    }

    auto FormatRecastTime(const float seconds, const bool tenths) -> std::string
    {
        const float remaining = std::isfinite(seconds) ? std::max(0.0f, seconds) : 0;
        char text[32]{};
        const auto whole = static_cast<unsigned>(std::ceil(remaining));
        if (whole >= 3600) std::snprintf(text, sizeof(text), "%u:%02u:%02u", whole / 3600, (whole / 60) % 60, whole % 60);
        else if (whole >= 60) std::snprintf(text, sizeof(text), "%u:%02u", whole / 60, whole % 60);
        else if (tenths) std::snprintf(text, sizeof(text), "%.1f", std::ceil(remaining * 10) / 10);
        else std::snprintf(text, sizeof(text), "%u", whole);
        return text;
    }
}
