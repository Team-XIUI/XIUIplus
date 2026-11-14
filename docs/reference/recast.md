# Recast

Displays native spell/ability cooldowns and shared ability-charge metadata as bars/icons or text-only rows.

**Start in:** [recast_source.cpp](../../src/runtime/modules/recast/recast_source.cpp), [recast_module.cpp](../../src/runtime/modules/recast/recast_module.cpp), [ability_recast.cpp](../../src/runtime/combat/ability_recast.cpp).

**Keep intact:** Deduplicate recast groups and confirm readiness from native samples. Clear stale cycles on session/job changes. The client drops pet commands (Blood Pacts, Release) from known abilities while no pet is out, so timer names fall back to the first resource ability on that timer, and SMN main/sub keeps Rage/Ward known for the Pet HUD. `/xiuiplus recast status` lists tracked names. Charge calculations are shared with Cast Cost and Action Bars; text-only mode retains saved icon/bar preferences and text effects.

**Focused check:** `xiui_casting_smoke`.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
