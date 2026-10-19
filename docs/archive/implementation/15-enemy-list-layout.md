# P15: Enemy List columns and target layout

> Historical record. Use the [current documentation](../../README.md) for implementation guidance.

Status: **Implemented** in the Native working tree. Gameplay validation: **Pending Cody's check**.

Audit date: October 15, 2026. Priority: Medium.

[Parity audit](../feature-parity.md) · [Plan index and shared contract](README.md)

## Implemented behavior

Enemy List now supports columns and separate enemy-target panels while retaining the existing enemy tracking, HP, distance, casts and optional attack timing.

- **Rows per column** (1–20), **Maximum columns** (1–5), row spacing and column spacing control the grid. The source remains capped at 32 enemies. Entries retain tracking order and each row reserves its measured cast, status and timing height.
- **Separate target panel** moves the enemy's last action recipient into its own native window. Width, X/Y offsets, padding, opacity and border visibility are independent of the main enemy card. Inline target names remain the default.
- **Entry borders**, **Claim-colored borders** and **Click to target** are configurable. Claim tint follows the shared game-state enemy name color. Native window artwork, text effects and meter styles remain shared.
- The **Statuses** topic uses P14's shared icon size, spacing, row limit, alignment, offsets, visible limit/overflow, opacity, timer, crowd-control and tooltip settings. Enemy durations remain unknown when no native duration is available; layout does not invent countdowns.

## Source evidence

- Lua reference: `XIUI/config/enemylist.lua`.
- Lua reference: `XIUI/modules/enemylist.lua`.
- Native integration: [src/runtime/modules/enemylist/enemy_list_module.hpp](../../../src/runtime/modules/enemylist/enemy_list_module.hpp).
- Native integration: [src/runtime/modules/enemylist/enemy_list_module.cpp](../../../src/runtime/modules/enemylist/enemy_list_module.cpp).
- Native integration: [src/runtime/hud_runtime.cpp](../../../src/runtime/hud_runtime.cpp): HandleTargetMouse.
- Native controller integration: [src/runtime/hud_runtime_status.cpp](../../../src/runtime/hud_runtime_status.cpp) and [src/runtime/hud_runtime_controller.cpp](../../../src/runtime/hud_runtime_controller.cpp).
- Shared status layout: [src/runtime/ui/status_strip.hpp](../../../src/runtime/ui/status_strip.hpp).
- Focused fixtures: [tests/enemy_layout_checks.cpp](../../../tests/enemy_layout_checks.cpp).

Lua paths are relative to the reference repository identified in the parity audit; brace lists name multiple files.

## Layout and persistence

One measured layout supplies drawing, card/panel hit tests, controller frames and edit bounds. Column widths include side panels and shifted status strips; row footprints include positive and negative offsets. Row and column gaps are never targetable. A duplicate advance of the HP detail line was removed so cast/status placement matches the reserved card height.

The grid moves inside the viewport, starts another column when a column reaches the available height, and omits complete columns that cannot fit. Oversized custom side-panel offsets are constrained to the available viewport. A single card made taller than the viewport by extreme status sizing can still exceed its height; normal layout limits do not discard individual active effects to force a fit.

Existing profiles retain one column, inline targets, enabled pointer targeting, card width, position, meters and text profiles. `rows_per_column` falls back to the previous `max_rows`; row spacing retains the existing `spacing` key. New column/panel/border/input settings persist in `[enemylist]`, status presentation in `[status.enemylist]`, and the existing layout/text sections remain in use. The Width control continues to size each enemy card.

Native targeting uses the existing validated game operation. It requires no Lua addon or `/shorthand` integration.

## Input and controller behavior

Clicking a card or its target panel selects that enemy when **Click to target** is enabled. Controller Confirm uses the same validated targeting operation and remains available when pointer targeting is disabled.

Up/Down browses enemies in tracking order; Left/Right moves to the nearest row in an adjacent visible column. Focus follows server ID plus entity index through reordering and ends if that identity leaves the visible grid.

From a focused enemy, **Actions** opens status inspection on its first visible effect. Directions or Previous/Next tab cycle effects; Cancel returns to enemy browsing. Status focus tracks effect ID plus duplicate occurrence for the current enemy. Removing the inspected effect ends its details rather than silently showing another effect. Pointer hover and controller inspection share the game's effect names, artwork and descriptions; neither performs a game action.

## Completion criteria

- [x] Configured row/column limits determine visible capacity; cast/status heights contribute to measured rows.
- [x] Drawing, pointer, controller and edit geometry use the same measured entry layout.
- [x] Existing settings retain one column and their saved presentation.
- [x] Target panels, native borders, claim tint, click preferences and shared status controls persist.
- [x] Enemy and status inspection focus use stable identities.
- [ ] Cody's in-game visual and controller check.

## Focused validation

The x86 runtime build and `xiui_module_parity_smoke --hud-parity-only` passed on October 15, 2026.

`RunEnemyLayoutChecks` in [tests/enemy_layout_checks.cpp](../../../tests/enemy_layout_checks.cpp) covers unequal cast/status row heights, grid capacity, gap hit exclusion, adjacent-column navigation, identity reorder, target-panel footprints, disabled pointer targeting, old single-column settings, settings round trips, viewport capacity, large target offsets, and stable effect inspection/removal. These fixtures were added for the coordinated focused check; this document does not claim gameplay or physical-controller validation.

