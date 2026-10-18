# P07: Treasure pool and lot/pass

Status: **Implemented** in the Native working tree, October 17, 2026. Live loot and physical-controller acceptance remain with Cody.

[Parity audit](../feature-parity.md) · [Plan index and shared contract](README.md)

## Current behavior

**Extras → Treasure Pool** configures automatic appearance, expanded rows, size, native container and four independent text profiles. `/xiui treasure` force-shows the pool; `/xiui treasure settings` opens configuration. Item names/icons/counts, local decisions, the winning lot/player and recent observed lots/passes share one management window. Footer controls provide Lot All, Pass All, Expand/Collapse and Details. A forced empty view offers Close.

Treasure alerts and item-received rewards continue through the existing **Notifications** module and its event switches. The pool adds no notification system or duplicate reward producer. Inventory restrictions are excluded; the server remains authoritative about accepting a lot or awarding an item.

## Source and actions

- [treasure_source](../../src/runtime/modules/treasure/treasure_source.cpp) reconciles ten native SDK slots and validated `0xD2`/`0xD3` packets. Identity includes the item, drop timestamp and a monotonic generation. Resolved-entry tombstones prevent an older SDK sample from restoring a removed item.
- Individual and batch Lot/Pass use the same validated service and outgoing `0x41`/`0x42` packets. Batch requests capture eligible identities, recheck each slot and reject replacements. Pending responses prevent immediate duplicate activation; native decisions replace local pending state.
- Zoning suspends actions while retaining entries until native reconciliation. Logout/character replacement clears transient state. Hidden UI, native menus/text/subtargets and foreground loss release browsing.
- New-drop countdowns estimate the server's five-minute lifetime. Old entries use clock reconciliation only when available; otherwise timing stays unknown. Duplicate packets preserve identity and the original estimate. Native resolution removes entries immediately.
- [treasure_module](../../src/runtime/modules/treasure/treasure_module.cpp) reuses shared native containers, item icons, typography, tinting, focus frames, settings and UI sounds. Edit samples cannot send requests.

## Input and controller behavior

Enter through **Treasure Pool** in `/xiui focus`, or bind `/xiui focus treasure` to an existing macro. It appears only while a ready, visible pool contains items. Opening the pool never takes focus automatically or makes a lot decision.

- **D-pad Up/Down:** change rows, retaining the same action where available; the footer is reachable from the last item.
- **Left/Right:** move among item details, Lot and Pass within a row, or among footer controls. Unavailable actions are skipped.
- **Confirm** (normally A/Cross): activate once. Holding Confirm cannot repeat it.
- **Actions:** toggle recent lot/pass details.
- **Cancel** (normally B/Circle): leave pool focus and return controls to gameplay.

Buttons follow effective game bindings. Keyboard arrows/Enter/Escape share these actions; mouse clicks use the same identity checks and retain ownership through release. Losing focus cancels pending pointer activation. Lot All/Pass All act on the captured pool, excluding items that arrive after a pointer press.

## Validation and remaining acceptance

The affected x86 build and focused `--phoenix-only` check pass. Coverage includes slot reuse, exactly-once requests, loading, native pass reconciliation, resolution tombstones, old/duplicate drops, stale batch identities, spatial controller navigation, disabled controls, held Confirm, cancellation and settings/text/position persistence. Notifications retain their existing path.

- [ ] Cody checks real party/alliance loot, native-menu decisions and server responses.
- [ ] Cody checks window appearance, pool timing across zones and physical-controller behavior.
