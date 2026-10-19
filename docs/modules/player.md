# Player Bar

Owns local HP/MP/TP presentation, identity text, state colors and optional attack/rest timing, MP-cost and healing overlays.

**Start in:** [player_module.cpp](../../src/runtime/modules/player/player_module.cpp), [combat_timing_source.cpp](../../src/runtime/combat/combat_timing_source.cpp), [mp_cost_preview.hpp](../../src/runtime/modules/castcost/mp_cost_preview.hpp).

**Keep intact:** Preserve independent resource labels/values and their migration keys. Use native maxima, shared meter layout and `TintTextProfile`. Optional previews/feedback must not replace confirmed resource values.

**Focused check:** `./scripts/check.ps1 -Name player-resources`; use `mp-preview` or `heal-prediction` for overlays.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
