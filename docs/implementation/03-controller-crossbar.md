# P03: Controller crossbar

Status: **Implemented** in the Native working tree; physical-device/gameplay acceptance remains with Cody.

October 16 implementation: six Lua-style trigger banks, ordered combinations, optional double taps, eight action slots per bank, shoulder/D-pad palette cycling, trigger hysteresis, controller setup and shared action editing. Replacement is explicit; without it, physical trigger activation stays native. See [Action Bars usage](../actionbars.md).

Code: `src/host/{crossbar_normalizer,controller_mailbox}.hpp`, `controller_input.cpp`, `src/shared/controller_input.hpp` and `src/runtime/modules/actionbars/actionbars_module.cpp`. ABI 17 adds cached crossbar configuration. Both per-event and writable state/data paths suppress claimed controls before FFXI processing. Sticks/camera remain available. Profile revisions, device replacement and suspension require neutral input before rearming. DirectInput currently supports digital trigger buttons and POV; axis-only trigger layouts require a remapper or XInput mode.

Focused `xiui_crossbar_input_smoke` and existing `xiui_controller_input_smoke` checks pass. They cover mappings, all banks, state/event deduplication, repeated/held input, ownership changes, device replacement and release draining. They do not establish physical controller feel.

The matching ABI 17 development host/runtime was installed and AgentLink confirmed it loaded on October 16 at 22:24:17. The explicit replacement preferences remain off until enabled in setup. Native macro reference discovery was confirmed available after the compatible runtime update at 22:25:05; execution was not invoked.

Audit date: October 15, 2026. Priority: High after P02.

[Parity audit](../feature-parity.md) · [Plan index and shared contract](README.md)

## Current gap

The original gap was controller action activation beyond existing XIUI navigation. The implementation now adds the crossbar on the existing host/focus foundation.

## Source evidence

- Lua reference: `XIUI/modules/hotbar/{controller,crossbar,devices,buttondetect,macropalette}.lua`.
- Lua reference: `XIUI/core/settings/factories.lua: createCrossbarDefaults`.
- Native integration: [src/host/controller_input.hpp](../../src/host/controller_input.hpp).
- Native integration: [src/shared/controller_input.hpp](../../src/shared/controller_input.hpp).
- Native integration: [src/runtime/hud_runtime_controller.cpp](../../src/runtime/hud_runtime_controller.cpp).
- Native integration: [docs/controller-implementation.md](../../docs/controller-implementation.md).

Lua paths are relative to the reference repository identified in the parity audit; brace lists name multiple files.

## Scope

- Controller slot clusters, left/right trigger banks, combined trigger modes, palette switching, mapping-aware prompts and keyboard/mouse access to the same slots.
- Configurable crossbar layout, visibility, action storage and pet-aware bank selection; deliberate native macro replacement when enabled.

## Dependencies

P02 action slots/executor; P04 palette selection. The current controller host/focus foundation is already implemented.

## Implementation

1. Extend normalized semantic actions for trigger-bank entry, bank transitions, slot selection and palette cycling. Inspect effective XInput/DirectInput mappings and the installed SDK state-copy contract; do not reuse raw numeric IDs across namespaces.
2. Render crossbar banks using P02 slots and P04 palette identities. Track simultaneous trigger order/neutral transitions explicitly so a bank change cannot activate a slot.
3. Add crossbar ownership to the existing host routing/mailbox and runtime focus system. Decide suppression before native processing and drain owned held state through release; keep callbacks independent of the runtime mutex.
4. Expose mapping conflicts, prompt fallback and per-bank settings through shared config controls. If the host/runtime contract must change, coordinate its version and installation; public output remains one DLL.

## Input and controller behavior

Movement/camera remain available where crossbar navigation permits. Native text, menus and spell/ability subtargeting keep ownership. Disconnect, focus loss, profile changes, zoning and replacement cancel the bank without executing anything.

## Completion criteria

- [x] Fresh slot events are dispatched once; transitions and held controls cannot activate another bank without neutral input.
- [x] Native controls remain available unless explicit replacement is enabled; owned holds/releases drain during handoff.
- [x] Setup exposes resolved mapping/readiness; neutral position labels avoid inventing device identity.
- [ ] Cody's physical XInput/DirectInput/remapper and live action acceptance.

## Focused validation

Focused normalizer and runtime fixtures for bank transitions, state/event deduplication and held releases. Physical XInput/DirectInput/remapper gameplay acceptance remains with Cody.

Apply the [shared implementation contract](README.md#shared-implementation-contract), then update this plan and the audit when the feature is integrated.

