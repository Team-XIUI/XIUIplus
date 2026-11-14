# Compass

Draws the native compass artwork, nearby native radar dots and tracked party/alliance/selected-target markers using the shared world source. Nearby dots use the client's entity-visibility check, actor visibility and floor-height limit. Enemy eligibility reads the native radar setting, preserving job restrictions. Tracked members and selected targets are deduplicated against nearby dots.

**Start in:** [compass_module.cpp](../../src/runtime/modules/compass/compass_module.cpp), [world_source.cpp](../../src/runtime/world/world_source.cpp).

**Keep intact:** Retain native camera rotation, flattened geometry, sprite tints and blends. Do not reveal entities beyond confirmed client data. Compass and Time / Weather retain separate enable states and positions.

**Focused check:** `xiui_world_smoke`.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
