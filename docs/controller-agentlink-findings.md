# Controller investigation: AgentLink follow-up

Date: October 14, 2026, approximately 00:23–00:25 local time. Scope: the current development client, existing AgentLink/Ashita diagnostic interfaces, source inspection and primary documentation. No application source, release DLLs or saved game/controller settings changed.

## Observed client state

- AgentLink responded to `?state`, `?loaded xiuinative` and `?ui menus`.
- XIUI was initially unloaded. It was loaded during the session, independently of the research commands. Subsequent `/xiui status` reported the development host present but its runtime unavailable. A loaded plugin alone is therefore not a sufficient XIUI test prerequisite.
- The input manager reported `GetDisableGamepad() = true` and `GetAllowGamepadInBackground() = false`. These values were not changed.
- The live boot configuration returned `-1` for `padmode000`, `padsin000` and `padguid000`; no explicit override was supplied there.
- The queried Windows FFXI configuration contained `padmode000 = 1,1,0,0,0,1`, indicating gamepad and XInput enabled in that configuration. This is separate from Ashita's runtime enumeration-disable flag. The selected GUID was all zeros.
- `XInputGetState` through the tested `xinput1_3` export returned 1167 (`ERROR_DEVICE_NOT_CONNECTED`) for user slots 0–3. This establishes that no connected device was reported through that path; it does not establish the availability of every possible controller/remapper/backend.
- The party manager reported one active party slot. No native action/subtarget was pending. Multi-member and alliance navigation could not be exercised in this state.
- Native chat input was initially `0x11` with a nonempty draft. The SDK identifies `0x11` as chat input and `0x21` as another native text editor. Draft contents were not printed or saved in the report.

## Actual mapping data

The queried Windows mapping has **32 values**, while the published Ashita configuration page describes 27 action slots. Parse the documented prefix and retain unrecognized trailing fields; do not truncate or rewrite it using a fixed 27-field assumption.

The documented prefix matches familiar XInput assignments for this configuration:

- Confirm: A; Cancel: B; main menu: X; active window: Y.
- Left/right triggers: macro bars; LB: autorun; Back: window visibility; Start: logout.
- D-pad directions: menu/target navigation; RB: the screenshot action slot.

These are configuration observations, not proof that the currently disabled controller path uses them successfully. In-process registry virtualization and native-client interpretation still need to be checked when establishing the final effective map.

The installed official `addons/remap/remap.lua` reference also shows why IDs need translation: A is FFXI mapping value **3**, Ashita XInput button-event ID **12**, and Win32 button mask **0x1000**. Y is respectively **2**, **15** and **0x8000**. DirectInput event offsets are another namespace, for example button offsets beginning at 48 and a POV value. Do not send a `padsin000` value directly to `QueueButtonData`.

## Bounded injection attempt

AgentLink 0.7 itself has no controller-emulation verb. Its slash-command forwarding can reach the existing `/addon exec <name> <Lua string>` interface. Temporary, in-memory diagnostics were run in the already-loaded FPS addon's state; no addon files were created or edited.

Temporary `xinput_state` and `xinput_button` counters were registered. The button callback would block only an injected Y event belonging to this probe. A paired Y press/release was queued through `GetXInput():QueueButtonData(15, 1)` and `(15, 0)`. The observed counts remained **zero state callbacks and zero injected-button callbacks**. Calling the XInput export separately also reported 1167 and produced no counted state callback.

This is **not a passed input/ownership test**. The result does not isolate disabled enumeration, device absence, call-path interception or queue timing. It does not prove a virtual pad was created or that native UI received a tap. A future functional test needs an established device/emulation path, frame-separated input and observed press/release plus a native-state transition. A queued or successfully evaluated command alone proves none of those.

The session also changed during the probe: the character moved, XIUI was loaded, and native draft/caret values changed. The before/after draft comparison is consequently inconclusive. Do not restore an old draft over the user's newer work or attribute those changes to the unobserved controller event.

Both temporary event handlers and their research state were removed, and removal was confirmed. A neutral release was included in the attempted sequence. No chat submission, spell, movement command, native Confirm/Cancel or gamepad-policy change was issued by the research.

## New primary-source findings

### Windows has a native software keyboard

Square Enix's December 10, 2015 Windows update explicitly added it under **Config → Misc. 4 → Software Keyboard**, with Off and Full choices at introduction. This is an in-game keyboard, distinct from the PlayOnline password keyboard. Its presence on this particular client, current setting, input behavior and interaction with XIUI's hidden native windows remain unverified. [Official update](https://forum.square-enix.com/ffxi/threads/49425).

This changes the implementation preference: expose and reuse the native keyboard first. Establish its setting and client capability before considering a replacement; never silently change the user's choice or create a second simultaneous editor.

### Runtime readiness issue resolved

After the initial observations, Windows displayed a Bad Image error for the development runtime's temporary DLL. File inspection found that the installed `plugins/xiuinative-runtime.dll` was entirely zero-filled, while the built runtime had a valid PE header. Replacing the installed file with the built bytes and verifying exact equality restored loading. AgentLink then reported runtime **Oct 14 2026 00:03:11**, HUD visible and GPU text rendering active.

The standalone v0.2.5 distribution also matched its tested release build. The zero-filled development file explains the observed runtime-unavailable state; the cause of its corruption was not established. This repair changed the installed development DLL, not application source, release artifacts or controller settings. It does not turn the earlier controller injection into a successful test.

### Controller state uses a writable copy

The official addon example documents `xinput_state.state` as the original state and `state_modified` as writable. The button event separately exposes mutable button/state, blocking and an injected marker. Its examples also show that callbacks must act before yielding. This makes the event and state paths separate integration concerns. It is insufficient to assume a button-level block, or an undocumented field on the state event, handles all held input. [Official Ashita example](https://github.com/AshitaXI/example/blob/main/example.lua).

The installed Freecam addon assigns `e.blocked` on its state path, while the published example specifies the modified-state buffer. That source difference is a reason to verify installed behavior, not evidence that either approach already passed a runtime test here. The native C++ SDK is the implementation contract for XIUI.

## Consequences for the plan

1. Add a **readiness and diagnostics step** before any mapping setup: XIUI runtime usable, native gamepad setting, Ashita enumeration policy, selected backend/device, actual successful input, and native keyboard availability. Selecting Controller mode must not silently change these policies.
2. Prefer the native Windows keyboard, with an explicit ownership/visibility bridge. Keep custom XIUI typing as a fallback, rather than a parallel first delivery.
3. Normalize bindings using separate map/event/mask namespaces; retain unknown configuration fields and distinguish configured values from proven effective behavior.
4. Resolve focus from native state rather than visible window presence: `?ui menus` lists attached panels, and a generic menu-open flag can also be true during text entry. Neither establishes active chat-log focus.
5. Keep native party spell selection and deliberate XIUI browsing separate. Real multi-member/alliance tests remain necessary; a solo snapshot or synthetic focus test cannot validate them.
6. Split future validation into deterministic action/ownership checks, supported input-injection checks, and real-device/native-gameplay checks. Record a result as observed, inconclusive or pending rather than calling every queued probe a pass.

## Still pending

Native active-window identity; current native keyboard setting and opening path; real chat/tab and multi-member/alliance navigation; the installed C++ callbacks' ordering and suppression; held analog input; hotplug/background behavior; and coexistence with controller remappers. No new feature implementation is authorized by this investigation alone.

## October 17: live controller enablement

The running development client initially had enumeration disabled. `/gamepad disable 0` enabled it successfully. Windows reported the attached 8BitDo device as an Xbox 360 controller; all three tested Windows XInput libraries returned success for user 0.

The real 32-bit US FFXI registry contained enabled XInput settings and a 32-value button map. In-process `RegQueryValueExA` instead returned `ERROR_FILE_NOT_FOUND` for `padmode000`, `padsin000` and `padguid000` while the boot overrides were `-1`. Explicitly copying those existing Windows values through the configuration manager's `SetValue` made all three in-process reads succeed and updated `config/boot/phoenix-launch.ini`. The original override values were all `-1`; enumeration remains enabled and background input remains disabled. Reloading XIUI then reported XInput with resolved bindings.

Passive input counters still observed no DirectInput or XInput callbacks. The final interval contained 1,029 render callbacks and **zero foreground game frames**, so it cannot establish that foreground input fails or that a client restart is necessary. The next check must occur with the game focused. All temporary callbacks and diagnostic globals were removed. No controller input was injected or native game input initialization patched.
