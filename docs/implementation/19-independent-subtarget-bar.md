# P19: Independent Subtarget Bar

Status: **Implemented** in the Native working tree. Gameplay validation: **Pending Cody's check**.

Audit date: October 15, 2026. Priority: Medium.

[Parity audit](../feature-parity.md) · [Plan index and shared contract](README.md)

## Implemented behavior

Target settings now include a **Subtarget** view with its own enable switch, name/prefix, HP bar/percent/label, distance, casts, status effects, width, meter/bookends, colors and text profiles. **Attached** remains the default; **Independent** exposes a separate movable display through the shared HUD movement controls.

## Source evidence

- Lua reference: `XIUI/config/targetbar.lua: Subtarget Bar tab`.
- Lua reference: `XIUI/modules/targetbar.lua: subtarget view and movement`.
- Native integration: [src/runtime/modules/target/target_module.cpp](../../src/runtime/modules/target/target_module.cpp): `views_[1]`, `UpdateDetails` and shared `DrawView`.
- Native integration: [src/runtime/modules/target/target_module.hpp](../../src/runtime/modules/target/target_module.hpp): ConfigViews.
- Independent movement adapter: [src/runtime/modules/target/target_child_module.cpp](../../src/runtime/modules/target/target_child_module.cpp).
- Presentation settings: [src/runtime/modules/target/target_of_target_settings.cpp](../../src/runtime/modules/target/target_of_target_settings.cpp), reused with separate subtarget settings keys.
- Focused fixtures: [tests/target_child_checks.cpp](../../tests/target_child_checks.cpp).

Lua paths are relative to the reference repository identified in the parity audit; brace lists name multiple files.

## Presentation and persistence

The same native subtarget read model supplies attached and independent rendering. It stays separate from the main target and the main target's action recipient. Cancelled or unavailable native subtargets leave no normal display; edit mode supplies a display sample. Disabling this HUD view never disables native subtargeting.

Attached mode stays below the linked Target contents and retains the old six-pixel gap. Missing subtarget settings inherit the currently loaded main meter, fills and text once, preserving the existing appearance; later edits persist independently. Optional Follow target controls allow deliberate sharing. The subtarget has its own status layout and timer/details profile using the shared status renderer and native artwork.

Independent mode uses its own width and position, includes its own cast/status footprint and is excluded from the main Target movement bounds. Its display remains available when the main Target module is disabled. Attached mode depends on the parent being shown. Contextual visibility applies to either placement without leaving an invisible independent hit area.

Presentation persists in `[target.subtarget]`, meter/text settings in `meter.target.subtarget` / `text.target.subtarget.*`, status layout in `[status.subtarget]`, and independent bounds in `[layout.subtarget]`. Only the child movement adapter writes those bounds, so the normal controller movement commit/cancel path also protects saves during a preview. Switching placement retains the independent position and linked offsets.

An optional **Mob level** switch uses P06's local database by exact native server ID. It appends a known static level/range and omits unavailable records; it does not guess a level from names or issue a `/check` probe. The default remains off.

## Input and controller behavior

FFXI retains spell/ability subtarget navigation and confirmation. Pointer dragging, centering, grid snapping and controller Move/Cancel use the shared movement adapter. Moving or configuring the display never confirms a spell, changes the hard target or creates a second target-selection cursor. Available status descriptions use the existing Target status-inspection path.

## Completion criteria

- [x] Main Target, recipient and subtarget presentation can change independently.
- [x] Independent placement persists; attached mode keeps the existing layout and requires no mob database.
- [x] Native subtarget cancellation/unavailability clears its read model without introducing a game action.
- [x] Independent children share existing movement and visibility controls.
- [ ] Cody's in-game visual and native subtarget check.

## Focused validation

`RunTargetChildChecks` in [tests/target_child_checks.cpp](../../tests/target_child_checks.cpp) adds separate main/subtarget/recipient read-model identities, attached/independent rendering, one draw per child, empty/cancelled views, parent-disabled children, visibility and settings/bounds persistence. The fixture seeds the read model and checks unavailable-source clearing; it does not simulate or establish live native spell targeting. Shared runtime movement checks cover commit/cancel integration. Gameplay validation remains with Cody.


Coordinated validation: affected runtime and focused executable built; `xiui_config_input_smoke --resource-target-features-only` and the settings presentation audit passed. Installed runtime hot reload was observed through AgentLink on October 15 at 20:44:13; saved current layouts/styles were retained. No native gameplay or physical-controller acceptance was performed.
