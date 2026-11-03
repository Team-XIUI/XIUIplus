#pragma once

#include "ashita_compat.hpp"

#include <cstdint>

namespace xiui
{
    struct native_target_selection
    {
        bool subtarget{};
        uintptr_t action_callback{}, cancel_callback{};
    };

    inline auto NativeTargetSelectionActive(const native_target_selection& state) -> bool
    {
        // Action flags and IDs can outlive selection; the client clears its callbacks on completion.
        return state.subtarget || state.action_callback != 0 || state.cancel_callback != 0;
    }

    template<typename Memory>
    inline auto NativeTargetSelectionActive(const Memory* memory) -> bool
    {
        const auto target = memory != nullptr ? xiui::ashita::View(memory->GetTarget()) : nullptr;
        if (target == nullptr) return false;
        return NativeTargetSelectionActive(native_target_selection{target->GetIsSubTargetActive() != 0,
            target->GetActionCallback(), target->GetCancelCallback()});
    }
}
