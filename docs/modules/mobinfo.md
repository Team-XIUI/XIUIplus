# Mob information data

Provides static monster metadata to Target’s configurable information groups. This data service has no separate input surface.

**Start in:** [mob_database.cpp](../../src/runtime/modules/mobinfo/mob_database.cpp), [target_mob_info.cpp](../../src/runtime/modules/target/target_mob_info.cpp), [phoenix_spawn_data.hpp](../../src/runtime/resources/phoenix_spawn_data.hpp).

**Keep intact:** Keep generated data/provenance separate from native observations. Do not present catalog estimates as current server state. Target owns group placement and visibility; unavailable groups should not occupy layout.

**Focused check:** `./scripts/check.ps1 -Name mob-info`.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
