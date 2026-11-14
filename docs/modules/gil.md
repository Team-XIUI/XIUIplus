# Gil

Shows the native balance and observed earnings in its own movable HUD window.

**Start in:** [gil_source.cpp](../../src/runtime/modules/gil/gil_source.cpp), [gil_module.cpp](../../src/runtime/modules/gil/gil_module.cpp).

**Keep intact:** Read gil from inventory container 0, slot 0, item ID 65535. Retain the native icon and separate balance/session readiness from formatting. Loading and session changes must not fabricate earnings.

**Focused check:** `xiui_inventory_smoke`.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
