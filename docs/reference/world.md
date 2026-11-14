# Time / Weather

Displays native Vana’diel time, weekday, weather and date as stacked, single-row or independently placed elements.

**Start in:** [world_source.cpp](../../src/runtime/world/world_source.cpp), [world_state.hpp](../../src/runtime/world/world_state.hpp), [world_info_module.cpp](../../src/runtime/modules/world/world_info_module.cpp).

**Keep intact:** Unavailable native time stays unavailable; do not substitute the computer clock. Keep independent element bounds/settings stable and reuse shared text effects. Clock sampling and Compass rendering share data, not module enable state.

**Focused check:** `xiui_world_smoke`.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
