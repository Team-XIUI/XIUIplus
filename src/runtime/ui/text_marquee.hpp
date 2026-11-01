#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace xiui::ui
{
    class text_marquee
    {
    public:
        auto BeginFrame(void) -> void { ++this->frame_; }

        auto Offset(uint64_t key, float distance, bool active, uint64_t now) -> float
        {
            auto found = std::find_if(this->entries_.begin(), this->entries_.end(),
                [key](const auto& entry) { return entry.used && entry.key == key; });
            if (!active || !std::isfinite(distance) || distance <= 0)
            {
                if (found != this->entries_.end()) found->used = false;
                return 0;
            }
            if (found == this->entries_.end())
            {
                found = std::min_element(this->entries_.begin(), this->entries_.end(), [](const auto& left, const auto& right) {
                    if (left.used != right.used) return !left.used;
                    return left.seen < right.seen;
                });
                *found = {key, now, now, this->frame_, distance, true};
            }
            if (found->frame + 1 < this->frame_ || now < found->seen || now - found->seen > 250 || found->distance != distance)
                found->began = now;
            found->seen = now; found->frame = this->frame_; found->distance = distance;
            constexpr double pause = 900;
            const double travel = std::max(500.0, static_cast<double>(distance) / 30 * 1000);
            const double phase = std::fmod(static_cast<double>(now - found->began), (pause + travel) * 2);
            if (phase <= pause) return 0;
            if (phase < pause + travel) return distance * static_cast<float>((phase - pause) / travel);
            if (phase <= pause * 2 + travel) return distance;
            return distance * static_cast<float>(1 - (phase - pause * 2 - travel) / travel);
        }

    private:
        struct entry
        {
            uint64_t key{}, began{}, seen{}, frame{};
            float distance{};
            bool used{};
        };
        std::array<entry, 32> entries_{};
        uint64_t frame_{};
    };
}
