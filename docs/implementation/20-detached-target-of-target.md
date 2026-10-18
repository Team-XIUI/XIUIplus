# P20: Detached Target of Target window

Status: **Implemented** in the Native working tree. Gameplay validation: **Pending Cody's check**.

Audit date: October 15, 2026. Priority: Small extension of existing recipient settings.

[Parity audit](../feature-parity.md) · [Plan index and shared contract](README.md)

## Implemented behavior

Target of Target's **Position** control now adds **Detached** alongside Inline right, Inline left, Below and Above. Detached mode has independent position, width, edit bounds, dragging, centering, snapping and controller movement. Existing linked placements and all recipient display/style preferences remain available.

## Source evidence

- Lua reference: `XIUI/config/targetbar.lua: Split Target Bars`.
- Lua reference: `XIUI/modules/targetbar.lua: independent target-of-target position`.
- Native integration: [src/runtime/modules/target/target_of_target_settings.hpp](../../src/runtime/modules/target/target_of_target_settings.hpp).
- Native integration: [src/runtime/modules/target/target_of_target_settings.cpp](../../src/runtime/modules/target/target_of_target_settings.cpp).
- Native integration: [src/runtime/modules/target/target_module.cpp](../../src/runtime/modules/target/target_module.cpp).
- Independent movement adapter: [src/runtime/modules/target/target_child_module.cpp](../../src/runtime/modules/target/target_child_module.cpp).
- Native integration: [src/runtime/hud_runtime_layout.cpp](../../src/runtime/hud_runtime_layout.cpp).
- Focused fixtures: [tests/target_child_checks.cpp](../../tests/target_child_checks.cpp).

Lua paths are relative to the reference repository identified in the parity audit; brace lists name multiple files.

## Presentation and persistence

The detached adapter shares the existing recipient read model, validated entity identity, health/cast/status content, HP animation and meter/fill/text follow preferences. Target renders linked content; the adapter renders detached content once. Detached content does not enlarge the main Target edit rectangle, and it remains available when the main Target module is disabled.

Detached mode hides the connecting arrow. It retains the arrow preference, linked spacing, offsets, alignment and linked width, restoring their effect when a linked placement is selected again. Detached Width adjusts the child window's own bounds; moving Target does not move that window.

Existing recipient settings stay in `[target.target_of_target]` and retain their current default linked placement when new keys are absent. Independent bounds use `[layout.targetrecipient]`. The child movement adapter alone saves these bounds, allowing the existing movement preview to save its committed position and restore on Cancel. Switching back to linked mode preserves the detached position for later reuse.

Outside editing, no resolved recipient means no detached display. Edit mode can show the recipient sample only when enabled and contextually visible. Attached adapters have no separate movement outline or pointer area; disabled or contextually hidden detached views have no preview/hit surface.

No new source, action-recipient estimate or external addon is required. P19 shares the child adapter and rendering path.

## Input and controller behavior

The detached display remains passive. Edit-mode mouse and controller movement share the existing commit/cancel path; native targeting is unaffected.

## Completion criteria

- [x] Detached recipient and Target use independent movement state and bounds.
- [x] Reattaching restores linked placement and retained arrow/style preferences.
- [x] Disabled, hidden and missing-recipient states do not produce a normal detached display.
- [x] Detached movement uses shared pointer/controller commit/cancel behavior.
- [ ] Cody's in-game visual and movement check.

## Focused validation

`RunTargetChildChecks` in [tests/target_child_checks.cpp](../../tests/target_child_checks.cpp) adds linked/detached switching, separate identities/positions, exactly-once child drawing, restored arrow behavior, hidden-preview behavior, parent-disabled rendering and persistence. Runtime movement fixtures cover save/Cancel integration through the actual registry. These are focused software fixtures; gameplay and physical-controller behavior remain for Cody to check.


Coordinated validation: affected runtime and focused executable built; `xiui_config_input_smoke --resource-target-features-only` and the settings presentation audit passed. Installed runtime hot reload was observed through AgentLink on October 15 at 20:44:13; saved current layouts/styles were retained. No native gameplay or physical-controller acceptance was performed.
