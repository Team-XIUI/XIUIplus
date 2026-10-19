# P09: Satchel item actions

> Historical record. Use the [current documentation](../../README.md) for implementation guidance.

Status: **Implemented first pass**. Focused action checks and the native runtime build passed. Live item mutations remain for Cody to verify.

`satchel_actions` serializes exact-slot move/split/use/equip/drop/bazaar/sort requests, revalidates SDK identity/access/locks/capacity and observes inventory/equipment changes with cancellation and timeout outcomes. Menus, drag-to-bag, double-click and controller workflows share that service. Quantity/price/equipment dialogs retain Cancel-first drop confirmation and nonrepeating activation. Optional autosort follows one observed owned transfer/split. Cached/slip sources and Read only mode cannot dispatch. Use currently targets self; items needing another target use the native menu. See [Satchel](../../reference/satchel.md).

Audit date: October 15, 2026. Priority: High after P08.

[Parity audit](../feature-parity.md) · [Plan index and shared contract](README.md)

## Original audited gap

Native has no inventory transfer, use/equip context menu, split/drop/bazaar dialog or bag sorting workflow.

## Source evidence

- Lua reference: `XIUI/modules/satchel/{contextmenu,itemlogic,packets,containerlogic}.lua`.
- Lua reference: `XIUI/modules/satchel/init.lua: drag transfers, double-click transfers and sorting`.
- Native integration: [src/runtime/modules/inventory/inventory_source.hpp](../../../src/runtime/modules/inventory/inventory_source.hpp).
- Native integration: [src/runtime/modules/equipment/equipment_source.hpp](../../../src/runtime/modules/equipment/equipment_source.hpp).
- Native integration: [src/runtime/config/config_controls.hpp](../../../src/runtime/config/config_controls.hpp).
- Native integration: [src/runtime/hud_runtime_controller_config.cpp](../../../src/runtime/hud_runtime_controller_config.cpp).

Lua paths are relative to the reference repository identified in the parity audit; brace lists name multiple files.

## Scope

- Use/equip, legal bag transfers with quantity selection, drag/drop and double-click transfer, stack splitting and bag sort/optional autosort.
- Bazaar price creation/modification/removal and explicit item-drop confirmation with live eligibility checks.
- Read-only/restricted rules for cached characters, storage-slip views and any selected server compatibility mode.

## Dependencies

P08 live contents/identity and context-menu surface.

## Original implementation plan

1. Define one item_action_service over immutable requested item identity, source/destination and quantity. Revalidate item generation, accessibility, lock/equipment/bazaar state and capacity before dispatch.
2. Inspect current SDK operations and the Lua packet logic. Use a bounded serialized request queue with server/client acknowledgement, timeout and cancellation; do not treat submission as completed movement.
3. Implement context menus and quantity/bazaar/drop dialogs through shared controls. Pointer drag and double-click call the same service as controller commands; visual reorder remains separate.
4. Add opt-in sorting/autosort and per-container restrictions. Coalesce observation updates and never start a sort loop from the updates produced by the previous sort.
5. Handle zoning, logout, bag lock/loading and identity replacement by cancelling stale pending work and preserving the visible error/result.

## Input and controller behavior

Every drag operation has an equivalent source/destination/quantity controller workflow. Destructive Drop starts on Cancel and requires fresh explicit confirmation. Focus movement and dialog opening cannot use or discard an item.

## Completion criteria

- [ ] A stale or read-only record cannot send an item action; quantity/capacity constraints are checked again on activation.
- [ ] Moves and bazaar updates appear complete only after authoritative state changes.
- [ ] Native menus and unrelated input retain ownership outside an active XIUI action/dialog.

## Focused validation

`xiui_satchel_actions_smoke` and `xiui_satchel_ui_smoke` passed: stale item/extra/generation rejection, capacity/stack rules, exact equipment-slot observation, read-only targets, timeout/cancellation, protected populated slips, Cancel-first Drop and fresh nonrepeating activation. No live item mutation was sent. The unchecked criteria above retain Cody's native-input/gameplay acceptance work.

Apply the [shared implementation contract](README.md#shared-implementation-contract), then update this plan and the audit when the feature is integrated.

