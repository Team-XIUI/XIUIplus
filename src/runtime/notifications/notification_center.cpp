#include "notification_center.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace xiui
{
    auto notification_center::Post(notification_event event, const uint64_t now) -> void
    {
        if (!std::isfinite(event.amount) || (!IsTextNotification(event.kind) && event.amount <= 0.0))
            return;
        event.subject.resize(std::min<size_t>(event.subject.size(), 96));
        if (IsInvitation(event.kind)) this->Dismiss(event.kind);
        if (!IsTextNotification(event.kind))
        {
            for (auto entry = this->entries_.rbegin(); entry != this->entries_.rend(); ++entry)
            {
                if (now - entry->updated_at > 350)
                    continue;
                if (entry->event.kind == event.kind && entry->event.key == event.key && entry->event.subject == event.subject)
                {
                    entry->event.amount += event.amount;
                    entry->updated_at = now;
                    return;
                }
            }
        }
        if (this->entries_.size() == capacity)
        {
            const auto oldest = std::find_if(this->entries_.begin(), this->entries_.end(),
                [](const notification_entry& entry) { return !IsInvitation(entry.event.kind); });
            this->entries_.erase(oldest != this->entries_.end() ? oldest : this->entries_.begin());
        }
        this->entries_.push_back({std::move(event), now, now, this->next_id_++});
    }

    auto notification_center::Update(const uint64_t now, const uint64_t lifetime) -> void
    {
        std::erase_if(this->entries_, [now, lifetime](const notification_entry& entry) { return entry.Remaining(now, lifetime) == 0; });
    }

    auto notification_center::Dismiss(const notification_kind kind) -> void
    {
        std::erase_if(this->entries_, [kind](const notification_entry& entry) { return entry.event.kind == kind; });
    }

    auto notification_center::Update(const uint64_t now, const std::span<const uint64_t> lifetimes) -> void
    {
        std::erase_if(this->entries_, [&](const notification_entry& entry) {
            const auto kind = static_cast<size_t>(entry.event.kind);
            return entry.Remaining(now, kind < lifetimes.size() ? lifetimes[kind] : 3200) == 0;
        });
    }

    auto notification_center::Find(const uint64_t id) const -> const notification_entry*
    {
        const auto found = std::find_if(this->entries_.begin(), this->entries_.end(), [id](const auto& entry) { return entry.id == id; });
        return found != this->entries_.end() ? &*found : nullptr;
    }

    auto notification_center::ResolveTrade(const uint32_t sender) -> void
    {
        std::erase_if(this->entries_, [sender](const auto& entry) {
            return entry.event.kind == notification_kind::trade_invite && (sender == 0 || sender == entry.event.key);
        });
    }

    auto DecodeRewardPacket(const uint16_t id, const std::span<const uint8_t> data, const uint32_t playerId) -> std::optional<notification_event>
    {
        if (playerId == 0 || data.size() < 0x1C || (id != 0x0029 && id != 0x002D))
            return std::nullopt;
        const auto word = [data](const size_t offset) -> uint16_t {
            uint16_t value{};
            std::memcpy(&value, data.data() + offset, sizeof(value));
            return value;
        };
        const auto dword = [data](const size_t offset) -> uint32_t {
            uint32_t value{};
            std::memcpy(&value, data.data() + offset, sizeof(value));
            return value;
        };
        const auto message = word(0x18) & 0x7FFF;
        if (id == 0x002D && dword(0x04) == playerId)
        {
            if (message == 8 || message == 253)
                return notification_event{notification_kind::experience, 0, static_cast<double>(dword(0x10)), "XP"};
            if (message == 371 || message == 372)
                return notification_event{notification_kind::experience, 1, static_cast<double>(dword(0x10)), "Limit points"};
        }
        if (id == 0x0029 && message == 38 && dword(0x08) == playerId && dword(0x0C) < 64 && dword(0x10) > 0 && dword(0x10) <= 100)
            return notification_event{notification_kind::skill, dword(0x0C), static_cast<double>(dword(0x10)) / 10.0, {}};
        return std::nullopt;
    }
} // namespace xiui
