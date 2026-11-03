#include "ability_recast.hpp"
#include "ashita_compat.hpp"

#include <algorithm>
#include <cmath>

namespace xiui
{
    auto AbilityChargePool(IAshitaCore* core, const uint32_t timer) -> ability_charge_pool
    {
        auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        auto player = memory != nullptr ? xiui::ashita::View(memory->GetPlayer()) : nullptr;
        auto* resources = core != nullptr ? core->GetResourceManager() : nullptr;
        if (player == nullptr) return {};
        if (timer == 102)
        {
            const auto* ready = resources != nullptr ? resources->GetAbilityByName("Ready", 2) : nullptr;
            return ready != nullptr && player->HasAbility(ready->Id) ? ability_charge_pool{3, 90} : ability_charge_pool{};
        }
        if (timer == 195) return {2, 120};
        if (timer == 231)
        {
            const int level = player->GetMainJob() == 20 ? player->GetMainJobLevel() : player->GetSubJob() == 20 ? player->GetSubJobLevel() : 0;
            return {static_cast<uint32_t>(level >= 10 ? std::min(5, (level - 10) / 20 + 1) : 0), 240};
        }
        return {};
    }

    auto CalculateChargeRecast(const uint32_t frames, const ability_charge_pool pool, const int16_t modifier) -> charge_recast
    {
        const float poolSeconds = pool.seconds + modifier;
        if (pool.maximum == 0 || !std::isfinite(poolSeconds) || poolSeconds <= 0) return {};
        const double perCharge = poolSeconds * 60.0 / pool.maximum;
        charge_recast result{};
        result.maximum = pool.maximum;
        result.charges = pool.maximum - std::min(pool.maximum, static_cast<uint32_t>(std::ceil(frames / perCharge)));
        result.duration = static_cast<float>(perCharge / 60);
        result.remaining = frames == 0 ? 0 : static_cast<float>((std::fmod(frames - 1.0, perCharge) + 1) / 60);
        return result;
    }
}
