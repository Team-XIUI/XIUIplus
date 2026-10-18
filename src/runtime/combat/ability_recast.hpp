#pragma once

#include "Ashita.h"

namespace xiui
{
    struct ability_charge_pool
    {
        uint32_t maximum{};
        float seconds{};
    };

    struct charge_recast
    {
        uint32_t charges{}, maximum{};
        float remaining{}, duration{};
    };

    auto AbilityChargePool(IAshitaCore* core, uint32_t timer) -> ability_charge_pool;
    auto CalculateChargeRecast(uint32_t frames, ability_charge_pool pool, int16_t modifier = 0) -> charge_recast;
}
