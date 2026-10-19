# Treasure Pool

Shows observed treasure entries, lot history and explicit Lot/Pass actions. Reward notifications remain separate.

**Start in:** [treasure_source.cpp](../../src/runtime/modules/treasure/treasure_source.cpp), [treasure_module.cpp](../../src/runtime/modules/treasure/treasure_module.cpp), [hud_runtime_treasure.cpp](../../src/runtime/hud_runtime_treasure.cpp).

**Keep intact:** Revalidate slot generation/identity before acting. Pointer controls work over ordinary native menus, while unclaimed keyboard/controller input remains native. Opening or viewing details must not lot/pass; held confirmation must not repeat requests.

**Focused check:** `./scripts/check.ps1 -Name treasure-input`.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
