# Mouse, keyboard and controller ownership

This is the current input contract. Feature-specific actions belong in the [module notes](../modules/README.md); old probes and proposed designs are in [the archive](../archive/README.md).

## Source owners

- Host callbacks and native suppression: [plugin.cpp](../../src/host/plugin.cpp), [keyboard_suppression.hpp](../../src/host/keyboard_suppression.hpp) and [controller_input.cpp](../../src/host/controller_input.cpp).
- Normalized actions and cached routing: [controller_input.hpp](../../src/shared/controller_input.hpp), [controller_mailbox.hpp](../../src/host/controller_mailbox.hpp) and [runtime_host.cpp](../../src/host/runtime_host.cpp).
- Runtime focus/activation: `src/runtime/hud_runtime_controller*.cpp`, [hud_runtime.cpp](../../src/runtime/hud_runtime.cpp) and `src/runtime/input/`.
- Configuration: [config_session.hpp](../../src/runtime/config/config_session.hpp), shared control geometry and `hud_runtime_config*.cpp`.

Read the ABI from [runtime_api.hpp](../../src/shared/runtime_api.hpp); do not copy a version number from an old plan. Follow [the controller skill](../../.agents/skills/xiui-controller-support/SKILL.md) when changing an interaction.

## Ownership rules

- Decide suppression before native processing. A queued action alone cannot block the original input.
- Keep native callbacks independent of the runtime/render mutex. Read cached routing, suppress immediately, and queue accepted semantic actions for the render thread. Expire stale work on suspension or replacement.
- Distinguish DirectInput IDs, XInput event IDs, masks and native mapping slots. Resolve actual configured bindings; historical A/B/Y conventions are not universal.
- Separate native configuration, Ashita enumeration, observed connection and runtime readiness. Auto/Keyboard & Mouse/Controller presentation changes prompts, not device settings.
- Respect an existing input owner. Track owned press, repeats, held state and release; one-shot Send/Confirm/Delete must not repeat. Require neutral before returning a captured stick.
- Release transient focus, dialogs and gestures on foreground loss, disconnect, hidden/disabled context, zoning, profile/runtime replacement and unload. Preserve drafts and committed layout.

## Mouse and locked HUD

Hover capture blocks native gameplay without blocking XIUI's own controls. `PointerInputBlocked` unwraps XIUI's capture state while retaining the previous mouse block. Satchel, Action Bars management and Treasure Pool accept pointer interaction over ordinary native menus.

Window visibility, pointer capture, text focus and deliberate browsing are separate. Clicking a bag/item or blank window space must not enter keyboard browsing. An explicitly opened editor/popup owns its required keys until dismissal. Unclaimed keyboard/controller navigation stays with the native menu. Native text entry, spell/ability subtargeting and cutscenes retain their existing guards.

## Keyboard

Follow native Keyboard Size through `input/keyboard_policy.hpp`. Compact layouts retain character shortcuts, including F-to-focus; Full mode leaves unfocused letters, digits, punctuation and Space available to native typing. Missing settings also preserve typing. Explicitly focused editors and deliberate browsing retain their keys.

Keyboard suppression tracks window-message gestures, immediate state and buffered events independently. Keep physical scan-code/extended-key identity, drain owned releases after a surface closes, and preserve other keys. A held Enter that sends chat cannot also open a native command menu.

## Controller

`/xiui focus` opens the picker; `/xiui focus <area>` enters a supported surface and `off` releases it. Focus uses stable identities, visible feedback and the same actions as pointer/keyboard controls. Directions repeat; activation and destructive confirmation require a fresh press. Configuration/modals can capture exclusively; ordinary chat-history browsing leaves movement/camera sticks available.

The configured Cycle Window action follows Chat 1 → Chat 2 → Buffs → Debuffs, skipping unavailable windows/effects. Enter/Confirm expands focused chat history; Cancel returns one layer. Gameplay Up/Down remains native party targeting. Explicit XIUI party browsing highlights members without changing the hard target until activation and yields to native menus/subtargets.

## Chat handoff

Composition, compact focus and expanded history are distinct. Preserve channel, recipient, draft and caret when adopting native slash/tell/Full-keyboard input; opening chat does not send it. Alt+R/Ctrl+R use one reply cycle. Enter sends once and remains owned through release. Native CHAT activation keeps its own initiating Enter release before handoff.

Native chat input (`0x11`) and other text editors (`0x21`) are different owners. Existing draft adoption does not establish arbitrary software-keyboard/IME transfer. See [Chat](../modules/chat.md) and [input details](chat-input.md).

## Remaining acceptance

Physical devices, remappers, hotplug and multiplayer cases need observed gameplay/hardware acceptance. AgentLink can forward existing commands, but accepted `QueueButtonData` or synthetic semantic actions do not prove delivery through actual callbacks.

Controller-only composition still needs verified native software-keyboard, IME and auto-translate ownership/transfer. Preserve drafts on Cancel and establish exactly-once send before extending it. Keep the game's software keyboard separate from PlayOnline login input.

[Controller research](../archive/controller-research-2026-10-14.md) and [AgentLink evidence](../archive/controller-agentlink-2026-10-14.md) preserve the native findings. They are not standing authorization for live experiments.

For a changed boundary, choose `controller-input`, `controller-config`, `cursor-ownership`, or the module's focused ownership argument from [the check guide](../../tests/README.md).
