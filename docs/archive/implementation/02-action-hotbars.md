# P02: Action hotbars

> Historical record. Use the [current documentation](../../README.md) for implementation guidance.

Status: **Implemented** in the Native working tree; live gameplay acceptance remains with Cody.

October 16 implementation: ten independent twelve-slot hotbars, shared action catalog/editor, pointer/key/controller activation, assignment locks and drag swaps, keyboard binding capture, native action artwork, resource/recast/charge presentation and shared HUD text profiles. Configuration has Hotbars/Crossbar tabs and a normal-bar selector. See [Action Bars usage](../../reference/actionbars.md).

Code: `src/runtime/modules/actionbars/{actionbars_module,action_executor,action_store}.cpp`, `hud_runtime_actionbars.cpp` and the shared icon cache. Native macro references use a validated client bridge. Software checks cover storage/import/dispatch boundaries and UI ownership without executing live actions; native hardware and visual/gameplay acceptance are separate.

Audit date: October 15, 2026. Priority: High.

[Parity audit](../feature-parity.md) · [Plan index and shared contract](README.md)

## Current gap

The original gap was configurable action slots and execution beyond Cast Cost/Recast. The implementation below now supplies that scope; skillchain prediction remains P05.

## Source evidence

- Lua reference: `XIUI/modules/hotbar/{init,data,display,actions,actiondb,slotrenderer,recast}.lua`.
- Lua reference: `XIUI/config/hotbar.lua`.
- Native integration: [src/runtime/modules/castcost/cast_cost_source.hpp](../../../src/runtime/modules/castcost/cast_cost_source.hpp).
- Native integration: [src/runtime/modules/recast/recast_source.hpp](../../../src/runtime/modules/recast/recast_source.hpp).
- Native integration: [src/runtime/hud_runtime.cpp](../../../src/runtime/hud_runtime.cpp).
- Native integration: [src/host/plugin.cpp](../../../src/host/plugin.cpp).

Lua paths are relative to the reference repository identified in the parity audit; brace lists name multiple files.

## Scope

- Up to ten independently configured bars, grid layouts, slot labels/icons, empty-slot hiding, lock state, drag assignment and slot swapping.
- Spell, ability, weapon skill, item, equip, pet and macro actions with target placeholders, resource/cooldown/charge availability and tooltips.
- Keyboard bindings and pointer activation through one action service; opt-in replacement of conflicting native macro controls.

## Dependencies

Use P04 for named/job/pet palettes and imports; P03 adds crossbar input; P05 adds skillchain highlighting.

## Implementation

1. Define typed slot records with action kind, stable resource ID or command/macro reference, target placeholder and optional display overrides. Keep binding records separate from presentation.
2. Implement one action resolver/executor that checks live job/action availability, target context, MP/TP, recasts and native input ownership. Use existing Ashita command dispatch and native target selection instead of generating UI clicks.
3. Build a cached slot renderer and action picker on shared controls, game icons, text profiles and native containers. Reuse the current recast/cost sources, extending their resource lookup API where necessary.
4. Register bar settings, persistence, edit bounds, assignment/swap gestures and configurable key routes. Resolve key conflicts before suppression; do not consume unrelated gameplay or native text input.

## Input and controller behavior

Focused bars support enter, slot navigation, tooltip/help, activate, assignment actions and cancel. A keyboard press, click and controller activation call the same executor once. Held activation never repeats a spell or macro.

## Completion criteria

- [x] Pointer, keybinding and focused navigation use the shared resolver/executor; unknown or unavailable actions explain why they cannot run.
- [x] Cached catalog/item snapshots and native recast/charge data supply resource and cooldown presentation.
- [x] Ten-bar settings and assignments persist; locking controls assignment gestures while retaining activation.
- [ ] Cody's live visual and action-execution acceptance.

## Focused validation

Focused resolver/executor checks with mocked dispatch, one key ownership case and persistence. No live actions need to be sent for the documentation or initial unit checks.

Apply the [shared implementation contract](README.md#shared-implementation-contract), then update this plan and the audit when the feature is integrated.

