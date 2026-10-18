#pragma once

#include "Ashita.h"
#include "combat/action_packet.hpp"

#include <cmath>
#include <string>

namespace xiui
{
    struct native_cast_sample
    {
        float percent{}, maximum{}, count{};
        uint32_t type{};
        auto Valid(void) const -> bool
        {
            return std::isfinite(this->percent) && std::isfinite(this->maximum) && std::isfinite(this->count) &&
                this->percent >= 0 && this->percent <= 1 && this->maximum > 0 && this->count >= 0 && this->count <= this->maximum;
        }
    };

    struct player_cast_state
    {
        std::string name{};
        uint32_t id{}, target_id{};
        bool item{}, sampled{};
        float progress{};
        uint64_t started{}, changed{}, interrupted{};
        native_cast_sample previous{};

        auto Clear(void) -> void { *this = {}; }
        auto Active(void) const -> bool { return !this->name.empty(); }
        auto Begin(std::string name, uint32_t id, bool item, uint64_t now, native_cast_sample baseline) -> void;
        auto Update(native_cast_sample sample, uint64_t now) -> void;
    };

    class player_cast_source
    {
    public:
        auto Clear(void) -> void { *this = {}; }
        auto State(void) const -> const player_cast_state& { return this->state_; }
        auto Available(void) const -> bool { return this->available_; }
        auto Update(IAshitaCore* core, uint64_t now) -> void;
        auto HandlePacket(IAshitaCore* core, uint16_t id, std::span<const uint8_t> data, uint64_t now, const combat_action_packet* decoded = nullptr) -> void;
        auto Observe(const combat_action& action, uint32_t player, IResourceManager* resources, uint64_t now) -> void;

    private:
        player_cast_state state_{};
        native_cast_sample native_{};
        uint32_t player_{}, zone_{};
        bool available_{};
    };
}
