# Action Bars and crossbar

Hotbars, controller crossbar, reusable macros, palettes and their management windows share one action store and executor.

**Start in:** [actionbars_module.hpp](../../src/runtime/modules/actionbars/actionbars_module.hpp), [actionbars_editor.cpp](../../src/runtime/modules/actionbars/actionbars_editor.cpp), [actionbars_display.cpp](../../src/runtime/modules/actionbars/actionbars_display.cpp), [hud_runtime_actionbars.cpp](../../src/runtime/hud_runtime_actionbars.cpp).

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

Running macros retain their current line and wait deadline while native menus, events, maps or target selection own input, including the item-trade menu before a mining animation. They resume when native input clears. New activations remain guarded; foreground, editor and character/zone/job restrictions still cancel the sequence. `/xiuiplus actionbars run <bar 1-10> <slot 1-12>` activates a hotbar assignment through the same validated executor for command-based use and AgentLink tests.

`/xiuiplus actionbars macrotest` runs an unsaved scratch macro for AgentLink. `set /echo a <wait3.6> | /echo b` replaces it in one command, splitting on `|` before each `/`. Commands AgentLink queues in the same frame can reorder, so separate `add <line>` calls need a `wait` between them. `run` uses the same executor and guards as a hotbar macro, but that run continues while the game is unfocused, so tests never need to take focus. `report` lists each dispatched line's offset and gap for the latest macro run from any source, with how it ended. `show` and `clear` manage the lines.

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

[Detailed guide](../reference/actionbars.md) · [Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)

[Shared HUD scaling](../reference/hud-scaling.md) keeps element coordinates separate from screen input/overlay anchors. Edit grips use the shared scale gesture; preserve independent scale values when changing layouts or inheritance.
