# Cast Cost and MP preview

Reads the selected native action to display its cost and supply MP-cost overlays on resource bars.

**Start in:** [cast_cost_source.cpp](../../src/runtime/modules/castcost/cast_cost_source.cpp), [cast_cost_module.cpp](../../src/runtime/modules/castcost/cast_cost_module.cpp), [mp_cost_preview.hpp](../../src/runtime/modules/castcost/mp_cost_preview.hpp).

**Keep intact:** Unsupported or missing menu/action data must remain unavailable. Player, Party and target previews consume this shared source; ability charges use `combat/ability_recast` rather than separate formulas.

**Focused check:** `./scripts/check.ps1 -Name mp-preview`.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
