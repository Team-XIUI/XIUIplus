# Heal prediction service

Supplies optional healing and overheal estimates to Player, Party and the world target bar. It has no independent HUD window.

**Start in:** [heal_prediction.cpp](../../src/runtime/modules/healing/heal_prediction.cpp), [heal_prediction.hpp](../../src/runtime/modules/healing/heal_prediction.hpp).

**Keep intact:** Estimates use known cast, target and potency data; they are not server-confirmed outcomes. Clear invalid/stale targets and keep the feature’s enable states and colors independent of ordinary HP rendering.

**Focused check:** `./scripts/check.ps1 -Name heal-prediction`.

[Detailed guide](../reference/heal-prediction.md) · [Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
