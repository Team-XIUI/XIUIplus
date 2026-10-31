#pragma once

#include <cstdint>

namespace xiui
{
    struct native_target_selection
    {
        bool action_target{}, subtarget{}, engaged{};
        uint32_t action_id{};
    };

    inline auto NativeTargetSelectionActive(const native_target_selection& state) -> bool
    {
        // Auto-attack keeps the flag set without a pending action ID.
        return state.subtarget || (state.action_target && (!state.engaged || state.action_id != UINT32_MAX));
    }

    template<typename Memory>
    inline auto NativeTargetSelectionActive(const Memory* memory) -> bool
    {
        const auto* target = memory != nullptr ? memory->GetTarget() : nullptr;
        if (target == nullptr) return false;
        native_target_selection state{target->GetActionTargetActive() != 0, target->GetIsSubTargetActive() != 0, false, target->GetActionId()};
        if (state.action_target && !state.subtarget && state.action_id == UINT32_MAX)
        {
            const auto* party = memory->GetParty();
            const auto* entity = memory->GetEntity();
            const auto index = party != nullptr ? party->GetMemberTargetIndex(0) : 0;
            state.engaged = entity != nullptr && index != 0 && index < entity->GetEntityMapSize() &&
                party->GetMemberServerId(0) != 0 && entity->GetServerId(index) == party->GetMemberServerId(0) && entity->GetStatus(index) == 1;
        }
        return NativeTargetSelectionActive(state);
    }
}
