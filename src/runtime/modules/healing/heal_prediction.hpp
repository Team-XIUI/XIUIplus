#pragma once

#include "modules/castbar/player_cast_source.hpp"
#include "modules/castcost/cast_cost_source.hpp"
#include "modules/party/party_source.hpp"
#include "modules/module.hpp"

namespace xiui
{
    struct healing_stats
    {
        uint32_t mind{}, vitality{}, skill{};
        bool divine_seal{}, rapture{};
    };

    auto EstimateHeal(uint32_t spell, const healing_stats& stats, float bonus = 0) -> uint32_t;
    auto HealRecipient(const player_cast_state& cast, uint32_t selectedSpell, uint32_t selectedTarget) -> uint32_t;

    struct heal_prediction
    {
        uint32_t id{}, amount{}, hp{}, maximum{}, percent{};
        std::string name{};
    };

    auto HealPreviewRange(const heal_prediction& prediction, D3DCOLOR color, float ratio = -1) -> ui::meter_range;
    auto OverhealPreviewRange(const heal_prediction& prediction, D3DCOLOR color, float ratio = -1) -> ui::meter_range;

    class heal_prediction_source
    {
    public:
        auto Clear(void) -> void { this->entries_ = {}; this->primary_ = 0; this->sampled_ = 0; }
        auto Update(IAshitaCore* core, const party_source& party, const player_cast_source& cast,
            const cast_cost_source& selection, uint64_t now) -> void;
        auto Find(uint32_t id, uint64_t now) const -> const heal_prediction*;
        auto Primary(uint64_t now) const -> const heal_prediction* { return this->Find(this->primary_, now); }
        auto Load(const settings_store& store) -> void;
        auto Save(const settings_store& store) const -> void;
        auto BonusOption(void) -> module_option;

    private:
        std::array<heal_prediction, 18> entries_{};
        uint32_t primary_{};
        uint64_t sampled_{};
        float bonus_{};
    };

    struct heal_preview_settings
    {
        static constexpr size_t option_count = 5;
        bool enabled{true};
        D3DCOLOR color{0xD066FF99};
        bool overheal{true};
        D3DCOLOR overheal_color{0xFFFF5FC8};
        auto Load(const settings_store& store, const char* section) -> void;
        auto Save(const settings_store& store, const char* section) const -> void;
        auto Option(size_t index, heal_prediction_source* source) -> module_option;
        static auto ColorOption(size_t index) -> bool { return index == 1 || index == 4; }
        auto Ranges(const heal_prediction_source* source, uint32_t id, uint64_t now, float ratio = -1) const -> std::array<ui::meter_range, 2>;
    };
}
