#pragma once

#include <cstdint>

namespace xiui
{
    struct native_target_selection
    {
        bool action_target{}, subtarget{};
        uint32_t action_id{};
    };

    inline auto NativeTargetSelectionActive(const native_target_selection& state) -> bool
    {
        // The client can retain the action flag after clearing its pending ID.
        return state.subtarget || (state.action_target && state.action_id != UINT32_MAX);
    }

    template<typename Memory>
    inline auto NativeTargetSelectionActive(const Memory* memory) -> bool
    {
        const auto* target = memory != nullptr ? memory->GetTarget() : nullptr;
        if (target == nullptr) return false;
        return NativeTargetSelectionActive(native_target_selection{target->GetActionTargetActive() != 0,
            target->GetIsSubTargetActive() != 0, target->GetActionId()});
    }
}
