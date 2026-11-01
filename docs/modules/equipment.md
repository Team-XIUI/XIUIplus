# Equipment

Displays the sixteen equipped slots using native item artwork, with independent layout, labels and passive tooltips.

One native outer container surrounds the grid. Inner slots use flat theme-colored backgrounds and thin rounded outlines with three-pixel corner radii and screen-pixel anti-aliasing. Icon padding and hover geometry share the slot bounds. Native quality borders stay in the untouched item bitmap. The old slot-only setup enables the outer container once; later container choices persist.

**Start in:** [equipment_source.cpp](../../src/runtime/modules/equipment/equipment_source.cpp), [equipment_module.cpp](../../src/runtime/modules/equipment/equipment_module.cpp), [game_icon_cache.cpp](../../src/runtime/ui/game_icon_cache.cpp).

**Keep intact:** Resolve real bag/slot identities. Loading or invalid records must not erase a ready item; confirmed unequips do. Keep the complete DAT bitmap, including native quality borders. Hover never captures input.

**Focused check:** `xiui_inventory_smoke`.

Native Equipment's Storage Options remain part of the native F/Cycle Window sequence, as does native inventory Sort. Shared window-focus routing mirrors replaced chat/status windows without consuming those native cycle stops; see [input ownership](../reference/input.md).

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
