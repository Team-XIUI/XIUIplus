# Inventory dots

Six independently placed trackers show occupied slots and capacity for inventory and supported storage groups. Satchel’s browser is a separate module.

**Start in:** [inventory_source.cpp](../../src/runtime/modules/inventory/inventory_source.cpp), [inventory_module.cpp](../../src/runtime/modules/inventory/inventory_module.cpp).

**Keep intact:** Count actual native capacity and omit inaccessible containers. Keep tracker identities, grouping and saved positions stable. Disabled trackers have no edit preview or hit area.

**Focused check:** `xiui_inventory_smoke`.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
