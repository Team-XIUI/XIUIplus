#pragma once

#include <cstdint>
#include <span>
#include <unordered_map>
#include <utility>

namespace xiui
{
    using item_totals = std::unordered_map<uint16_t, uint32_t>;

    class inventory_tracker
    {
    public:
        auto Reset(uint64_t now) -> void
        {
            *this = {};
            this->Invalidate(now);
        }

        auto Invalidate(uint64_t now) -> void
        {
            this->items_.clear();
            this->ready_ = false;
            this->settled_at_ = now + 300;
            this->next_scan_ = now;
        }

        auto BeginSync(uint64_t now) -> void
        {
            this->Invalidate(now);
            this->syncing_ = true;
        }

        auto HandleSyncPacket(std::span<const uint8_t> data, uint64_t now) -> void
        {
            if (data.size() < 12 || data[4] > 1)
                return;
            this->syncing_ = data[4] == 0;
            this->settled_at_ = now + 300;
        }

        auto CanSample(uint64_t now, uint32_t counter, uint32_t flags, uint32_t requiredFlags) -> bool
        {
            if (flags != this->flags_ || requiredFlags != this->required_flags_)
            {
                this->Invalidate(now);
                this->flags_ = flags;
                this->required_flags_ = requiredFlags;
            }
            if (counter != this->counter_)
            {
                this->counter_ = counter;
                this->settled_at_ = now + 300;
            }
            return !this->syncing_ && requiredFlags != 0 && (flags & requiredFlags) == requiredFlags &&
                now >= this->settled_at_ && now >= this->next_scan_;
        }

        auto Sample(item_totals totals, uint64_t now) -> item_totals
        {
            item_totals gains;
            if (this->ready_)
            {
                for (const auto& [id, count] : totals)
                {
                    const auto previous = this->items_.find(id);
                    const auto oldCount = previous != this->items_.end() ? previous->second : 0;
                    if (count > oldCount)
                        gains.emplace(id, count - oldCount);
                }
            }
            this->items_ = std::move(totals);
            this->ready_ = true;
            this->next_scan_ = now + 500;
            return gains;
        }

    private:
        item_totals items_{};
        uint32_t counter_{}, flags_{}, required_flags_{};
        uint64_t settled_at_{}, next_scan_{};
        bool ready_{}, syncing_{};
    };
} // namespace xiui
