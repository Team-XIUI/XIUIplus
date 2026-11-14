# Minimap

Renders regular/enhanced zone maps with player, party, pet, target, enemy and NPC markers, plus per-zone zoom and configurable edges.

Appearance → Minimap theme offers Follow global theme (default), Standard and XIV 1.0 (Fading circle). The fading circle can be selected with either global theme; Standard restores the saved map shape and edges even while the global theme is XIV 1.0. The choice persists with profiles and uses the shared mouse, keyboard and controller dropdown.

The XIV 1.0 appearance uses a softly fading circular map, a warm north label and a turquoise player marker. The frame, backing and coordinate strips are suppressed while this appearance is active; saved shape and edge preferences are retained. Marker opacity follows the feather, and wheel/hover capture uses the visible circular shape.

Default zoom is 4×. A versioned migration updates the old 3× starting value once, retaining other saved starting values and all per-zone zoom choices. The shipped preset contains no development zone overrides.

**Start in:** [minimap_module.cpp](../../src/runtime/modules/minimap/minimap_module.cpp), [minimap_source.cpp](../../src/runtime/modules/minimap/minimap_source.cpp), [minimap_maps.cpp](../../src/runtime/modules/minimap/minimap_maps.cpp), [contour.hpp](../../src/runtime/ui/contour.hpp).

**Keep intact:** Keep native zone/floor IDs and named option/INI identities stable. Decode asynchronously through the bounded cache. Burnt outlines and marker/cone hit tests must avoid repeated expensive distance work. Do not regenerate map assets for routine rendering changes.

Placement uses the shared committed-position path. Edge drags save the visible anchor and set `layout.minimap.position_initialized`, so negative authored anchors cannot be mistaken for automatic startup placement. Legacy negative drags below -1 remain explicit. Startup and viewport changes preserve manually accepted positions.

Dungeon floors use the native terrain lookup. Discovery matches the floor call and function prologue without depending on surrounding map-display code; both signatures must be unique, the call must target that function, and the terrain pointer must remain inside the client module. Map ownership and AllMaps do not gate the embedded artwork.

Bundled artwork covers the Phoenix-era zones. Zones without it (WotG, Adoulin and later) read the installed client's own map DAT: file ID `5312 + resource`, or `53295 + resource` when the native record's low flag bit is set. The worker decodes paletted (`0xB1`, bottom-up rows) and DXT3 (`0xA1`) images, doubling their 0x80-opaque alpha. Bundled art stays preferred, and Enhanced falls back the same way. `/xiuiplus minimap status` reports the artwork source as bundled, native or none.

**Focused check:** `./scripts/check.ps1 -Name minimap`.

Hovering the visible map claims shared pointer capture for scroll-wheel zoom. Use the same shape/contour hit test for hover and wheel input; captions and transparent corners leave native input available.

Markers → Enemies starts enabled and shows living visible monsters within 120 yalms on the current floor. Dots are red, including party/alliance claims; other groups' claims are purple. The selected enemy keeps the same color under its target ring. NPC and enemy collection share one bounded 150 ms scan; dots reuse the existing draw batches.

Markers → Player icon selects Triangle (default) or Dot. Both retain the marker size, opacity and black outline; the facing cone remains independent.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
