#pragma once

#include <cstdint>
#include <deque>
#include <optional>
#include <span>
#include <string>

namespace xiui
{
    enum class notification_kind : uint8_t { item, experience, skill, message, gil, key_item, treasure, party_invite, trade_invite };
    inline auto IsTextNotification(const notification_kind kind) -> bool
    { return kind == notification_kind::message || kind == notification_kind::key_item || kind == notification_kind::treasure || kind == notification_kind::party_invite || kind == notification_kind::trade_invite; }
    inline auto IsItemNotification(const notification_kind kind) -> bool
    { return kind == notification_kind::item || kind == notification_kind::gil || kind == notification_kind::key_item || kind == notification_kind::treasure; }
    inline auto IsInvitation(const notification_kind kind) -> bool
    { return kind == notification_kind::party_invite || kind == notification_kind::trade_invite; }

    struct notification_event
    {
        notification_kind kind{notification_kind::message};
        uint32_t key{};
        double amount{};
        std::string subject{};
    };

    struct notification_entry
    {
        static constexpr uint64_t party_invite_lifetime = 90000;
        notification_event event{};
        uint64_t created_at{};
        uint64_t updated_at{};
        uint64_t id{};

        auto Lifetime(uint64_t fallback) const -> uint64_t
        { return this->event.kind == notification_kind::party_invite ? party_invite_lifetime : this->event.kind == notification_kind::trade_invite ? 30000 : fallback; }
        auto Remaining(uint64_t now, uint64_t fallback) const -> uint64_t
        {
            const auto lifetime = this->Lifetime(fallback);
            const auto elapsed = now >= this->updated_at ? now - this->updated_at : 0;
            return elapsed < lifetime ? lifetime - elapsed : 0;
        }
    };

    class notification_center
    {
    public:
        static constexpr size_t capacity = 32;
        auto Post(notification_event event, uint64_t now) -> void;
        auto Update(uint64_t now, uint64_t lifetime) -> void;
        auto Update(uint64_t now, std::span<const uint64_t> lifetimes) -> void;
        auto Dismiss(notification_kind kind) -> void;
        auto Find(uint64_t id) const -> const notification_entry*;
        auto ResolveTrade(uint32_t sender) -> void;
        auto Clear(void) -> void { this->entries_.clear(); }
        auto Entries(void) const -> const std::deque<notification_entry>& { return this->entries_; }

    private:
        std::deque<notification_entry> entries_{};
        uint64_t next_id_{1};
    };

    auto DecodeRewardPacket(uint16_t id, std::span<const uint8_t> data, uint32_t playerId) -> std::optional<notification_event>;
} // namespace xiui
