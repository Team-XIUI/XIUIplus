#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
#include <optional>
#include <span>

namespace xiui
{
    inline auto DecodeBlueLearned(std::span<const uint8_t> packet, const uint32_t localId) -> std::optional<uint16_t>
    {
        if (localId == 0 || packet.size() < 0x1C) return std::nullopt;
        uint32_t target{}, spell{}; uint16_t message{};
        std::memcpy(&target, packet.data() + 8, 4);
        std::memcpy(&spell, packet.data() + 12, 4);
        std::memcpy(&message, packet.data() + 24, 2);
        if (target != localId || (message & 0x7FFF) != 419 || spell == 0 || spell >= 2048) return std::nullopt;
        return static_cast<uint16_t>(spell);
    }

    struct blue_alert_frame
    {
        bool visible{};
        float entrance{}, scale{1}, alpha{};
    };

    inline auto BlueAlertFrame(const uint64_t age, const float hold) -> blue_alert_frame
    {
        const auto seconds = age / 1000.0f;
        const auto duration = std::clamp(hold, 1.0f, 10.0f);
        if (seconds >= 1.0f + duration + 0.5f) return {};
        blue_alert_frame result{true, 0, 1, 1};
        if (seconds < 0.7f) result.entrance = std::pow(1 - seconds / 0.7f, 3.0f);
        else if (seconds < 1.0f) result.scale += 0.18f * std::sin((seconds - 0.7f) / 0.3f * 3.14159265f);
        else if (seconds > 1.0f + duration) result.alpha = std::clamp(1 - (seconds - 1.0f - duration) / 0.5f, 0.0f, 1.0f);
        return result;
    }

    class blue_magic_alert
    {
    public:
        auto Clear(void) -> void { *this = {}; }
        auto Enqueue(const uint16_t spell, const uint64_t now) -> bool
        {
            for (const auto& event : this->recent_)
                if (event.spell == spell && now >= event.at && now - event.at < 5000) return false;
            if (this->pending_.size() >= 4) return false;
            this->recent_[this->next_recent_++ % this->recent_.size()] = {spell, now};
            this->pending_.push_back(spell);
            return true;
        }
        auto Tick(const uint64_t now, const float hold) -> bool
        {
            if (this->active_ && (now < this->started_ || !BlueAlertFrame(now - this->started_, hold).visible)) this->active_ = false;
            if (this->active_ || this->pending_.empty()) return false;
            this->pending_.pop_front(); this->active_ = true; this->started_ = now;
            return true;
        }
        auto Frame(const uint64_t now, const float hold) const -> blue_alert_frame
        { return this->active_ && now >= this->started_ ? BlueAlertFrame(now - this->started_, hold) : blue_alert_frame{}; }

    private:
        struct recent_event { uint16_t spell{}; uint64_t at{}; };
        std::array<recent_event, 8> recent_{};
        size_t next_recent_{};
        std::deque<uint16_t> pending_{};
        uint64_t started_{};
        bool active_{};
    };
}
