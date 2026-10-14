#pragma once

#include "combat_source.hpp"
#include "timing_model.hpp"

#include <unordered_map>

namespace xiui
{
    class combat_timing_source
    {
    public:
        auto Clear(void) -> void;
        auto Update(IAshitaCore* core, const combat_source& combat, uint64_t now) -> void;
        auto HandlePacket(IAshitaCore* core, const combat_source& combat, uint16_t id, std::span<const uint8_t> data, uint64_t now) -> void;
        auto Player(double now) const -> attack_prediction { return this->attack_.Predict(now); }
        auto Enemy(uint32_t id, double now) const -> attack_prediction;
        auto TP(uint32_t goal, double now) const -> tp_prediction;
        auto Recovery(void) const -> const recovery_clock& { return this->recovery_; }
        auto Count(void) const -> size_t { return this->enemies_.size(); }

    private:
        struct enemy_clock { attack_clock clock{}; uint32_t index{}; uint64_t effect_key{}; float base{4}; };
        attack_clock attack_{};
        recovery_clock recovery_{};
        tp_clock tp_{};
        recovery_sample sample_{};
        std::unordered_map<uint32_t, enemy_clock> enemies_{};
        uint32_t player_{}, zone_{};
        uint64_t next_scan_{}, key_{}, previous_scan_{}, vitals_at_{};
        double estimate_{4};
        bool engaged_{}, initialized_{};
        auto ReadLoadout(IAshitaCore& core) -> void;
        auto Observe(IAshitaCore& core, const combat_action& action, double now) -> void;
    };
}
