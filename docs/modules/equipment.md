# Equipment

Displays the sixteen equipped slots using native item artwork, with independent layout, labels and passive tooltips.

**Start in:** [equipment_source.cpp](../../src/runtime/modules/equipment/equipment_source.cpp), [equipment_module.cpp](../../src/runtime/modules/equipment/equipment_module.cpp), [game_icon_cache.cpp](../../src/runtime/ui/game_icon_cache.cpp).

**Keep intact:** Resolve real bag/slot identities. Loading or invalid records must not erase a ready item; confirmed unequips do. Keep the complete DAT bitmap, including native quality borders. Hover never captures input.

**Focused check:** `xiui_inventory_smoke`.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
