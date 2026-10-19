# P14: Status display controls and tooltips

> Historical record. Use the [current documentation](../../README.md) for implementation guidance.

Status: **Implemented** in the Native working tree.

Updated October 15, 2026. Priority: Medium.

[Parity audit](../feature-parity.md) · [Status Effects usage](../../reference/status-effects.md) · [Plan index and shared contract](README.md)

## Delivered behavior

Target and Enemy List expose status visibility, icon size, spacing, alignment, offsets, icon limits, wrapping, overflow counts, opacity, countdown visibility, crowd-control filtering and unknown-duration markers. One shared layout supplies drawing and inspection bounds. These are presentation preferences; hiding a countdown does not discard its source data.

Party retains its existing left/right/inside placement and containers. Its **Status order** offers Native order, Show debuffs first, Show buffs first and Split buffs and debuffs. Native order is the default for existing layouts. Split mode provides independently configurable buff and debuff rows, including icon size, columns, spacing, limits, offsets, opacity, timer visibility and text styles. Native source order and paired local deadlines survive grouping.

The **Status Effects** settings page adds separate local-player Buffs and Debuffs modules. Each has its own enabled state, movable bounds, icon geometry, timers, optional native container and heading/timer/tooltip text profiles. Empty groups hide during normal play.

Hover and deliberate controller inspection show names and descriptions from the installed client's resource manager. Tooltips render in the final overlay pass and yield to active configuration/input surfaces. Merely inspecting an effect does not target, cast or cancel it.

Tooltips fit their content up to 300 pixels wide. They show a capitalized bold/brighter effect name, native description, and a smaller “Estimate: Effects may end early” note when applicable. Each text role wraps/measures at its own size with normal text effects. Effect IDs, duplicate countdown/unavailable-duration lines and combat-observation details are omitted.

Party includes an explicit edit-only status preview with mixed buffs, debuffs and sample countdowns. Full-party member preview remains independent; status samples also work with only the visible members or one empty-party example. Render and measurement use matching display copies. Preview countdowns follow the normal timer switches without altering saved flags or native member data. The old implicit buffs on the second full-party sample were removed. Party split-row icons default to 12 pixels with containers and timers off; hiding Buff row shows only debuffs without empty space.

## Source evidence

- Lua reference: `XIUI/config/targetbar.lua: Buffs/Debuffs and Bard Song Settings`.
- Lua reference: `XIUI/config/enemylist.lua: Debuffs`.
- Lua reference: `XIUI/handlers/statushandler.lua: render_tooltip and FlushTooltip`.
- Native integration: [src/runtime/ui/status_strip.hpp](../../../src/runtime/ui/status_strip.hpp).
- Native integration: [src/runtime/modules/target/target_module.cpp](../../../src/runtime/modules/target/target_module.cpp).
- Native integration: [src/runtime/modules/enemylist/enemy_list_module.cpp](../../../src/runtime/modules/enemylist/enemy_list_module.cpp).
- Native integration: [src/runtime/modules/party/party_module.cpp](../../../src/runtime/modules/party/party_module.cpp).
- Native integration: [src/runtime/modules/status/status_effects_module.cpp](../../../src/runtime/modules/status/status_effects_module.cpp).
- Shared grouping metadata: [src/runtime/ui/status_classification.hpp](../../../src/runtime/ui/status_classification.hpp).
- Final tooltip/input routing: [src/runtime/hud_runtime_status.cpp](../../../src/runtime/hud_runtime_status.cpp).

Lua paths are relative to the reference repository identified in the parity audit; brace lists name multiple files.

## Native data boundary

[P13](13-debuff-tracking-rules.md) uses actual local status timestamps and the original XIUI database for confirmed combat-effect duration estimates. Remote party snapshots provide effect IDs without durations. Countdown labels show plain numbers; the smaller tooltip note identifies an estimate and possible early removal. Unknown-duration markers identify effects absent from the catalog. Native hidden timers do not receive these markers. Alternate status artwork is covered by P23.

## Input and controller behavior

From a focused Party row, Next tab enters effect inspection; Actions retains Tell composition. Enemy List uses Actions to inspect effects. Directions or previous/next tab moves between effects, and Cancel returns to the row. `/xiui focus target`, `/xiui focus buffs` and `/xiui focus debuffs` enter their available effects directly. These surfaces also appear in the explicit focus picker when usable.

## Completion criteria

- [x] Turning off timers or markers changes presentation while preserving tracked state.
- [x] Icon scaling/wrapping/offsets update measured bounds and hit tests together.
- [x] Descriptions render after the HUD and have explicit controller inspection paths.
- [x] Party supports stable grouping and separate configurable buff/debuff rows.
- [x] Local Buffs and Debuffs have independent modules, settings and positions.

## Focused validation

Follow-up work adds independent row spacing, timer text offsets/Above/Over placement, mode-specific Party controls without duplicate labels, a distinct effects icon, edit-only sample previews and Debuffs following Buffs presentation while retaining independent preferences. The focused HUD parity check passed with geometry, mode visibility, persistence, preview isolation and follow/unfollow coverage.

The x86 runtime build and `xiui_module_parity_smoke --hud-parity-only` passed on October 15, 2026, including duplicate-effect inspection and runtime configuration/visibility checks.

`tests/status_display_checks.cpp` checks native timestamp conversion/sentinels, unknown versus hidden timers, confirmed versus inferred effects, scaled wrapping/hits, persistence, classification, stable grouping and separate Party row geometry. `tests/status_module_checks.cpp` covers the standalone groups. Visual and native-controller gameplay checks remain with Cody.

The implementation reuses the [shared contract](README.md#shared-implementation-contract); no live game session or hardware validation is implied by these deterministic checks.

