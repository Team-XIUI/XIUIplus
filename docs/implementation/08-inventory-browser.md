# P08: Satchel inventory browser

Status: **Implemented first pass**. Native runtime build passed; focused interaction checks and installation are recorded in [Satchel](../satchel.md). Visual and gameplay acceptance remains with Cody.

The registered `satchel_module` supplies all/bag tabs, paged native icons, quantities, gil, search, state borders, item-resource details, charges and supported system-1 augments. Custom display order uses item/extra fingerprints with deterministic duplicate reconciliation and remains separate from server sorting. It has independent settings/text/visibility/position, `/xiui satchel` commands, opt-in native aliases and shared keyboard/controller operations. Unsupported extended formats remain absent. See [usage](../satchel.md).

Audit date: October 15, 2026. Priority: High.

[Parity audit](../feature-parity.md) · [Plan index and shared contract](README.md)

## Original audited gap

Native inventory modules show capacity dots/counts and Equipment shows equipped items. Neither is a searchable contents browser.

## Source evidence

- Lua reference: `XIUI/modules/satchel/{init,ui,itemlogic,containerlogic,icons,tooltips,searchlogic,layoutstate}.lua`.
- Lua reference: `XIUI/config/satchel.lua`.
- Native integration: [src/runtime/modules/inventory/inventory_source.hpp](../../src/runtime/modules/inventory/inventory_source.hpp).
- Native integration: [src/runtime/modules/equipment/equipment_source.hpp](../../src/runtime/modules/equipment/equipment_source.hpp).
- Native integration: [src/runtime/modules/equipment/equipment_module.cpp](../../src/runtime/modules/equipment/equipment_module.cpp).
- Native integration: [src/runtime/ui/game_icon_cache.hpp](../../src/runtime/ui/game_icon_cache.hpp).

Lua paths are relative to the reference repository identified in the parity audit; brace lists name multiple files.

## Scope

- Bag tabs, item grids, stack quantities, gil, temporary/available containers, empty-slot settings and item-state borders.
- Search and rich item tooltips including supported extended item data/augments, equipment flags, jobs/levels and descriptions.
- Persistent window settings and visual item ordering, plus commands to open/close; /satchel interception only as an explicit preference.

## Dependencies

P09 adds game-changing item actions; P10 adds offline characters and storage slips.

## Original implementation plan

1. Extend inventory reading with bounded item records keyed by container, slot, item ID and change generation. Reuse settled-loading rules but keep the capacity tracker API stable.
2. Extract reusable item-view/tooltip helpers from equipment presentation where useful. Decode supported extended item data separately from rendering; missing details remain absent.
3. Build an inventory_browser_module with tabs, search, cached icons, tooltips and paged/scrollable grids using native shared UI primitives.
4. Distinguish visual order from server inventory order. Persist visual placement by stable item identity where possible and reconcile stack merges/splits rather than moving items to match the screen.
5. Add explicit open/close commands, optional native /satchel override, layout persistence and edit samples. Keep this milestone read-only until P09 is implemented.

## Input and controller behavior

Controller routes cover bag switching, grid movement, search entry, tooltip/details and Back. Search accepts keyboard typing initially; an empty search or bag change never activates an item.

## Completion criteria

- [ ] Contents, stack counts, gil and search agree with settled client data, including unavailable bags and temporary items.
- [ ] Tooltips and focus work on both pointer and controller paths; hidden empty slots do not trap navigation.
- [ ] Existing capacity dots and Equipment retain their independent visibility, settings and positions.

## Focused validation

`xiui_satchel_data_smoke`, `xiui_satchel_actions_smoke` and `xiui_satchel_ui_smoke` passed. They cover native loading/records, stale selections, cache/slip boundaries, shared confirmation, sparse navigation, visual identity reconciliation and text/session/settings round trips. No automated live item mutation or visual/gameplay check was performed. The unchecked criteria above retain Cody's live acceptance work.

Apply the [shared implementation contract](README.md#shared-implementation-contract), then update this plan and the audit when the feature is integrated.

