# Mouse, keyboard and controller ownership

This is the current input contract. Feature-specific actions belong in the [module notes](../modules/README.md).

## Source owners

- Host callbacks and native suppression: [plugin.cpp](../../src/host/plugin.cpp), [keyboard_suppression.hpp](../../src/host/keyboard_suppression.hpp) and [controller_input.cpp](../../src/host/controller_input.cpp).
- Normalized actions and cached routing: [controller_input.hpp](../../src/shared/controller_input.hpp), [controller_mailbox.hpp](../../src/host/controller_mailbox.hpp) and [runtime_host.cpp](../../src/host/runtime_host.cpp).
- Runtime focus/activation: `src/runtime/hud/hud_runtime_controller*.cpp`, [hud_runtime.cpp](../../src/runtime/hud/hud_runtime.cpp) and `src/runtime/input/`.
- Configuration: [config_session.hpp](../../src/runtime/config/config_session.hpp), shared control geometry and `hud/hud_runtime_config*.cpp`.

Read the ABI from [runtime_api.hpp](../../src/shared/runtime_api.hpp). Follow [controller support](controller-support.md) when changing an interaction.

## Ownership rules

- Decide suppression before native processing. A queued action alone cannot block the original input.
- Keep native callbacks independent of the runtime/render mutex. Read cached routing, suppress immediately, and queue accepted semantic actions for the render thread. Expire stale work on suspension or replacement.
- Crossbar routing carries a transient verified native focus slot. Host callbacks and the mailbox re-read its focused window before allowing command-menu or replacement macro-palette input; a cached exception cannot consume a newly opened native menu. Native text and target selection keep their existing guards.
- Distinguish DirectInput IDs, XInput event IDs, masks and native mapping slots. Resolve actual configured bindings; historical A/B/Y conventions are not universal.
- Separate native configuration, Ashita enumeration, observed connection and runtime readiness. Auto/Keyboard & Mouse/Controller presentation changes prompts, not device settings.
- Respect an existing input owner. Track owned press, repeats, held state and release; one-shot Send/Confirm/Delete must not repeat. Require neutral before returning a captured stick.
- Suspend ownership on foreground loss while retaining the selected window, chat draft, caret, channel and recipient; returning resumes that focus. Pointer/resize gestures end and queued controller input is invalidated. Disconnect, hidden/disabled context, zoning, profile/runtime replacement and unload release transient focus normally.

## Mouse and locked HUD

Opening settings leaves Edit Mode OFF. Live modules retain hover editing; selecting a module previews it. The shared footer toggle and controller Utilities action explicitly preview all enabled modules. Render visibility also gates grips, dragging, context menus, snapping and controller movement entries, so hidden modules cannot capture input.

Edit Mode's right-edge two-icon selector selects XIUI or Native UI editing through shared pointer/controller Utilities actions. Native editing exposes separate native window/group movement adapters and Show/Hide controls; XIUI replacement modules leave the edit surface. Native artwork can be hidden while its native input remains active. Native outlines stay available for restoring visibility; placement/hides use validated live native owners and do not execute purchases, sales or other game actions. Mode selection is transient and captured gestures stay owned through release.

Hover capture blocks native gameplay without blocking XIUI+'s own controls. `PointerInputBlocked` unwraps XIUI+'s capture state while retaining the previous mouse block. Satchel, Action Bars management and Treasure Pool accept pointer interaction over ordinary native menus.

Opening Configuration ends shared HUD hover capture before saving its previous mouse-block state, so hovering or clicking a hotbar cannot become a false external input owner. Configuration uses ordinary clicks without Shift. Shared [mouse normalization](ashita-compatibility.md#mouse-boundary) owns press/release state from button messages, including callbacks with zero data. A gesture stays with the surface that received its press until the matching release, so the game cannot see a drag XIUI+ started. Without a release message, a gesture ends only on foreground loss, a repeated press of that button, or corroborated polling. Window capture only extends delivery outside the window; losing it never ends a gesture. Cancellation never activates a pending close or menu action.

`/xiuiplus mouse status` reports the Ashita core/interface version, module, SDK layout, capture state and the last six plain plus six Shift left/right button events. Each entry includes raw data, normalized keys, polled buttons, coordinates, routing owner, handling and config held-state transitions. The gesture line reports how the last left gesture ended, ignored and recovered polled releases, repeated presses, host movement flags, swapped buttons, capture losses and the input/frame/window thread IDs. For an intermittent click failure, run `/xiuiplus mouse clear`, open Configuration and try a plain click followed by a Shift-click before reading status; pair the output with `/xiuiplus actionbars status`. The separate histories preserve plain-click evidence while using Shift to close settings. The bounded trace stays in memory and does not change settings or generate per-click log output.

Native chat cleanup preserves pointer gestures and pending releases owned by another XIUI+ surface. Repeated command-menu/combat polling must not cancel workspace drags or button clicks between press and release. Foreground/input suspension still releases capture immediately.

Configuration, color-picker and profile/import close buttons activate on the matching release; releasing outside cancels the close. Action workspace close controls use the same press/release rule. Dismissing a popup keeps native mouse blocking and held movement capture until its owned release, even after the popup disappears.

Window visibility, pointer capture, text focus and deliberate browsing are separate. Clicking a bag/item or blank window space must not enter keyboard browsing. An explicitly opened editor/popup owns its required keys until dismissal. Unclaimed keyboard/controller navigation stays with the native menu. Native text entry, spell/ability subtargeting and cutscenes retain their existing guards.

## Keyboard

Follow native Keyboard Size through `input/keyboard_policy.hpp`. Compact layouts retain character shortcuts, including F-to-focus. Explicit number-row hotbar bindings take priority while gameplay input is available, even if keyboard settings are temporarily unavailable after zoning. In Full mode this applies only when the active layout types a digit there; AZERTY's unshifted é/è/ç/à keys are text and start composition. Unbound digits remain available for typing/native input. A fresh press replaces stale hotbar ownership from a missed release without repeating an already-held action. With XIUI+ chat replacing native chat, Full mode opens composition from a fresh letter, unbound top-row digit, punctuation or Space press without waiting for a hidden native editor. Numpad movement, Ctrl/Alt commands and existing input owners retain priority. Missing settings leave chat initiation native. Explicitly focused editors and deliberate browsing retain their keys.

Ashita keybinds stay suppressed while an XIUI+ surface captures the keyboard, and focused editors still type keys Ashita reports as bound; see [keyboard boundary](ashita-compatibility.md#keyboard-boundary). Keyboard suppression tracks window-message gestures, immediate state and buffered events independently. Cached keyboard routing reserves chat/status navigation before DirectInput polling, including compact F entry, while leaving unrelated typing, numpad movement and modified shortcuts available. Keep physical scan-code/extended-key identity and drain owned releases after a surface closes. A held Enter used by XIUI+ cannot also open native expanded chat or a command menu. The host reads this routing without taking the runtime mutex.

## Controller

First-load [Setup](setup.md) shares mouse, keyboard and controller actions. It captures input while visible, keeps held confirmation and pointer releases owned through dismissal, and yields to native text, targeting, events and foreground loss.

`/xiuiplus focus` opens the picker; `/xiuiplus focus <area>` enters a supported surface and `off` releases it. Focus uses stable identities, visible feedback and the same actions as pointer/keyboard controls. Directions repeat; activation and destructive confirmation require a fresh press. Configuration/modals can capture exclusively; ordinary chat-history browsing leaves movement/camera sticks available.

F/the configured Cycle Window action follows Chat → Buffs → Debuffs, skipping unavailable effects. When no XIUI+ chat window is shown and the native log is visible, the native log is the controller's chat stop: entering the cycle or leaving the last status group passes the press to the game, so its log and expanded history stay reachable beside XIUI+ Buffs. Window 2 is a tab inside expanded chat instead of a second compact focus stop. Enter/Confirm expands focused chat history; Cancel returns one layer. Gameplay Up/Down remains native party targeting. Explicit XIUI+ party browsing highlights members without changing the hard target until activation and yields to native menus/subtargets.

Inside expanded chat, F/Cycle Window visits Window 1 → Window 2 → Say → Tell → Party → LS1 → LS2 → Assist E → Assist J → Unity → Shout → Emotes, then wraps. Shift+F reverses that order. Window tabs retain their native message routing; channel tabs combine both windows. Native menu focus polling cannot replace the expanded log or reset its tab; native text editors and action targeting still take priority.

Window 2 is omitted when its XIUI+ window is disabled, the game uses a single log, or no message types route to the second native log. Keyboard and controller navigation skip it in both directions. A configured but quiet window stays available; removing the selected Window 2 tab returns to Window 1.

If the game's native expanded log becomes active, XIUI+ releases chat/controller focus and makes that native log visible until it closes. This overrides only the effective render mask, preserving saved HUD preferences, so a hidden native log cannot trap gameplay controls.

F/Cycle Window selects XIUI+ chat directly over the combat command menu; Enter/Confirm expands its log. Enter remains owned through its release so it cannot also activate the native menu. Equipment, inventory and other native menus keep the game's original cycle so Storage Options and Sort remain accessible. XIUI+ mirrors the validated native focus identity for replaced chat/status windows and releases navigation when focus returns to a native control. The host leaves the mapped cycle control's press, held state and release native in both controller backends while native cycling owns it. Native text editors and action subtargets still take priority.

Deliberate controller browsing of Buffs/Debuffs keeps its focus frame and navigation over the combat command menu. This captures navigation only with native menus open or closed, preserving movement/camera sticks; native text entry and action subtargets still take priority.

Within local Buffs/Debuffs, Confirm/A or Enter requests removal only for a client-marked cancellable effect. Navigation/hover never removes effects, held Confirm does not repeat, and Cancel/B exits browsing. Remote status inspection stays read-only.

## Chat handoff

Composition, compact focus and expanded history are distinct. Preserve channel, recipient, draft and caret when adopting native slash/tell/Full-keyboard input; opening chat does not send it. Alt+R/Ctrl+R use one reply cycle. Enter sends once and remains owned through release. Native CHAT activation keeps its own initiating Enter release before handoff.

Alt+Tab retains composition and window selection while suspending input. Each fresh opening of expanded history selects its first tab and starts at the newest messages with zero scroll in each history tab; a suspended, already-open history view retains its selected tab and position until deliberately closed.

Native chat input (`0x11`) and other text editors (`0x21`) are different owners. Existing draft adoption does not establish arbitrary software-keyboard/IME transfer. See [Chat](../modules/chat.md) and [input details](chat-input.md).

## Remaining acceptance

Automated checks do not cover physical devices, remappers, hotplug or multiplayer cases; these need to be observed in game on real hardware. An accepted `QueueButtonData` call or a synthetic semantic action does not prove delivery through actual callbacks.

Controller-only composition still needs verified native software-keyboard, IME and auto-translate ownership/transfer. Preserve drafts on Cancel and establish exactly-once send before extending it. Keep the game's software keyboard separate from PlayOnline login input.

For a changed boundary, choose `controller-input`, `controller-config`, `cursor-ownership`, or the module's focused ownership argument from [the check guide](../../tests/README.md).

Opening native chat with Space transfers even an empty chat session into XIUI+, preserving an unsent draft. Up/Down browse the same sent-command history used after opening with `/`; Down past the newest entry restores the current draft. Each newly opened session begins at the latest sent entry. Native search and other text editors keep their own input ownership.
