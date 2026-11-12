# Mob information data

Provides static monster metadata to Target’s configurable information groups. This data service has no separate input surface.

**Start in:** [mob_database.cpp](../../src/runtime/modules/mobinfo/mob_database.cpp), [target_mob_info.cpp](../../src/runtime/modules/target/target_mob_info.cpp), [phoenix_spawn_data.hpp](../../src/runtime/resources/phoenix_spawn_data.hpp).

**Keep intact:** Keep generated data/provenance separate from native observations. Do not present catalog estimates as current server state. Target owns group placement and visibility; unavailable groups should not occupy layout.

Weaknesses and Resistances list only damage-taken changes (damage types, absorbs and physical/magic/ranged/breath kinds), strongest first; neutral values never appear. Resistance ranks change magic accuracy, not damage, so they stay in the optional rank groups.

XIV 1.0 attaches Together-mode Job, Behavior, Links and Detection cells above the right-hand Target caption through `DrawHeaderIcons`. It measures the visible cells before right alignment and defaults to two-pixel spacing. Own-row and independent groups keep the regular renderer and placement.

**Focused check:** `./scripts/check.ps1 -Name mob-info`.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
