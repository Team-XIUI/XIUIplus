# Treasure Pool

Shows observed treasure entries, lot history and explicit Lot/Pass actions. Reward notifications remain separate.

Container opacity defaults to 0.35. A one-time migration replaces the old 0.95 default (including slider rounding) while preserving other custom opacities.

Show member decisions defaults on under General. While the pool is visible, party/alliance name rows show the highlighted item's native decision: `[---]` waiting, `[Pass]` passed, or `[number]` lotted. Pointer hover and controller navigation select the inspected item without taking a treasure action. The selected pool row is highlighted, and the same context reaches both alliance groups. Data is keyed by server ID and item generation; unavailable, out-of-zone and non-player members have no indicator. Empty pools, zoning and hidden/disabled pool presentation clear the context.

**Start in:** [treasure_source.cpp](../../src/runtime/modules/treasure/treasure_source.cpp), [treasure_module.cpp](../../src/runtime/modules/treasure/treasure_module.cpp), [hud_runtime_treasure.cpp](../../src/runtime/hud_runtime_treasure.cpp).

**Keep intact:** Revalidate slot generation/identity before acting. Pointer controls work over ordinary native menus, while unclaimed keyboard/controller input remains native. Opening or viewing details must not lot/pass; held confirmation must not repeat requests.

**Focused check:** `./scripts/check.ps1 -Name treasure-input`.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
