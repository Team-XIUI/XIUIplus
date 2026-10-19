# Minimap

Renders regular/enhanced zone maps with player, party, pet, target and optional NPC markers, plus per-zone zoom and configurable edges.

**Start in:** [minimap_module.cpp](../../src/runtime/modules/minimap/minimap_module.cpp), [minimap_source.cpp](../../src/runtime/modules/minimap/minimap_source.cpp), [minimap_maps.cpp](../../src/runtime/modules/minimap/minimap_maps.cpp), [contour.hpp](../../src/runtime/ui/contour.hpp).

**Keep intact:** Keep native zone/floor IDs and named option/INI identities stable. Decode asynchronously through the bounded cache. Burnt outlines and marker/cone hit tests must avoid repeated expensive distance work. Do not regenerate map assets for routine rendering changes.

**Focused check:** `./scripts/check.ps1 -Name minimap`.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
