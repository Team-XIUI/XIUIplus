# Action Bars and crossbar

Hotbars, controller crossbar, reusable macros, palettes and their management windows share one action store and executor.

**Start in:** [actionbars_module.hpp](../../src/runtime/modules/actionbars/actionbars_module.hpp), [actionbars_editor.cpp](../../src/runtime/modules/actionbars/actionbars_editor.cpp), [actionbars_display.cpp](../../src/runtime/modules/actionbars/actionbars_display.cpp), [hud_runtime_actionbars.cpp](../../src/runtime/hud/hud_runtime_actionbars.cpp).

Create/Edit Macro follows the Lua editor's order: action type, upper-right artwork preview, command block, optional recast source, slot label and Save/Cancel. Native library, availability and binding controls live under More options. Eight command rows appear when space permits; every page, added row and keyboard transition uses the same visible-row count. Multiline paste is bounded and atomic. Editor inputs use the shared text control with a dark fill, bright foreground and 14-pixel minimum.

Slot Label offers an explicit Unicode Paste action through the shared editor and controller controls. Resource lookups accept English and Japanese names for actions, recasts and inferred icons. Resolve recognized command arguments to the client's default resource name only at dispatch; preserve saved commands and custom labels. `xiui_actionbars_data_smoke --japanese-names-only` covers language IDs, command arguments and game encoding.

The icon picker groups Game, original XIUI and Vincerno artwork with cached name/category searches. Game spell/ability recast artwork and item bitmaps use the existing icon cache; large grids bound their native item working set. Automatic inference is separate from recast inference: prefer a weapon skill over preparation such as Boost, retain explicit artwork and preserve saved icon keys. `--macro-editor-only` covers geometry, rows, paste, shared dropdowns, icon sources and draft persistence; the action-data check covers icon priorities.

**Custom** adds user PNG, JPEG, BMP, GIF, TIFF, ICO and static WebP images from `config/XIUIplus/icons/`, with Open folder and explicit Refresh. Slot menus expose Change icon for ordinary actions too. `resources/custom_icon_library.*` owns relative keys, bounded background scans/decoding and refresh generations; `resources/wic_image.*` shares WIC decoding with minimaps. WebP uses bundled libwebp; GIF/TIFF use the first frame/page. `game_icon_cache` uploads and evicts custom textures only before HUD drawing, with a 512-texture limit. Preserve filename/category text and saved assignments across scans. `xiui_custom_icon_smoke` covers files, alpha, paths and worker lifetime; `xiui_actionbars_ui_smoke --custom-icons-only` covers the picker, controller actions and persistence.

Import reads XIUI, tHotBar and tCrossBar actions, shared macros and hotbar/crossbar palettes only. `action_import.*` owns the restricted data parser and atomic action-store commit; HUD settings, positions and controller configuration are outside this boundary. Native profile-file sharing remains separate. Focused check: `./scripts/check.ps1 -Name actions-import`.

Under **Main job + subjob** storage a subjob uses the job's shared palettes (subjob 0) until the palette window's **Separate for this subjob** copies them; **Share with other subjobs** returns to the shared set, or promotes the subjob's palettes when the job has none. New palettes join whichever set is in use, so one job can mix shared and separate subjobs as in the Lua addon. `xiui_actionbars_data_smoke` covers the fallback and both actions.

Global visuals are opt-in per hotbar. Preserve independent shapes, assignments, keys and positions; unlinking restores saved local visuals. The detailed guide records remaining Lua compatibility gaps.

**Keep intact:** Keep native macro suppression separate from crossbar activation. All execution uses the validated executor. Pointer management can work over native menus; unclaimed keyboard/controller navigation remains native. Shared library edits must not silently assign slots.

Explicit number-row bindings work with Full, Compact and temporarily unavailable native keyboard settings, including after zoning. Unbound digits still pass through for chat/native input. A fresh key press retires stale ownership from a missed release; held repeats remain consumed without activating again. Focused check: `xiui_actionbars_ui_smoke --input-ownership-only`.

Verified native Attack/command menus permit assigned action-bar shortcuts, clicks and crossbar actions. Other native menus and active action/subtarget selection retain their guards. Refresh the executor's transient menu permission at every input path and scheduler update.

Controller callbacks and the mailbox re-read the verified native focus slot before allowing that menu exception. Cached command-menu permission cannot capture a newly focused inventory/menu. The same check permits native trigger suppression and handles an already-open replacement macro palette; no game restart is required to apply the setting. `xiui_crossbar_input_smoke` covers both controller backends and live permission changes.

An active native macro palette can sit above a still-visible command menu. Identify ownership from the verified focused palette, not the presence of a background menu; holding Ctrl/Alt must not cancel delayed macro lines.

`shared/native_target.hpp` identifies actual target selection from an explicit subtarget or pending native action/cancel callbacks. Action flags, types and IDs can remain after dispatch, including a zero ID from ranged attacks; they do not establish input ownership. Host/controller input, action execution and macro waits share this check so completed targeted macros do not disable later hotbar input. Real selectors remain guarded even for zero-ID actions. `xiui_actionbars_data_smoke` covers retained flags/IDs, callback completion and native selection ownership.

`/xiuiplus actionbars status` reports current TP, pending macro line/wait and the last queued timed line. Input suspension records its cancellation reason instead of silently discarding the remaining sequence.

Running macros retain their current line and wait deadline while native menus, events, maps or target selection own input, including the item-trade menu before a mining animation. They resume when native input clears. New activations remain guarded; foreground, editor and character/zone/job restrictions still cancel the sequence. `/xiuiplus actionbars run <bar 1-10> <slot 1-12>` activates a hotbar assignment through the same validated executor for command-based use and automated tests.

`/xiuiplus actionbars macrotest` runs an unsaved scratch macro for automated testing. `set /echo a <wait3.6> | /echo b` replaces it in one command, splitting on `|` before each `/`. Commands queued in the same frame can reorder, so separate `add <line>` calls need a `wait` between them. `run` uses the same executor and guards as a hotbar macro, but that run continues while the game is unfocused, so tests never need to take focus. `report` lists each dispatched line's offset and gap for the latest macro run from any source, with how it ended. `show` and `clear` manage the lines.

Inline waits are a line-ending `<wait N>`, `<waitN>` or bare `<wait>` (1 second). Other `<wait…>` tokens remain command text.

Timed and untimed command macros use the same native macro queue mode for every line, including `/targetnpc` and other non-action commands. Parsing waits must not change the dispatch origin.

Replace native macros defaults off in code, resets and bundled setups. Preserve explicit saved choices.

Native macro replacement and **Instant native macro bar** are grouped under Native UI → HUD Windows → Native Macro Bar. Instant mode changes the native Ctrl/Alt palette delay and preserves command waits and action cooldowns. It remains independent of hotbar visibility and pauses while Replace native macros is enabled; see [General](general.md).

The Action Bars sidebar page follows the enabled state of all ten hotbars and the crossbar. Hide it only when all bars are disabled; global visual settings do not count as an enabled bar.

Closing Configuration hands focus to an open action workspace and retains its unsaved draft. Keep its close button, Escape and controller Cancel usable after that handoff.

Opening Configuration must release shared hotbar hover capture before saving its external mouse-block baseline. `--input-ownership-only` covers this transition, an existing external block and plain config clicks after a missed release. Use `/xiuiplus mouse status` to distinguish config clicks from workspace capture in user reports.

Workspace close buttons activate on release and cancel outside the button. Popup dismissal retains mouse blocking and held movement capture through its owned release.

Locked bars require Shift when pressing to rearrange slots; unlocked bars allow ordinary click-drag. Shift reads both mouse modifier flags and keyboard state, including in Movement mode. The gesture temporarily bypasses destination locks, reveals hidden empty slots and retains that permission through release, preserving saved locks and slot keybinds. Library entries still drag directly onto a bar. Cancelled clicks drain their release in both the runtime and module; fresh presses replace any stale release marker. Native chat cleanup preserves ordinary hotbar hover capture so successive macro clicks need no Escape.

Actions & Macros is nonmodal to exposed hotbars: slot drags, bar locks and slot/bar menus remain usable with the library open. The library keeps its search, category, scroll, assignment context and draft behind child menus; dismissing a menu restores it. Window bounds take precedence over covered bars, and an active drag retains its press/release ownership. Existing slot actions and controller Cancel are shared with the child menus. Focused check: `xiui_actionbars_ui_smoke --library-bars-only`.

Clicking a saved macro in Actions & Macros opens its editor without assigning or executing it. Dragging the entry still assigns it to a hotbar slot; controller Confirm retains destination placement and Actions opens the editor. Save, Cancel, Escape and deletion return to the existing library search, category, scroll and slot context. New macro, Single command and Game macro also remember that context while their independent editor is open. Save refreshes results immediately.

Saving a catalog action under its original name replaces that action's default library result with the saved definition. Reopening, dragging and controller placement retain its target and other edits instead of creating another default action. Explicitly named variants remain separate, and catalog resources are unchanged.

The standalone library has no assignment destination. Clicking a catalog action or saving a new library action opens/saves its shared definition without changing hotbar 1, slot 1. An explicit empty-slot picker retains its chosen destination; dragging and controller placement remain deliberate assignment paths. Preserve this distinction when returning from child editors or slot menus.

`ReleaseChatInput` must preserve action-workspace pointer capture while native chat polls the command menu. The input-ownership fixture places that poll between drag/button press and release, during and after combat, and checks suspension cleanup. Create labels distinguish multi-line New macro, Single command and an existing FFXI Game macro; old imported type names remain accepted.

Expanded banks replace the first-held trigger's half: LT then RT expands left, RT then LT expands right. Shared expanded assignments follow the same side selection. Keep drawing, prompts, animation layers and pointer bounds aligned without remapping saved banks.

**Focused check:** `xiui_actionbars_ui_smoke --input-ownership-only`; crossbar layout uses `--crossbar-layout-only`, normalization uses `xiui_crossbar_input_smoke`.

## Behavior

Configuration → **Action Bars** has six destinations: Hotbars, Crossbar, Actions & Macros, Action Sets, Import and Global. Hotbars and Crossbar contain their layout and display settings; the other destinations open separate workspace windows. Hotbars shows direct buttons for Hotbar 1–10 above its settings, wrapping to fit the window. Each surface has its own enable flag, position, layout and assignment lock. Normal bars offer up to twelve slots; the controller crossbar has seven stored banks of eight slots, including a separate Shared expanded bank. Move bars through the existing configuration movement controls.

Action workspaces remain open when Configuration closes. The remaining workspace receives focus, preserving unsaved macro text; use its X button, Escape or controller Cancel to dismiss it.

Assigned actions and macros can run from clicks, configured shortcuts and the crossbar while the normal native Attack/command menu is open. Native shop/editor menus and active target selection remain guarded. `/xiuiplus actionbars status` includes the focused native window and current execution permission, and the context of the last key press a hotbar binding took (key, native keyboard mode, input language, whether XIUI+ or native chat was typing), for reports of bindings firing while typing.

Controller trigger replacement applies live over the normal command menu, including during combat. The host and input queue verify the current focused native window before suppressing triggers or accepting crossbar actions, so a newly opened inventory or other native menu keeps its input even before the next HUD frame. An already-open native macro palette cannot block replacement behind its hidden buttons. After changing trigger settings, release held triggers and action buttons before using the crossbar again.

For delayed macros, the status command also reports current TP, the pending line/wait and the last queued timed line. If another window suspends execution, the cancellation reason is retained for diagnosis.

### Global settings and Lua compatibility

**Action Bars → Global** edits shared hotbar size, spacing, display, slot design, effects, overlays and text. Enable **Hotbars → Layout → Use Global Settings** for each bar that should follow it. Shapes, visible slot counts, positions, enabled states, locks, actions and keybindings remain independent. Turning the toggle off restores saved independent visuals, including after reload. Existing profiles and imported palettes remain independent until opted in; the crossbar keeps its own settings. Slot size accepts 16–72 px and spacing 0–32 px, covering Lua's smaller slots and wider spacing.

Compared with XIUI+ Lua 1.8.5, native supports Global/main-job/main-job+subjob action storage, pet-aware assignments, palettes, ten hotbars, per-slot keybindings, label wrapping, quantity/charges, MP costs, text effects, overlay anchors and expanded/double/shared crossbar banks. **Global action storage** controls assignments; **Global visual settings** controls appearance.

Remaining differences: Lua's selected-bar stack with one group anchor and shared stack spacing is not implemented; native bars move independently. Lua's separate full-stack item count, custom slot-frame image selector, per-bar background themes/scales and separate background/border padding/opacity controls are also not implemented. Native uses the shared game-window theme and container controls. Legacy action imports leave current visuals and Global links intact.

### Assigning actions

The library has **Your actions** / **All actions** filters and search on one row, with category navigation on the left and actions in two columns. Search has a labeled placeholder and Ctrl+F shortcut. Your actions includes spells and abilities learned for the current jobs and owned items, including supplies such as Shihei. Item slots display their inventory quantity; activation still requires a usable item. The list uses the configured font metrics for row heights, clipping and scroll geometry; button labels use the same scale as Configuration. Scrolling moves complete rows, and directional navigation follows the two-column layout. Category headings and click/drag instructions are omitted from the content area. Mouse messages complete owned gestures; asynchronous button-state polling cannot cancel a queued category release. `/xiuiplus actionbars status` includes the active library category, filter and result count.

Click an empty slot or choose **Actions & macros** from the bar menu. Search spells, abilities, weapon skills, items, equipment, pet commands and the shared macro library. Click an action to assign it to the selected slot, or drag it from the library onto a visible slot. The library stays open after clicks, drags and controller placement so you can place more actions. New macro, Single command and Game macro remember the library while editing; Cancel or Save returns to the same search, category, scroll and selected slot.

Click an assigned slot to activate it. On a locked bar, hold Shift when clicking and drag to move or swap assignments; on an unlocked bar, click-drag normally. Shift also permits dropping onto locked bars, revealing hidden empty destination slots. Releasing Shift before the mouse still completes that drag. Unlocking shows empty assignment targets. Right-click a slot for Change action, Edit action, key binding and Clear slot. The detailed editor offers target placeholders, label/artwork overrides and macro lines. New macro, Single command and Game macro are available directly from the library.

Each bar's header is a number, a lock and a small menu. Click the number or menu for bar options; right-click a hotbar number to cycle 12×1, 6×2, 4×3, 3×4, 2×6 and 1×12 shapes. Layout also offers Custom. The optional action set caption sits below the icons. Counts, charges, MP costs and remaining recast time appear on icons.

Each hotbar and the crossbar has its own **Slots**, **Effects** and **Overlays** pages. Slots offers beveled, flat, outline and icon-only frames; corner radius, border thickness, artwork padding, opacity, colors, shadow and top highlight. Effects controls unavailable/inactive artwork, pointer/press highlights, activation flash and cooldown shading. Recast shading can use a clock sweep or vertical fill, and the countdown can remain visible with shading disabled. Overlays places key labels, quantities, MP costs and recast text at nine anchors with independent X/Y offsets. Apply design to all bars copies appearance only; Reset design restores only the selected bar's slot appearance.

**Text** has separate label, key, countdown, quantity and MP-cost profiles using shared font, gradient, stroke, shadow and glow controls. Slot labels and keys match the MP cost typography, with white text, a black stroke and shadow; MP costs use the MP bar's green. Settings migrations update older standard styles while preserving custom colors and text positions. Macro/command slots read display metadata from their first spell, ability, weapon skill, pet or item command, so their MP cost and recast follow that action. This display lookup does not change how the macro executes.

Actions & Macros, Action Sets and Import open compact, movable, resizable windows using the shared native container, configuration chrome, resize geometry and cursor. Each workspace remembers its bounds independently of the hotbars. Drag the title bar or resize from an edge or the bottom-right grip. Clicking Configuration brings it forward and preserves the workspace; closing Configuration leaves the workspace open. A library drag keeps its pointer through release and assigns only an exposed hotbar slot. Clicking away commits text to the draft and releases typing focus without closing the window. Tab starts deliberate keyboard navigation; controller focus includes Move, Resize and Close controls, with Confirm to accept and Cancel to undo a move/resize.

Management remains available during edit mode. Dragging a locked slot without Shift positions the bar; Shift-drag rearranges its slots. Unlocked slots allow ordinary assignment dragging. Right-click opens slot options. Spell execution stays suspended while editing. Missing or suspended controller input clears trigger state without closing mouse/keyboard management. Trigger-bank selectors appear only on the Crossbar layout tab.

Keyboard, pointer and controller activation use the same executor. It checks the live character, learned action, job/level, resources, cooldowns and native input ownership before dispatch. Tooltips explain unavailable actions. Range and the final target remain the game's decision. Holding an activation button never repeats its action.

**Replace native macros** is off by default, keeping native macro controls available. When enabled, XIUI+ hides the built-in macro palettes and blocks native macro execution. Ctrl/Alt + numbers belong to XIUI+, including unassigned, empty and hidden slots; only visible assigned XIUI+ slots activate an action. Native controller macro triggers are suppressed even when the crossbar is disabled. Turn this preference off to restore native macro controls. Saved choices are preserved. Native chat, ordinary menus and subtarget selection retain input ownership.

The game's macro palette is recognized separately from other native menus. A native execution guard blocks palette confirmation as well as keyboard/trigger starts, while explicit XIUI-macro actions use the guarded host bridge to run their selected book/set. Keyboard and controller state, buffered input and owned releases are handled together so quick taps or a preference change cannot replay a native macro activation.

Keyboard bindings follow the game's keyboard mode. Compact layouts retain ordinary letter/number bindings. Explicit number-row bindings work in every layout, including after zoning. Full mode reserves unbound digits, unfocused letters, punctuation, Space and Shift-only text for chat; Ctrl/Alt combinations and function keys remain available for global actions. Explicit editor/binding-capture focus still receives typing. A held action key drains through release if the keyboard mode changes.

### Custom icon files

Right-click an assigned hotbar or crossbar slot and choose **Change icon...**, or open **Edit action... → Change**. The same picker works for ordinary spells, abilities, items and macros. Choose **Custom → Open folder** and add PNG, JPEG, BMP, GIF, TIFF, ICO or static WebP files to `config/XIUIplus/icons/` in the running installation. Subfolders appear as searchable categories. Return to the game and choose **Refresh**, select the image, then **Save** in the editor. Controller browsing uses the same controls; Confirm selects and Cancel returns without saving.

Transparency and indexed colors are supported. GIFs and multi-page TIFFs use the first frame/page; icons do not animate. WebP uses the bundled decoder and needs no Windows codec extension. Non-square images keep their aspect ratio with transparent padding. Source images may be up to 2048 × 2048 pixels and 8 MB; the catalog supports 4096 images. Invalid images show a loading error when selected. **Automatic (from action)** restores the original artwork after Save. Changing artwork preserves the action, command lines, target and recast settings.

Custom image references are relative to the icons folder, including subfolders. When sharing profiles, copy the matching image files and folder structure to the other installation's icons folder too; profiles do not embed image files. Refresh updates thumbnails and assigned images without a plugin reload. Missing images keep their saved reference so adding the file and refreshing restores them.

### Macros and palettes

Macros are shared library entries. Click a saved macro in Actions & Macros to edit it; dragging it to a hotbar still assigns it. Controller Confirm starts placement and Actions opens the editor. Saving or canceling returns to the same library view. Editing one updates every slot referencing it; duplicate it to make an independent variant. Deleting an entry leaves assigned slots with their commands. Multiline commands support `/wait` and `<wait seconds>`; macros wait for their own native subtarget selection before continuing. Opening an editor, changing character/job/zone, losing foreground or hiding XIUI+ cancels pending sequences. `/xiuiplus macros cancel` also cancels a sequence.

Create/Edit Macro follows the Lua layout: **Action Type**, a large icon preview and **Change** button at the upper right, **Macro Commands**, **Recast Source (Optional)**, **Slot Label**, then **Save / Cancel**. More options contains the native library, binding and availability controls. Command inputs have a dark backing and bright text. Eight lines appear when space permits; paging reaches all 32 supported lines. Enter and Up/Down move between rows, and multiline paste preserves command order without changing the saved action until Save.

Use **Paste** beside **Slot Label** to replace it with clipboard text, including Japanese names. Ctrl+V also works while editing text fields. Labels remain unchanged when saved and shared. Actions, optional recast sources and automatic icons recognize both English and Japanese resource names, such as `Cure` and `ケアル`. Recognized macro action arguments use the client's default resource name when executed; the editor keeps the original command text, targets and waits.

**Change** opens a searchable picker for **Game**, original **XIUI+**, and **Vincerno** artwork. Game entries include spell/ability recast icons and native item/equipment images; XIUI+ includes the default weapon-skill artwork. Vincerno retains its 220 custom variants. Search by action/item name, job, magic type or variant; Ctrl+F focuses search. Mouse, keyboard and controller share selection and scrolling. **Automatic (from action)** clears the override and infers artwork from the macro's main action: for Boost → wait → Combo, it selects Combo. Explicit choices take priority. Back/Cancel preserves the draft; Save applies it. Icon keys remain portable without extra installation.

**New macro** creates a sequence of commands and waits, such as Boost → wait → Combo. **Single command** creates one command shortcut. **Game macro** runs an existing FFXI macro by book, set, Ctrl/Alt row and slot. It does not copy the game's macro book into XIUI+. The bridge checks the installed client signatures and reports unavailable execution if they do not match. The create buttons include hover/controller help.

Click the action set caption to choose a set; the bar menu opens management. Create, rename, copy, reorder or delete sets there. A hotbar set contains all ten hotbars; switching it changes their assignments together. Crossbar sets are independent. Storage can be global, shared across a main job's subjobs, or specific to a main/subjob pair. Missing job/subjob data falls back to shared-main-job and then global data. Changing storage scope copies the current layout when creating a new destination context; existing destination palettes are preserved.

**Pet-aware assignments** selects matching overrides when a pet is present. The editor can save an override for the current pet; the base assignment remains the fallback.

### Keybinds

Select **Keybinds** in the left sidebar: directly below **Actions** for Hotbars and below **Controller Setup** for Crossbar. The sidebar item opens the list directly. Bar menus, `/xiuiplus keybinds`, `/xiuiplus hotbar keybinds` and `/xiuiplus crossbar keybinds` open the same independent, movable/resizable window. Its Bar dropdown lists all ten hotbars and Crossbar, including empty slots. Each row shows the slot/action and current binding; Clear removes the binding without removing the action.

For a hotbar, select its binding and press a key combination. Ctrl, Alt and Shift are supported. Escape cancels, and an existing conflicting assignment is reported without replacing either slot. Bindings stay attached to bar slots when action sets change. Explicit number-row bindings also work in Full keyboard mode and after zoning; unbound digits and ordinary letters remain available for chat. Active chat, native menus/targeting and editors retain their input.

For Crossbar, choose a trigger bank to see its actions, select XInput or DirectInput, then choose the button for a slot using the shared mapping dropdown. Auto follows the game mapping; Clear uses Unbound. Physical slot mappings apply to every trigger bank and use the same device settings as Controller Setup. Hardware mappings remain local when sharing profiles.

Mouse, keyboard and controller share the list, dropdowns and Clear actions. Directional navigation scrolls rows into view; Confirm opens a binding and Cancel dismisses a dropdown/capture or leaves the window. Keyboard capture consumes its key press/release and never executes the slot. Existing gameplay execution remains paused while the workspace is open.

### Controller Setup

Choose Crossbar → **Controller Setup** → **Controller Setup**, or run `/xiuiplus crossbar setup`. General is the first Crossbar topic, followed by Controller Setup. General, Mappings and Timing tabs keep everyday setup separate from raw device adjustments. Auto follows the effective FFXI mappings; Xbox, PlayStation, Nintendo and custom choices are also available. Physical mapping and replacement preferences stay on this computer when profiles are shared. Setup and Configuration pause gameplay trigger actions; close both to use the crossbar.

Mappings, input type and presets open the shared dropdown menus. Timing and trigger thresholds use shared sliders with click-to-type numbers; timing values are in milliseconds. Controller Confirm enters slider adjustment, Left/Right changes its value and Cancel leaves adjustment. Actions opens numeric entry. Release always remains below Press. Labels and controls share a compact value column.

Dropdowns use the same native panel and list styling as Input mode. Long lists include the shared scrollbar with thumb dragging, page clicks and wheel scrolling; controller selection stays visible. Setup defaults to a compact window; custom window sizes remain saved.

**Use XIUI+ trigger controls** is on by default, so trigger actions route to XIUI+ whenever the crossbar is enabled; turning it on later also enables the crossbar. Turning it off leaves physical trigger macros to FFXI while the bar remains available for editing and mouse use. Native replacement removes the selected trigger/button inputs before FFXI processes them; it does not disable macro commands or native macro reference actions.

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

### Importing

**Action Bars → Import** reads a local XIUI, tHotBar or tCrossBar saved Lua data file, displays a preview and warnings, then creates new palettes and shared macro records after **Import as new** is selected. Native INI action data is also supported. Paste the file path into Local file and choose Read and preview. Imports retain commands, targets, labels, icon references, job/subjob assignments, palette ordering and pet variants. Only action data is imported: current HUD visibility, positions, appearance, typography, input settings and controller mappings are preserved. Settings-only Lua files are rejected. Source Lua is parsed as restricted saved data and never executed. Existing palettes remain available; XIUI+ profile-file sharing remains in the separate profile menu.

The reusable library accepts typed actions as well as command macros, including unused entries from Lua. The standalone Actions & Macros window opens entries for editing and saves new entries to the library without assigning a hotbar slot. Drag an entry onto a slot, or choose a destination with the controller, to assign it. Opening the picker from an empty slot retains that explicit destination. Right-click a shared library entry, or focus it and press controller Actions / keyboard Menu, to edit it without assigning a slot. Shared edits update references; deleting a library entry keeps its currently assigned actions usable.

Macro editing offers Automatic, None or a selected spell/ability/weapon-skill/pet/item cooldown source, plus Always available. These affect displayed cooldown/availability metadata; commands retain their normal guarded execution path.

Hotbar and crossbar sets have independent names, selection and ordering. Combined sets from older settings split on load while retaining assignments. Text supports independent per-bar profiles and two-line labels; Layout separates horizontal/vertical gaps and crossbar diamond/group spacing. Crossbar visibility can follow held triggers or combat, with a release delay and fade.

Action data, macro commands and presentation settings follow normal XIUI+ profiles. Physical controller mappings, held input, browsing focus and pending macro execution do not travel with a share key. Imports do not apply a source addon's controller/device configuration.

### Commands and validation

- `/xiuiplus hotbar` or `/xiuiplus crossbar`: open configuration for that surface.
- `/xiuiplus hotbar actions` (or `edit`), `/xiuiplus crossbar actions`, `/xiuiplus macros`: open the action library.
- `/xiuiplus hotbar palettes`, `/xiuiplus crossbar palettes`: manage palettes.
- `/xiuiplus hotbar next` / `previous`, `/xiuiplus crossbar next` / `previous`: cycle palettes.
- `/xiuiplus hotbar import`: preview an import.
- `/xiuiplus actionbars status`: report palette/macro counts and native bridge status.
- `/xiuiplus actionbars run <bar 1-10> <slot 1-12>`: execute a hotbar assignment with the normal input and character guards. Native gathering events pause an existing macro until the animation ends, retaining elapsed waits.

Physical controller feel, remappers and live action execution are not covered by automated checks; the checks do not send live actions.

ABI 18 and later include the host-owned native macro runner and independent native-trigger suppression. Native macro replacement defaults off; using the XIUI+ crossbar is a separate setup choice. Updating this contract requires a matching development host/runtime pair.

ABI 22 carries the transient verified native focus slot with crossbar routing settings so controller callbacks can check live command/palette ownership without taking the runtime mutex. It is excluded from saved settings and portable profiles.

### FFXIV references

The interaction model follows Square Enix's [action assignment flow](https://na.finalfantasyxiv.com/uiguide/know/know-xhb/xhb_how_to.html): choose an action, then place it on a bar. Mouse assignment follows [drag-and-drop registration](https://na.finalfantasyxiv.com/uiguide/know/know-hb/hotbar_registration.html); the six shapes follow the [hotbar layout options](https://na.finalfantasyxiv.com/uiguide/know/know-hb/hotbar_shape.html). Trigger halves and ordered expanded holds follow the [expanded crossbar guide](https://na.finalfantasyxiv.com/uiguide/know/know-xhb/xhb_hold.html), with optional [double-tap banks](https://na.finalfantasyxiv.com/uiguide/know/know-xhb/wxhb_how_to.html). XIUI+ retains its FFXI-specific action validation, native input ownership and existing data format.

### Skillchains and Magic Burst

**Hotbars / Crossbar → Skillchains** contains the shared Action predictions switch and Retail/Horizon rules for every bar. Compatible closers receive skillchain hints; matching offensive spells receive MB hints. **Open HUD settings** opens **Extras → Magic Burst** for the independent display’s visibility, artwork, animation and text. `/xiuiplus skillchain` opens the prediction section; `/xiuiplus magicburst` opens the display editor.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)

[Shared HUD scaling](hud-scaling.md) keeps element coordinates separate from screen input/overlay anchors. Edit grips use the shared scale gesture; preserve independent scale values when changing layouts or inheritance.
