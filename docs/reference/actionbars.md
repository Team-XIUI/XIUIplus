# Action Bars

Configuration → **Action Bars** has six destinations: Hotbars, Crossbar, Actions & Macros, Action Sets, Import and Global. Hotbars and Crossbar contain their layout and display settings; the other destinations open separate workspace windows. Hotbars shows direct buttons for Hotbar 1–10 above its settings, wrapping to fit the window. Each surface has its own enable flag, position, layout and assignment lock. Normal bars offer up to twelve slots; the controller crossbar has seven stored banks of eight slots, including a separate Shared expanded bank. Move bars through the existing configuration movement controls.

Action workspaces remain open when Configuration closes. The remaining workspace receives focus, preserving unsaved macro text; use its X button, Escape or controller Cancel to dismiss it.

Assigned actions and macros can run from clicks, configured shortcuts and the crossbar while the normal native Attack/command menu is open. Native shop/editor menus and active target selection remain guarded. `/xiuiplus actionbars status` includes the focused native window and current execution permission.

Controller trigger replacement applies live over the normal command menu, including during combat. The host and input queue verify the current focused native window before suppressing triggers or accepting crossbar actions, so a newly opened inventory or other native menu keeps its input even before the next HUD frame. An already-open native macro palette cannot block replacement behind its hidden buttons. After changing trigger settings, release held triggers and action buttons before using the crossbar again.

For delayed macros, the status command also reports current TP, the pending line/wait and the last queued timed line. If another window suspends execution, the cancellation reason is retained for diagnosis.

## Global settings and Lua compatibility

**Action Bars → Global** edits shared hotbar size, spacing, display, slot design, effects, overlays and text. Enable **Hotbars → Layout → Use Global Settings** for each bar that should follow it. Shapes, visible slot counts, positions, enabled states, locks, actions and keybindings remain independent. Turning the toggle off restores saved independent visuals, including after reload. Existing profiles and Lua imports remain independent until opted in; the crossbar keeps its own settings. Slot size accepts 16–72 px and spacing 0–32 px, covering Lua's smaller slots and wider spacing.

Checked against XIUI+ Lua 1.8.5: native already supports Global/main-job/main-job+subjob action storage, pet-aware assignments, palettes, ten hotbars, per-slot keybindings, label wrapping, quantity/charges, MP costs, text effects, overlay anchors and expanded/double/shared crossbar banks. **Global action storage** controls assignments; **Global visual settings** controls appearance.

Remaining differences: Lua's selected-bar stack with one group anchor and shared stack spacing is not implemented; native bars move independently. Lua's separate full-stack item count, custom slot-frame image selector, per-bar background themes/scales and separate background/border padding/opacity controls also remain absent. Native uses the shared game-window theme and container controls. Lua profile import converts effective per-bar visuals rather than restoring Global links.

## Assigning actions

The library has **Your actions** / **All actions** filters and search on one row, with category navigation on the left and actions in two columns. Search has a labeled placeholder and Ctrl+F shortcut. Your actions includes spells and abilities learned for the current jobs and owned items, including supplies such as Shihei. Item slots display their inventory quantity; activation still requires a usable item. The list uses the configured font metrics for row heights, clipping and scroll geometry; button labels use the same scale as Configuration. Scrolling moves complete rows, and directional navigation follows the two-column layout. Category headings and click/drag instructions are omitted from the content area. Mouse messages complete owned gestures; asynchronous button-state polling cannot cancel a queued category release. `/xiuiplus actionbars status` includes the active library category, filter and result count.

Click an empty slot or choose **Actions & macros** from the bar menu. Search spells, abilities, weapon skills, items, equipment, pet commands and the shared macro library. Click an action to assign it to the selected slot, or drag it from the library onto a visible slot. The library stays open after clicks, drags and controller placement so you can place more actions. New macro, Single command and Game macro remember the library while editing; Cancel or Save returns to the same search, category, scroll and selected slot.

Click an assigned slot to activate it. On a locked bar, hold Shift when clicking and drag to move or swap assignments; on an unlocked bar, click-drag normally. Shift also permits dropping onto locked bars, revealing hidden empty destination slots. Releasing Shift before the mouse still completes that drag. Unlocking shows empty assignment targets. Right-click a slot for Change action, Edit action, key binding and Clear slot. The detailed editor offers target placeholders, label/artwork overrides and macro lines. New macro, Command and Native macro are available directly from the library.

The number, lock and small menu replace the full bar header. Click the number or menu for bar options; right-click a hotbar number to cycle 12×1, 6×2, 4×3, 3×4, 2×6 and 1×12 shapes. Layout also offers Custom. The optional action set caption sits below the icons. Counts, charges, MP costs and remaining recast time appear on icons. The first presentation upgrade removes the old default container background while preserving positions, assignments, bindings and custom opacity/padding.

Each hotbar and the crossbar has its own **Slots**, **Effects** and **Overlays** pages. Slots offers beveled, flat, outline and icon-only frames; corner radius, border thickness, artwork padding, opacity, colors, shadow and top highlight. Effects controls unavailable/inactive artwork, pointer/press highlights, activation flash and cooldown shading. Recast shading can use a clock sweep or vertical fill, and the countdown can remain visible with shading disabled. Overlays places key labels, quantities, MP costs and recast text at nine anchors with independent X/Y offsets. Apply design to all bars copies appearance only; Reset design restores only the selected bar's slot appearance.

**Text** has separate label, key, countdown, quantity and MP-cost profiles using shared font, gradient, stroke, shadow and glow controls. Slot labels and keys match the MP cost typography, with white text, a black stroke and shadow; MP costs use the MP bar's green. Versioned migrations update the previous standard styles while preserving custom colors and text positions. Macro/command slots read display metadata from their first spell, ability, weapon skill, pet or item command, so their MP cost and recast follow that action. This display lookup does not change how the macro executes.

Actions & Macros, Action Sets and Import open compact, movable, resizable windows using the shared native container, configuration chrome, resize geometry and cursor. Each workspace remembers its bounds independently of the hotbars. Drag the title bar or resize from an edge or the bottom-right grip. Clicking Configuration brings it forward and preserves the workspace; closing Configuration leaves the workspace open. A library drag keeps its pointer through release and assigns only an exposed hotbar slot. Clicking away commits text to the draft and releases typing focus without closing the window. Tab starts deliberate keyboard navigation; controller focus includes Move, Resize and Close controls, with Confirm to accept and Cancel to undo a move/resize.

Management remains available during edit mode. Dragging a locked slot without Shift positions the bar; Shift-drag rearranges its slots. Unlocked slots allow ordinary assignment dragging. Right-click opens slot options. Spell execution stays suspended while editing. Missing or suspended controller input clears trigger state without closing mouse/keyboard management. Trigger-bank selectors appear only on the Crossbar layout tab.

Keyboard, pointer and controller activation use the same executor. It checks the live character, learned action, job/level, resources, cooldowns and native input ownership before dispatch. Tooltips explain unavailable actions. Range and the final target remain the game's decision. Holding an activation button never repeats its action.

**Replace native macros** is off by default, keeping native macro controls available. When enabled, XIUI+ hides the built-in macro palettes and blocks native macro execution. Ctrl/Alt + numbers belong to XIUI+, including unassigned, empty and hidden slots; only visible assigned XIUI+ slots activate an action. Native controller macro triggers are suppressed even when the crossbar is disabled. Turn this preference off to restore native macro controls. Saved choices are preserved. Native chat, ordinary menus and subtarget selection retain input ownership.

The game's macro palette is recognized separately from other native menus. A native execution guard blocks palette confirmation as well as keyboard/trigger starts, while explicit XIUI-macro actions use the guarded host bridge to run their selected book/set. Keyboard and controller state, buffered input and owned releases are handled together so quick taps or a preference change cannot replay a native macro activation.

Keyboard bindings follow the game's keyboard mode. Compact layouts retain ordinary letter/number bindings. Full mode reserves unfocused letters, digits, punctuation, Space and Shift-only text for chat; use Ctrl/Alt combinations or function keys for global actions. Explicit editor/binding-capture focus still receives typing. A held action key drains through release if the keyboard mode changes.

## Custom icon files

Right-click an assigned hotbar or crossbar slot and choose **Change icon...**, or open **Edit action... → Change**. The same picker works for ordinary spells, abilities, items and macros. Choose **Custom → Open folder** and add PNG files to `config/XIUIplus/icons/` in the running installation. Subfolders appear as searchable categories. Return to the game and choose **Refresh**, select the image, then **Save** in the editor. Controller browsing uses the same controls; Confirm selects and Cancel returns without saving.

Transparent and indexed PNGs are supported. Non-square images keep their aspect ratio with transparent padding. Source images may be up to 2048 × 2048 pixels and 8 MB; the catalog supports 4096 PNGs. Invalid images show a loading error when selected. **Automatic (from action)** restores the original artwork after Save. Changing artwork preserves the action, command lines, target and recast settings.

Custom image references are relative to the icons folder, including subfolders. When sharing profiles, copy the matching PNG files and folder structure to the other installation's icons folder too; profiles do not embed image files. Refresh updates thumbnails and assigned images without a plugin reload. Missing images keep their saved reference so adding the file and refreshing restores them.

## Macros and palettes

Macros are shared library entries. Click a saved macro in Actions & Macros to edit it; dragging it to a hotbar still assigns it. Controller Confirm starts placement and Actions opens the editor. Saving or canceling returns to the same library view. Editing one updates every slot referencing it; duplicate it to make an independent variant. Deleting an entry leaves assigned slots with their commands. Multiline commands support `/wait` and `<wait seconds>`; macros wait for their own native subtarget selection before continuing. Opening an editor, changing character/job/zone, losing foreground or hiding XIUI+ cancels pending sequences. `/xiuiplus macros cancel` also cancels a sequence.

Create/Edit Macro follows the Lua layout: **Action Type**, a large icon preview and **Change** button at the upper right, **Macro Commands**, **Recast Source (Optional)**, **Slot Label**, then **Save / Cancel**. More options contains the native library, binding and availability controls. Command inputs have a dark backing and bright text. Eight lines appear when space permits; paging reaches all 32 supported lines. Enter and Up/Down move between rows, and multiline paste preserves command order without changing the saved action until Save.

Use **Paste** beside **Slot Label** to replace it with clipboard text, including Japanese names. Ctrl+V also works while editing text fields. Labels remain unchanged when saved and shared. Actions, optional recast sources and automatic icons recognize both English and Japanese resource names, such as `Cure` and `ケアル`. Recognized macro action arguments use the client's default resource name when executed; the editor keeps the original command text, targets and waits.

**Change** opens a searchable picker for **Game**, original **XIUI+**, and **Vincerno** artwork. Game entries include spell/ability recast icons and native item/equipment images; XIUI+ includes the default weapon-skill artwork. Vincerno retains its 220 custom variants. Search by action/item name, job, magic type or variant; Ctrl+F focuses search. Mouse, keyboard and controller share selection and scrolling. **Automatic (from action)** clears the override and infers artwork from the macro's main action: for Boost → wait → Combo, it selects Combo. Explicit choices take priority. Back/Cancel preserves the draft; Save applies it. Icon keys remain portable without extra installation.

**New macro** creates a sequence of commands and waits, such as Boost → wait → Combo. **Single command** creates one command shortcut. **Game macro** (formerly Native macro) runs an existing FFXI macro by book, set, Ctrl/Alt row and slot. It does not copy the game's macro book into XIUI+. The bridge checks the installed client signatures and reports unavailable execution if they do not match. The create buttons include hover/controller help.

Click the action set caption to choose a set; the bar menu opens management. Create, rename, copy, reorder or delete sets there. A hotbar set contains all ten hotbars; switching it changes their assignments together. Crossbar sets are independent. Storage can be global, shared across a main job's subjobs, or specific to a main/subjob pair. Missing job/subjob data falls back to shared-main-job and then global data. Changing storage scope copies the current layout when creating a new destination context; existing destination palettes are preserved.

**Pet-aware assignments** selects matching overrides when a pet is present. The editor can save an override for the current pet; the base assignment remains the fallback.

## Keybinds

Select **Keybinds** in the left sidebar: directly below **Actions** for Hotbars and below **Controller Setup** for Crossbar. The sidebar item opens the list directly. Bar menus, `/xiuiplus keybinds`, `/xiuiplus hotbar keybinds` and `/xiuiplus crossbar keybinds` open the same independent, movable/resizable window. Its Bar dropdown lists all ten hotbars and Crossbar, including empty slots. Each row shows the slot/action and current binding; Clear removes the binding without removing the action.

For a hotbar, select its binding and press a key combination. Ctrl, Alt and Shift are supported. Escape cancels, and an existing conflicting assignment is reported without replacing either slot. Bindings stay attached to bar slots when action sets change. Full keyboard mode still reserves ordinary unfocused typing for chat.

For Crossbar, choose a trigger bank to see its actions, select XInput or DirectInput, then choose the button for a slot using the shared mapping dropdown. Auto follows the game mapping; Clear uses Unbound. Physical slot mappings apply to every trigger bank and use the same device settings as Controller Setup. Hardware mappings remain local when sharing profiles.

Mouse, keyboard and controller share the list, dropdowns and Clear actions. Directional navigation scrolls rows into view; Confirm opens a binding and Cancel dismisses a dropdown/capture or leaves the window. Keyboard capture consumes its key press/release and never executes the slot. Existing gameplay execution remains paused while the workspace is open.

## Controller Setup

Choose Crossbar → **Controller Setup** → **Controller Setup**, or run `/xiuiplus crossbar setup`. General is the first Crossbar topic, followed by Controller Setup. General, Mappings and Timing tabs keep everyday setup separate from raw device adjustments. Auto follows the effective FFXI mappings; Xbox, PlayStation, Nintendo and custom choices are also available. Physical mapping and replacement preferences stay on this computer when profiles are shared. Setup and Configuration pause gameplay trigger actions; close both to use the crossbar.

Mappings, input type and presets open the shared dropdown menus. Timing and trigger thresholds use shared sliders with click-to-type numbers; timing values are in milliseconds. Controller Confirm enters slider adjustment, Left/Right changes its value and Cancel leaves adjustment. Actions opens numeric entry. Release always remains below Press. Labels and controls share a compact value column.

Dropdowns use the same native panel and list styling as Input mode. Long lists include the shared scrollbar with thumb dragging, page clicks and wheel scrolling; controller selection stays visible. Setup omits explanatory text beneath the form and defaults to a shorter window. Existing custom window sizes remain saved.

Turn on **Use XIUI+ trigger controls** to enable the crossbar and route trigger actions to XIUI+. Turning it off leaves physical trigger macros to FFXI while the bar remains available for editing and mouse use. Native replacement removes the selected trigger/button inputs before FFXI processes them; it does not disable macro commands or native macro reference actions.

The stored banks preserve the Lua model:

- Left trigger and right trigger each have a base bank.
- Left then right and right then left select separate combined banks when ordered banks are enabled.
- Double left and double right select extra banks when double taps are enabled.
- **Shared expanded bank** routes both two-trigger orders to its own assignments while retaining the two separate ordered banks. Select Shared expanded in Edited bank to configure it.
- Each bank has D-pad Up/Right/Down/Left and Face Top/Right/Bottom/Left slots.
- Hold a bank and the selected shoulder, then press D-pad Up/Down to cycle palettes.

The two trigger halves stay in place. Each half contains separate D-pad and face-button diamonds with non-overlapping icons. Hold a trigger to select its side, then hold the other trigger to reveal the bank behind that same side: LT then RT expands the left half; RT then LT expands the right. Release the second trigger to return to the first side's base bank. Shared expanded mode follows the same side selection using its shared assignments. Double taps replace the matching half. Expanded and double-tap banks can be previewed from settings without activating an action.

The crossbar uses XIUI+ Lua's original controller artwork, with larger center button clusters and trigger badges. Prompts follow the effective button mappings, including custom face, direction and shoulder assignments. Auto shows Xbox artwork for XInput and its disconnected preview; choose PlayStation or Nintendo explicitly for DirectInput. Unidentified DirectInput buttons retain numbered labels. The center artwork scales to fit smaller slots.

Trigger badges, center prompts and action artwork have the Lua press/release pop. Changed banks fade and slide upward over 500 ms, with rapid changes continuing from the current picture. Slot frames and hit areas stay fixed, and actions use the newly selected bank immediately. **Crossbar → Controller Setup → Crossbar animations** disables motion; **Controller prompts** and **Button prompts** control the artwork.

XInput uses separate trigger press/release thresholds to avoid flicker. DirectInput uses mapped digital trigger buttons and POV directions; axis-only DirectInput trigger layouts need an appropriate remapper or XInput mode. Movement sticks and camera axes remain available during ordinary crossbar activation. Native chat, menus and targeting suspend the crossbar. Previously held action buttons require neutral across bank transitions; a fresh trigger and action arriving in the same state packet can activate. Palette changes, device replacement and reloads drain held controls before further activation.

For deliberate browsing, use `/xiuiplus focus hotbar` or `/xiuiplus focus crossbar`, or select the bar from XIUI+'s focus picker. Directions select slots spatially, Confirm activates assignments or opens the library for an empty slot, Actions opens slot options, tabs change action sets, and Cancel releases focus. In the library, choose an action, select its destination with directions, use tabs to change bar/bank, and Confirm to place it. Cancel returns to the library with its query preserved. Controller browsing and keyboard typing are supported in the editor; controller-only text entry remains subject to the existing XIUI+ text-input limitations.

## Importing

**Import Lua / tHotBar / tCrossBar** reads a local saved data file, displays a preview and warnings, then creates new palettes after Import is selected. Existing layouts are preserved. Supported sources include XIUI+ saved profiles using the actual assignment serializer, returned Lua tables, tHotBar/tCrossBar assignments and native INI action data. This imports actions and shared library records; full settings/placement imports are available separately through [Import profile → XIUI+ Lua](lua-profile-import.md). Expressions and executable Lua are rejected. Unsupported source features are reported rather than executed.

The reusable library accepts typed actions as well as command macros, including unused entries from Lua. Right-click a shared library entry, or focus it and press controller Actions / keyboard Menu, to edit it without assigning a slot. Shared edits update references; deleting a library entry keeps its currently assigned actions usable.

Macro editing offers Automatic, None or a selected spell/ability/weapon-skill/pet/item cooldown source, plus Always available. These affect displayed cooldown/availability metadata; commands retain their normal guarded execution path.

Hotbar and crossbar sets have independent names, selection and ordering. Existing combined sets split on load while retaining assignments. Text supports independent per-bar profiles and two-line labels; Layout separates horizontal/vertical gaps and crossbar diamond/group spacing. Crossbar visibility can follow held triggers or combat, with a release delay and fade. Existing settings default to their prior behavior.

Action data, macro commands and presentation settings follow normal XIUI+ profiles. Physical controller mappings, held input, browsing focus and pending macro execution do not travel with a share key. Imports do not apply a source addon's controller/device configuration.

## Commands and validation

- `/xiuiplus hotbar` or `/xiuiplus crossbar`: open configuration for that surface.
- `/xiuiplus hotbar actions` (or `edit`), `/xiuiplus crossbar actions`, `/xiuiplus macros`: open the action library.
- `/xiuiplus hotbar palettes`, `/xiuiplus crossbar palettes`: manage palettes.
- `/xiuiplus hotbar next` / `previous`, `/xiuiplus crossbar next` / `previous`: cycle palettes.
- `/xiuiplus hotbar import`: preview an import.
- `/xiuiplus actionbars status`: report palette/macro counts and native bridge status.

Focused software checks cover storage, macro references, import boundaries, input normalization and UI ownership. Native builds and AgentLink status confirm the installed integration. Physical controller feel, remappers, visual polish and live action execution remain for Cody's gameplay acceptance; the implementation checks do not send live actions.

ABI 18 adds the host-owned native macro runner and independent native-trigger suppression. Native macro replacement defaults off; using the XIUI+ crossbar remains a separate setup choice. Updating this contract requires a matching development host/runtime pair.

ABI 22 carries the transient verified native focus slot with crossbar routing settings so controller callbacks can check live command/palette ownership without taking the runtime mutex. It is excluded from saved settings and portable profiles.

The October 16 input revision passed focused action-data, UI, native-HUD and keyboard-suppression checks. The host/runtime pair was installed with SHA-256 verification in `C:/Games/PhoenixXI-dev/PhoenixXI`. AgentLink confirmed both builds `23:48:49` active, HUD visible, twelve macros retained and the native macro bridge available. The previous pair and settings/profiles are backed up under `out/actionbars-input-install-20261016-234916`. Physical keyboard ordering, visual behavior and controller feel remain in-game acceptance checks.

The compact workspace revision passed `xiui_actionbars_ui_smoke`, including independent bounds, controller move/resize cancellation, typing blur, Configuration coexistence and exposed/occluded library drops. The runtime was installed with SHA-256 verification and compatible hot reload; AgentLink confirmed runtime `23:59:00`, edit mode retained and twelve macros present. Previous runtime/settings are backed up under `out/actionbars-workspaces-install-20261016-235950`.

The October 17 library refinement passed focused checks for every category's mouse press/release with 220 seeded results, unique control IDs, compact layouts at 8/14/18 px and 0/4 px spacing, filter selection, Ctrl+F and Escape release. Runtime `Oct 17 00:09:28` was hot-reloaded with settings and twelve macros preserved; an AgentLink capture confirmed readable categories, search placeholder, consistent button text and removal of duplicate/instruction rows. Previous runtime/settings: `out/actionbars-library-ui-install-20261017-000944`.

## FFXIV references

The interaction revision follows Square Enix's [action assignment flow](https://na.finalfantasyxiv.com/uiguide/know/know-xhb/xhb_how_to.html): choose an action, then place it on a bar. Mouse assignment follows [drag-and-drop registration](https://na.finalfantasyxiv.com/uiguide/know/know-hb/hotbar_registration.html); the six shapes follow the [hotbar layout options](https://na.finalfantasyxiv.com/uiguide/know/know-hb/hotbar_shape.html). Trigger halves and ordered expanded holds follow the [expanded crossbar guide](https://na.finalfantasyxiv.com/uiguide/know/know-xhb/xhb_hold.html), with optional [double-tap banks](https://na.finalfantasyxiv.com/uiguide/know/know-xhb/wxhb_how_to.html). XIUI+ retains its FFXI-specific action validation, native input ownership and existing data format.

## Skillchains and Magic Burst

**Hotbars / Crossbar → Skillchains** contains the shared Action predictions switch and Retail/Horizon rules for every bar. Compatible closers receive skillchain hints; matching offensive spells receive MB hints. **Open HUD settings** opens **Extras → Magic Burst** for the independent display’s visibility, artwork, animation and text. `/xiuiplus skillchain` opens the prediction section; `/xiuiplus magicburst` opens the display editor. Existing prediction/rules choices and HUD positions carry over.
