# Minimap

Renders regular/enhanced zone maps with player, party, pet, target, enemy and NPC markers, plus per-zone zoom and configurable edges.

The XIV 1.0 appearance uses a softly fading circular map, a warm north label and a turquoise player marker. The frame, backing and coordinate strips are suppressed while this appearance is active; saved shape and edge preferences are retained. Marker opacity follows the feather, and wheel/hover capture uses the visible circular shape.

Default zoom is 4×. A versioned migration updates the old 3× starting value once, retaining other saved starting values and all per-zone zoom choices. The shipped preset contains no development zone overrides.

**Start in:** [minimap_module.cpp](../../src/runtime/modules/minimap/minimap_module.cpp), [minimap_source.cpp](../../src/runtime/modules/minimap/minimap_source.cpp), [minimap_maps.cpp](../../src/runtime/modules/minimap/minimap_maps.cpp), [contour.hpp](../../src/runtime/ui/contour.hpp).

**Keep intact:** Keep native zone/floor IDs and named option/INI identities stable. Decode asynchronously through the bounded cache. Burnt outlines and marker/cone hit tests must avoid repeated expensive distance work. Do not regenerate map assets for routine rendering changes.

**Focused check:** `./scripts/check.ps1 -Name minimap`.

Hovering the visible map claims shared pointer capture for scroll-wheel zoom. Use the same shape/contour hit test for hover and wheel input; captions and transparent corners leave native input available.

Markers → Enemies starts enabled and shows living visible monsters within 120 yalms on the current floor. Dots are red, including party/alliance claims; other groups' claims are purple. The selected enemy keeps the same color under its target ring. NPC and enemy collection share one bounded 150 ms scan; dots reuse the existing draw batches.

Markers → Player icon selects Triangle (default) or Dot. Both retain the marker size, opacity and black outline; the facing cone remains independent.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
