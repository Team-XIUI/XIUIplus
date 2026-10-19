# Cast Bar

Displays the local player’s current spell or item-use progress, completion and interruption. Cast Cost and Recast have separate state and placement.

**Start in:** [player_cast_source.cpp](../../src/runtime/modules/castbar/player_cast_source.cpp), [cast_bar_module.cpp](../../src/runtime/modules/castbar/cast_bar_module.cpp), [cast_adjustments.hpp](../../src/runtime/modules/castbar/cast_adjustments.hpp).

**Keep intact:** Use confirmed local actions and native progress. Clear stale casts on death, zoning, logout or identity changes; never write native action selection. Keep interruption colors in the normal text profile.

**Focused check:** `xiui_casting_smoke`.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
