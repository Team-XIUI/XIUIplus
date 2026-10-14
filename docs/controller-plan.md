# XIUI controller support plan

Research date: October 14, 2026, including a follow-up AgentLink investigation. Status: proposal; bounded live diagnostics were performed, but functional controller navigation has not been established or tested on hardware. This work changes documentation and agent guidance only. See [the follow-up evidence and limits](controller-agentlink-findings.md).

## Recommendation

Add one shared controller input and focus system, then adapt Chat, Party, configuration and the remaining interactions to it. Keep the current HUD, profiles and styling. The mode should change navigation, button prompts and focus treatment; it should not move modules or reset their settings.

Put a compact **Input mode** control in the config's top-right area, before the existing utility icons. Offer **Auto**, **Keyboard & Mouse**, and **Controller**. Auto is the recommended default: meaningful input changes prompts, while controller navigation requires deliberate focus. Both devices continue to work in every mode. A connected controller or a drifting stick must never open menus or acquire focus.

Prioritize **chat history and party targeting**, followed by chat composition and the rest of configuration. Treat text entry as a separate deliverable: browsing chat with a controller does not establish that controller-only typing works.

## What the research establishes

These are historical FFXI conventions. The official guide assigns movement/camera to the sticks, menu/target navigation to the D-pad, confirm/cancel to A/B, active-window selection to Y, software-keyboard entry to RB, and macros to the triggers. Current Windows configuration data agrees with the core face-button assignments, but successful controller input has not been observed; RB's documented configuration slot is screenshot, so keyboard opening must be verified contextually. [Square Enix: basic actions](https://www.playonline.com/ff11newplayer/tips/en/list.html).

The Xbox manual also assigns X to the main menu, LB to autorun, Back to HUD visibility, and Start to logout. These occupied buttons explain why a new global shortcut needs conflict checks. [Square Enix: Seekers of Adoulin manual, game controls](https://support.na.square-enix.com/document/manual/20/FFXI_SeekersOfAdoulin_Xbox360.pdf#page=3).

Windows mappings are configurable: Ashita documents 27 `padsin000` action slots, different DirectInput/XInput assignments, and a selected-controller GUID. The queried local mapping has 32 values: retain unknown trailing fields. Resolve effective settings, including boot overrides; do not interpret Windows button masks as FFXI mapping IDs. [Ashita: gamepad configuration](https://docs.ashitaxi.com/usage/configurations/#padsin000).

Ashita exposes both controller backends and can intercept input before the client receives it. Its SDK offers event and state callbacks; the exact blocking/order behavior on our installed client remains an implementation investigation. [Ashita: input hooks](https://docs.ashitaxi.com/features/#directinput--xinput-hooks), [official SDK](https://github.com/AshitaXI/Ashita-v4beta/blob/main/plugins/sdk/Ashita.h).

Party and alliance subtarget macros are distinct native operations, including `<stpt>` and `<stal>`. XIUI must preserve the pending action's target rules. [Square Enix: party/alliance subtarget update](https://www.playonline.com/ff11eu/polnews/news14970.shtml).

The Windows client has a native in-game software keyboard, added under **Config → Misc. 4 → Software Keyboard**. It should be the first controller-only composition route to investigate. Its availability/setting and XIUI coexistence on this client are still pending, rather than its existence on Windows generally. [Square Enix: Windows keyboard introduction](https://forum.square-enix.com/ffxi/threads/49425).

## What the live follow-up changes

The current session reported controller enumeration disabled by Ashita, no connected XInput device through the tested export, and one active party slot. A bounded queued press/release produced no observed controller callbacks. XIUI's initially unavailable development runtime was subsequently traced to a zero-filled installed DLL and repaired; loading, visible HUD and GPU text were confirmed. The controller prerequisites still require resolution before functional tests. No gamepad settings were changed; temporary diagnostic callbacks were removed. [Detailed observations](controller-agentlink-findings.md).

General → Input should distinguish **configured**, **connected**, **observed input**, and **XIUI runtime ready**. Explain when native gamepad settings or Ashita enumeration prevent input. Pinning Controller presentation should still work for setup, but must not silently enable enumeration, pick another device or change background-input policy. Offer targeted instructions for the actual missing prerequisite.

## Current XIUI foundations and gaps

- `src/host/plugin.cpp`: registers keyboard/mouse callbacks and tracks consumed keyboard presses through release. There is no controller registration.
- `src/shared/runtime_api.hpp`: ABI 8 exposes mouse and keyboard entry points; it has no controller contract. The first controller change therefore needs a matching development host/runtime update, as well as the standalone release.
- `src/runtime/hud_runtime.cpp`: already distinguishes native input, targeting, chat and configuration. `HandleTargetMouse` validates entity identity before selecting a party/enemy target. That validation can inform shared target actions, but pointer simulation should not become controller navigation.
- `src/runtime/modules/chat/chat_history.cpp`: already provides compact focus, full-screen history, ten tabs, paging and message-based scrolling. Its entry points and help text are keyboard/mouse based.
- `src/runtime/chat/native_chat_input.cpp`: adopts certain native command/tell drafts. This does not prove that native software-keyboard composition or arbitrary plain text can transfer safely.
- `src/runtime/modules/party/party_source.cpp`: already reads a native party cursor and derives selected/subtarget states, including a slot-based path. Its guarded client offsets need to remain an optional capability.
- `src/runtime/modules/party/party_module.cpp`: has row layout, target highlighting and pointer hit testing. It lacks a separate controller browsing cursor and context actions.
- `src/runtime/config`, `hud_runtime_choices.cpp` and `hud_runtime_profiles.cpp`: reusable controls/dialogs exist, but not a complete focus graph covering all of configuration.

## Input mode and focus

### Presentation

The top-right control displays the effective device and opens the three mode choices. A controller user can reach it through the config's utility focus region; its purpose is also visible while focused. General gains an **Input** page for bindings, prompt style, repeat/dead-zone settings and a small input-status display.

In Auto, keyboard typing inside a controller-opened editor should not flip the navigation legend on every character. Keep the active interaction's prompts stable; switch on a deliberate navigation/device action, while showing text-editing shortcuts where relevant. Gameplay analog motion should not steal an explicitly mouse-focused config region.

Auto responds to button presses, intentional stick movement, mouse actions and keyboard use. Ignore stick drift, stale packets and tiny pointer movement. Use a short settling interval to prevent prompt flicker. Switching prompts must preserve the current field, draft, selected row and scroll position. Forced modes pin presentation, rather than disabling the other device.

Use the player's effective Confirm/Cancel bindings, with Xbox, PlayStation or neutral prompts. DirectInput does not reliably identify a controller family; offer a manual glyph choice. Prompts should describe actions when a mapping is unknown.

### Entry and exit

Aim for the native **Select active window** action to reach XIUI chat when the client has actually focused its log window. That integration needs a reliable active-window signal: today's generic menu snapshot does not provide one. Passing every Y press into XIUI would break native menus.

Live menu-list presence and `GetIsMenuOpen()` are insufficient signals: the first lists attached panels, and the latter was also true during native text input. Establish active-log identity and distinguish `Closed`, `OpenedChat` (`0x11`) and `OpenedOther` (`0x21`) before implementing this bridge.

Provide an explicit remappable **XIUI Focus** entry as the dependable alternative. It opens a small focus picker for available Chat, Party, Ready Check and Config surfaces. Choose its button/chord in setup with collision feedback; do not silently claim Start, Back or a macro trigger. Proposed command equivalents such as `/xiuin focus chat` are future implementation work, not currently available commands.

Once XIUI owns a surface, Cancel backs out one layer and ultimately returns control to the game. Display a persistent small action legend while focused. Keep controller focus distinct from pointer hover: a parked mouse must not continually move it.

### Input ownership

Use an explicit owner rather than one global controller-enabled flag. Owners include the game/native menus, native spell subtargeting, XIUI configuration/dialogs, party browsing, chat history, chat composition and native text entry.

An existing XIUI dialog gets its own controls. Native software keyboards, IME and native text editors retain theirs. Pending native targeting retains its D-pad and Confirm/Cancel unless the user explicitly opens an unrelated XIUI surface; suspend that XIUI surface when returning to the pending action. Hiding a native window does not mean it is inactive.

While XIUI owns a modal surface, consume navigation, confirmation and underlying movement/camera state consistently. Drain consumed controls through release and require neutral before handing a held stick back. Pressing Cancel to close config must not also cancel a spell or move the camera. Outside XIUI focus, leave gameplay controls alone. Native menus, cutscenes, zoning, loss of window focus, device disconnect, profile changes, runtime replacement and unload need explicit cleanup paths.

## Chat: first major feature

### History and tabs

Reuse the existing ten filters: Window 1, Say, Tell, Party, LS1, LS2, Assist E, Assist J, Unity and Shout. Browsing a filter must not silently change the channel used to send messages.

Proposed controls while XIUI chat history owns focus:

- **Confirm:** expand compact history; in expanded history, collapse it, matching the existing Enter behavior.
- **Cancel:** collapse first, then leave chat focus.
- **D-pad Up/Down:** move by one complete message, including wrapped lines.
- **D-pad Left/Right:** page backward/forward.
- **Previous/Next tab:** shoulder buttons where available; **Select active window** cycles forward while expanded, matching the existing F action. These are contextual assignments, not global steals.
- **Compose/Reply actions:** explicit entries in the focused chat actions menu; reply uses a validated last-teller/recipient.

Preserve each tab's scroll, support Window 2 when enabled, retain keyboard shortcuts and wheel scrolling, and replace hardcoded keyboard-only footer text with shared action prompts. Empty tabs still have a clear way back. New messages should not pull a reader away from older history.

### Composition and sending

Support controller-plus-keyboard immediately: controller users can choose a channel/recipient and browse, then type into the existing editor. Keyboard typing must not lose the selected channel or controller focus context. This is the first usable chat milestone.

For controller-only typing, prioritize the existing native Windows software keyboard. First read its current Off/Full setting and verify its actual opening action without changing that preference automatically. Then verify visibility when XIUI replaces native chat, text ownership, caret, cancel, auto-translate, IME and exactly-once submission on this client. Existing slash/tell handoff is insufficient evidence.

If native composition can coexist safely, temporarily expose the required native text surface, preserve the draft/recipient, then return to XIUI. Let the native editor submit directly when transfer is unreliable; do not submit the same message again from XIUI. Restore native visibility on success, cancel, error and unload.

If that bridge is unsuitable, a compact XIUI software keyboard becomes a separate milestone, with D-pad navigation, explicit Send/Cancel, editing, channel/recipient selection and an auto-translate route. Until either path is proven, label controller-only composition incomplete. Do not promise an on-screen keyboard as an existing Windows/Ashita capability.

Never bind the Confirm used to enter chat to immediate sending. Use a fresh press and a visible Send action. Preserve the current unsent draft when backing out; leave native `/`, tell, command-menu and spell-selection input paths working.

## Party: native targeting plus deliberate browsing

### During a native spell or ability selection

Let FFXI own D-pad movement and Confirm/Cancel for `<st>`, `<stpc>`, `<stpt>`, `<stal>` and relevant action selectors. XIUI mirrors the native cursor using the existing party source, with a clear distinction between the enemy hard target and the pending friendly subtarget.

Do not convert every highlighted party row into a hard target or queue a new spell. Do not require a visible world-model/entity index for a native slot-based selection: a member changing equipment can temporarily lack that model. When the native party-cursor capability is unavailable, retain the supported target state and native UI fallback.

### Browsing XIUI party members

Explicit Party focus opens a separate browsing cursor. D-pad Up/Down moves through visible members; Left/Right changes enabled party/alliance groups. Confirm commits a validated normal target when legal, or activates a context-menu choice. A separate Actions command opens available interactions such as Tell or Inspect. Cancel leaves browsing without changing the existing hard target.

Browsing alone must not retarget. Persist focus by member/server ID plus group/slot, not a sorted visual row number. Recover safely when a member leaves, the group reorders, or an alliance group is hidden. Out-of-zone members can remain inspectable while world-target actions are disabled. Keep native slot eligibility separate from XIUI's visual browsing eligibility.

Share target validation and actions with mouse input. Use the existing native Tell/Inspect paths where available, preserving recipient identity. Leader-only or destructive party management is outside the first delivery; any later additions need explicit availability and confirmation.

## Configuration and remaining modules

Keep the existing category/header/content hierarchy. Define reachable focus regions for categories, top tabs, element selectors, settings, profiles and title-bar utilities. A focus action moves between regions; D-pad navigates within one. Scroll the focused control into view.

Each shared config component gets controller semantics once: toggle, cycle button, dropdown, slider, number editor, text field, color picker, icon button, tabs and modal. Confirm enters adjustment, Left/Right changes values, and Cancel backs out predictably. Directions may repeat; confirmation, sending and destructive actions must not repeat. Focus displays tooltip/help content currently available only on hover. Delete profile retains its red confirmation and initial Cancel selection.

Movement preview gets a module list and a deliberate Move mode: D-pad nudges by the chosen snap amount, a modifier gives fine adjustment, and Center X/Center Y remain real actions. Keep pre-move bounds for Cancel and commit through normal position saving. Reuse the grid/snapping foundations; no stick-as-mouse requirement.

Ready Check buttons become focusable actions. Equipment gets slot navigation and focus-driven item tooltips without adding equip behavior that does not exist today. Interactive enemy rows use the shared target action. Inventory and other purely informational HUDs need no meaningless focus stops; any existing item/bag actions must be reachable. Passive bars remain passive.

Keep current HUD text profiles, fonts/effects, native containers and UI sounds. Focus sound plays on an actual focus change, selection sound on activation, and send sound on successful submission. Hover must never be the only way to reveal required information.

## Implementation phases

1. **Prove readiness and input routing.** Establish a usable XIUI runtime, native/Ashita pad settings, selected backend/device and successfully observed input. Resolve effective mappings, preserving unknown fields; inspect installed callback semantics and native active-window/software-keyboard signals. A queued injection is not a received input event or a virtual controller. Confirm press/release/state suppression and duplicate-event behavior. Deliver no guessed global binding.
2. **Shared input and focus.** Add host adapters for DirectInput/XInput, normalized actions, ownership, repeat/release handling, the Input mode control, focus picker, visible focus and config primitives. Update the host/runtime contract together. Preserve the standalone one-DLL release.
3. **Chat history and Party.** Connect existing history actions, tabs and prompts; mirror native subtargets; add deliberate party browsing and validated target activation. These are the first gameplay priorities.
4. **Chat composition.** Ship the verified native keyboard bridge, or implement the separate XIUI keyboard if necessary. Include channel/recipient selection, cancel/draft preservation, auto-translate and exactly-once Send.
5. **Complete configuration and interactions.** Cover profile dialogs, color controls, layout nudging, Ready Check, equipment, enemy rows and all remaining actual actions. Check hybrid use and save/reload behavior.

## Proposed file organization and persistence

Use `src/host/controller_input.*` for Ashita/backend adaptation and `src/shared/controller_input.hpp` for a bounded, versioned contract. Place action mapping, ownership, focus and repeat logic under `src/runtime/input/`; put focus visuals and controller behavior in the existing shared config controls. Keep chat and party behaviors beside their modules. These names are proposals, not existing files.

Keep callbacks small and route state into the runtime through a defined serialized boundary. Avoid independent polling paths that cannot suppress the game's copy, callback re-entry and double handling from event/state callbacks or remappers. Register/remove both backends through the host's lifetime gate.

Make the blocking decision synchronously at the input boundary; merely enqueueing UI work for the next render is too late to prevent native handling. Keep ownership visible to that boundary without unsafe cross-thread UI mutation. Verify suppression of the returned controller state as well as individual button events. Preserve injected-input provenance to avoid replay loops, but do not discard every injected event: legitimate remappers may generate them.

The presentation preference can live with profile preferences. Physical-device selection, calibration and local bindings should stay local, excluded from profile share/import. Unknown or imported device identities must never select a random connected pad. Extend the persistence/export rules intentionally rather than assuming today's preference visitor already separates hardware settings. Focus, held controls and detected activity are transient.

The first host callback/ABI change cannot be delivered through a runtime-only hot reload to the current host. Plan a coordinated development host/runtime installation after saving/draining settings; later runtime-only refinements can resume hot reload. Keep the immutable XIUI2 sharing preset unchanged.

## Focused acceptance checks for future implementation

- One XInput device, one DirectInput device, remapped Confirm/Cancel and a controller exposed through a remapper. Validate the selected backend/device; do not process one press twice.
- Check distinct numeric namespaces with known examples: local A maps to FFXI value 3, Ashita event 12 and Win32 mask 0x1000; Y maps to 2, 15 and 0x8000. Retain the local 32-entry configuration without inventing meanings for trailing entries.
- Gameplay untouched outside XIUI focus; native macros, autorun, targeting and menus remain usable. Native action selection and hidden native windows do not leak confirmation into XIUI or vice versa.
- Close config while holding Cancel or a stick: no camera motion, attack, native menu action or stuck control on return.
- Chat expands, tabs/page/message scrolling work, drafts survive, recipient stays correct, native keyboard cancels safely and a message sends once. Cover `/` while targeting and native Tell entry.
- Party browsing preserves hard target until activation; native subtarget confirms once. Check equipment blinking, invalid/out-of-zone entities, party reorder and disabled alliance displays.
- Every actual config action is reachable; dropdowns scroll; a long list keeps focus visible; hover-only details are available through focus. Disabled/removed controls cannot trap navigation.
- Device unplug, Alt-Tab, cutscene, zoning, hidden HUD, profile switch and runtime replacement release capture safely. Both devices continue to work without resetting fonts, layouts or preferences.

Use small synthetic action/ownership checks where they prevent duplicate activation or leaked releases. Separately establish that an emulated device/input actually reaches the hooked game path before using it to judge behavior. Device-level tests need observed press/release, held state and native-state transitions. Solo and synthetic tests cannot replace multi-member, alliance, hotplug or real-device checks. Cody checks visual feel and gameplay; run only the focused live experiments authorized for the investigation.

## Decisions still requiring implementation evidence

Configuration observations now identify XInput enabled in the queried Windows settings, no boot map override, and Ashita enumeration disabled at runtime. Successful game-path input and the fully effective native map remain unproven. Native active-window identity; current keyboard setting, visibility and plain-text handoff; installed callback blocking order; held-state behavior; real party/alliance navigation; and remapper coexistence still require evidence. The follow-up report distinguishes observations from inconclusive probes and pending functional tests.
