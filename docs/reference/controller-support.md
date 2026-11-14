# Controller support

Shared requirements for XIUI+ keyboard, mouse and controller interactions.

Read [project guidance](../../AGENTS.md), [shared foundations](../foundations.md) and [input ownership](input.md) first for implemented commands, semantic actions and native-keyboard/hardware limits. Resolve the ABI from `src/shared/runtime_api.hpp`. Reuse current normalized input, config session and focus foundations.

`docs/reference/input.md#remaining-acceptance` records remaining acceptance. Historical proposals are not available APIs or standing authorization for live experiments. A request for research or planning authorizes documentation, not application implementation.

For live diagnostics, AgentLink can forward existing Ashita diagnostic commands, but an accepted `QueueButtonData` call does not prove a connected or emulated controller, delivered input or successful navigation. Confirm runtime readiness, device/backend availability and observed callbacks/state transitions first. Honor the task's authorization for experiments and preserve current user work.

## Native behavior and input contract

- Resolve effective FFXI/Ashita bindings. The official guide's A/B confirmation, Y active-window selection, RB software keyboard and trigger macros are historical conventions, not universal bindings. DirectInput IDs, XInput event IDs, XInput bit masks and FFXI mapping slots are different namespaces.
- Treat native gamepad configuration, Ashita's runtime enumeration policy and actual device connection as separate prerequisites. Do not silently change them when selecting a presentation mode. Published mapping lengths may lag the client; preserve unrecognized fields and configured overrides.
- Inspect the installed SDK. Use Ashita's controller interfaces and the host/runtime contract; support DirectInput and XInput. Avoid duplicate event/state processing and remapper-generated duplicate actions. Register and unregister callbacks through the host lifetime gate.
- Normalize device input to shared actions. Mouse, keyboard and controller should activate the same module operations and validation; do not build a second behavior path by synthesizing clicks or globally injecting Enter/Escape.
- Keep native movement, camera, macros, menus and targeting available outside explicit XIUI+ focus. Respect an already-blocked event and other input owners. A hidden native window may still own input.
- Consume owned press, held state and release consistently. Handle direction repeat separately from one-shot activation. Require neutral after releasing a captured stick. Never let closing a dialog also act in the game.
- Chat browsing captures navigation only: movement/camera sticks and unrelated keyboard movement remain available. Typing retains keyboard text ownership; config/modals may capture exclusively. Party browsing yields to the live native menu-input flag, including main menu, inventory and equipment, rather than relying only on movable command-panel visibility.
- Keep native controller callbacks independent of the runtime/render mutex. Read cached routing, decide suppression immediately, and queue accepted actions for the render thread; never invoke UI/native actions while holding the mailbox lock. Expire stale work and invalidate it on suspension, profile changes and runtime replacement.
- Decide suppression before native input processing; next-frame queued work alone cannot block the original input. Verify the installed state-copy contract, not only per-button blocking. Preserve provenance to prevent replay loops without rejecting legitimate remapper input merely because it is injected.
- Release/suspend ownership on disconnect, lost game-window focus, zoning, cutscenes, hidden/disabled UI, profile changes, runtime replacement and unload. Do not globally change Ashita's background-controller policy.

## Requirements for interactive features

- Every action needs a controller route when its surface is focused: enter, navigate, activate, cancel/back and adjust where applicable. Pure display elements need no artificial focus stops.
- Use stable focus IDs, deterministic ordering, visible focus, focus-driven help/tooltips and scroll-into-view. Keep focus separate from hover. Hidden/disabled/removed controls must not trap navigation.
- Share behavior for dropdowns, sliders, tabs, text/numeric editors, color pickers, icon buttons and dialogs. Preserve explicit confirmations and existing Cancel defaults for destructive actions. Never repeat Confirm, Send or Delete from a held button.
- A mode preference changes prompts/navigation presentation; it does not reset profiles, move modules or disable the other device. Auto detection ignores drift and preserves drafts and focus. Resolve prompts from the active mapping, with a generic/manual glyph fallback.
- Preserve the project's text profiles, native control styling and UI sounds. Focus movement can reveal hover help; it must not silently activate the hovered action.

## Chat and party rules

- Keep chat history focus, expanded history and composition distinct. Reuse the existing tab filters, message-based scroll and per-tab position. A history filter is not automatically a send-channel change.
- Controller browsing plus keyboard typing is a valid milestone, but is not controller-only composition. Verify native software-keyboard/IME/auto-translate ownership and visibility before promising a bridge. Existing native slash/tell draft adoption does not prove arbitrary text transfer.
- Prefer the existing Windows in-game software keyboard under Config → Misc. 4 when available. Read its current setting without changing it automatically. Do not confuse it with the PlayOnline password keyboard. Native chat input (0x11) and other text editors (0x21) need different ownership; generic menu visibility is not active-log focus.
- Preserve draft, channel and recipient on cancel or device change. Opening chat must not send it. One explicit activation sends once, through the existing validated path; never submit again after native submission.
- During native spell/ability subtargeting, let FFXI own navigation and confirmation. Mirror its party cursor; do not turn highlight movement into a hard-target change or new cast.
- Separate a XIUI+ browsing cursor from game hard target and native subtarget. Keep member identity stable across row reorder, joins/leaves and hidden alliance groups. Slot-based native targeting must not depend on a visible world model during equipment changes.
- Revalidate identity and availability at activation. Out-of-zone inspection and world-target eligibility are different. Share normal target actions with pointer input; do not duplicate spell execution.

## Persistence and validation

Keep hardware identity/calibration/local bindings out of portable profiles; inspect/update the actual export and overlay rules when introducing them. Focus and held controls are transient. Preserve settings and drain writers before a coordinated host/runtime ABI update. Do not modify the immutable XIUI2 preset.

Use a focused check for ownership, exactly-once activation, stale focus or persistence when the change warrants it. Describe controller support honestly if an input foundation or native capability is still missing. Cody checks visual/gameplay behavior; no broad suites or hardware manipulation without task-specific need.

For authorized live probes, separate deterministic action tests, supported device-injection tests and actual native gameplay checks. Use an observed press/release and a native transition to establish input delivery. Track and remove temporary diagnostic handlers; include neutral releases. If the user types, moves or loads UI during a probe, mark affected before/after comparisons inconclusive and never restore an older draft over their current one. A solo session cannot validate alliance selection.

## Primary references

- [Square Enix basic controls](https://www.playonline.com/ff11newplayer/tips/en/list.html)
- [Ashita pad mappings and effective configuration](https://docs.ashitaxi.com/usage/configurations/#padsin000)
- [Ashita controller hooks](https://docs.ashitaxi.com/features/#directinput--xinput-hooks)
- [Official SDK](https://github.com/AshitaXI/Ashita-v4beta/blob/main/plugins/sdk/Ashita.h)
- [Windows in-game software keyboard](https://forum.square-enix.com/ffxi/threads/49425)
- [Official addon event/state contracts](https://github.com/AshitaXI/example/blob/main/example.lua)

Start local investigation at `src/host/plugin.cpp`, `src/shared/runtime_api.hpp`, `src/runtime/hud/hud_runtime.cpp`, `src/runtime/modules/chat/chat_history.cpp`, `src/runtime/chat/native_chat_input.cpp` and `src/runtime/modules/party/party_source.cpp`.
