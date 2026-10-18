# P18: Target display options and entity palettes

Status: **Implemented** in the Native working tree. Gameplay validation: **Pending Cody's check**.

Audit date: October 15, 2026. Priority: Medium.

[Parity audit](../feature-parity.md) · [Plan index and shared contract](README.md)

## Current gap

Main Target now offers independent Name, Cast bar and Lock highlight switches, separate player/monster HP-percent inclusion, and an optional decimal or hexadecimal enemy ID suffix. The suffix matches Lua XIUI: the last three characters of the native monster server ID. It is not an entity index, database identifier or mob level; unavailable identities are omitted.

## Source evidence

- Lua reference: `XIUI/config/targetbar.lua: Target settings and color tabs`.
- Lua reference: `XIUI/config/global.lua: Entity Name Colors`.
- Lua reference: `XIUI/modules/targetbar.lua`.
- Native integration: [src/runtime/modules/target/target_module.hpp](../../src/runtime/modules/target/target_module.hpp).
- Native integration: [src/runtime/modules/target/target_module.cpp](../../src/runtime/modules/target/target_module.cpp).
- Native integration: [src/runtime/entities/entity_name_colors.hpp](../../src/runtime/entities/entity_name_colors.hpp).
- Native integration: [src/runtime/modules/target/target_of_target_settings.hpp](../../src/runtime/modules/target/target_of_target_settings.hpp).
- Display controls/persistence: [src/runtime/modules/target/target_display_settings.cpp](../../src/runtime/modules/target/target_display_settings.cpp).
- Focused fixture: [tests/target_display_checks.cpp](../../tests/target_display_checks.cpp).

Lua paths are relative to the reference repository identified in the parity audit; brace lists name multiple files.

## Scope

- Show/hide target name, enemy cast and lock-on treatment; enemy ID in decimal/hex; HP-percent entity inclusion preferences.
- Target/recipient HP gradients by entity type and configurable shared name/claim colors.
- Dedicated cast layout/text/fill preferences while retaining shared meters and current game-state classification.

## Dependencies

P06 for optional database level information; P14 owns status controls; P19/P20 own separate subtarget/recipient surfaces.

## Implemented presentation

**Cast Bar** provides its own height, spacing, optional meter effects/style and optional fill gradient. Cast text has an independent profile in Text → Selected text → Cast. Defaults retain the current HP-meter styling and theme accent, and missing cast-text settings inherit the saved value style. Hiding casts removes their measured segment. Hiding both name and distance collapses their header space.

**Entity Colors** uses shared classification for group/other players, NPCs, unclaimed/group-claimed/other-claimed monsters and other entities. Custom entity names are opt-in; disabled defaults preserve native explicit name colors and the previous game-state palette. Enabled colors flow through the existing shared entity-name helper, including Target and Enemy List. Names still use `TintTextProfile`, retaining gradients and effects. Optional entity HP gradients affect Target and child views that follow Target colors; independent child gradients remain configurable through P19/P20.

Target owns portable palette persistence in `[entity.colors]` and `[target.entity_fills]`. Existing Target flags remain in `[target]`; dedicated cast preferences use `[target.cast]`, `[meter.target.cast]` and `[text.target.cast]`. Main Target and both child views also offer optional **Mob level**, using P06's exact native server-ID lookup to append a known static level/range. The default remains off; unavailable records are omitted without a name guess or `/check` probe.

## Input and controller behavior

Configuration follows the implemented controller path. Visual target changes do not acquire target ownership or execute actions.

## Completion criteria

- [x] Name/cast/lock switches and player/monster HP-percent inclusion are explicit preferences.
- [x] Shared classification supplies configurable name/claim colors and optional HP gradients.
- [x] IDs use the documented native server-ID suffix; cast text retains full shared effects.
- [ ] Cody's in-game visual check.

## Focused validation

`RunTargetDisplayChecks` in [tests/target_display_checks.cpp](../../tests/target_display_checks.cpp) passed through `xiui_config_input_smoke --new-features-only`. It covers classification, opt-in shared colors, decimal/hex suffixes, cast/header measurement, persistence and exclusion of the reserved cast-height slot from all config sections. The complete settings-presentation audit passed; no gameplay validation is claimed.

