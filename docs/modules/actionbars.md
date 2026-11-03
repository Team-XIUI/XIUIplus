# Action Bars and crossbar

Hotbars, controller crossbar, reusable macros, palettes and their management windows share one action store and executor.

**Start in:** [actionbars_module.hpp](../../src/runtime/modules/actionbars/actionbars_module.hpp), [actionbars_editor.cpp](../../src/runtime/modules/actionbars/actionbars_editor.cpp), [actionbars_display.cpp](../../src/runtime/modules/actionbars/actionbars_display.cpp), [hud_runtime_actionbars.cpp](../../src/runtime/hud_runtime_actionbars.cpp).

Create/Edit Macro follows the Lua editor's order: action type, upper-right artwork preview, command block, optional recast source, slot label and Save/Cancel. Native library, availability and binding controls live under More options. Eight command rows appear when space permits; every page, added row and keyboard transition uses the same visible-row count. Multiline paste is bounded and atomic. Editor inputs use the shared text control with a dark fill, bright foreground and 14-pixel minimum.

The icon picker groups Game, original XIUI and Vincerno artwork with cached name/category searches. Game spell/ability recast artwork and item bitmaps use the existing icon cache; large grids bound their native item working set. Automatic inference is separate from recast inference: prefer a weapon skill over preparation such as Boost, retain explicit artwork and preserve saved icon keys. `--macro-editor-only` covers geometry, rows, paste, shared dropdowns, icon sources and draft persistence; the action-data check covers icon priorities.

Global visuals are opt-in per hotbar. Preserve independent shapes, assignments, keys and positions; unlinking restores saved local visuals. The detailed guide records remaining Lua compatibility gaps.

**Keep intact:** Keep native macro suppression separate from crossbar activation. All execution uses the validated executor. Pointer management can work over native menus; unclaimed keyboard/controller navigation remains native. Shared library edits must not silently assign slots.

Verified native Attack/command menus permit assigned action-bar shortcuts, clicks and crossbar actions. Other native menus and active action/subtarget selection retain their guards. Refresh the executor's transient menu permission at every input path and scheduler update.

An active native macro palette can sit above a still-visible command menu. Identify ownership from the verified focused palette, not the presence of a background menu; holding Ctrl/Alt must not cancel delayed macro lines.

`shared/native_target.hpp` identifies actual target selection from an explicit subtarget or pending native action/cancel callbacks. Action flags, types and IDs can remain after dispatch, including a zero ID from ranged attacks; they do not establish input ownership. Host/controller input, action execution and macro waits share this check so completed targeted macros do not disable later hotbar input. Real selectors remain guarded even for zero-ID actions. `xiui_actionbars_data_smoke` covers retained flags/IDs, callback completion and native selection ownership.

`/xiuiplus actionbars status` reports current TP, pending macro line/wait and the last queued timed line. Input suspension records its cancellation reason instead of silently discarding the remaining sequence.

Replace native macros defaults off in code, resets and bundled setups. Preserve explicit saved choices.

Users keeping native macros can enable **Instant native macro bar** in Extras → Native Menus. It changes the native Ctrl/Alt palette delay and preserves command waits and action cooldowns. It remains independent of hotbar visibility and pauses while Replace native macros is enabled; see [General](general.md).

The Action Bars sidebar page follows the enabled state of all ten hotbars and the crossbar. Hide it only when all bars are disabled; global visual settings do not count as an enabled bar.

Closing Configuration hands focus to an open action workspace and retains its unsaved draft. Keep its close button, Escape and controller Cancel usable after that handoff.

Workspace close buttons activate on release and cancel outside the button. Popup dismissal retains mouse blocking and held movement capture through its owned release.

Locked bars require Shift when pressing to rearrange slots; unlocked bars allow ordinary click-drag. Shift reads both mouse modifier flags and keyboard state, including in Movement mode. The gesture temporarily bypasses destination locks, reveals hidden empty slots and retains that permission through release, preserving saved locks and slot keybinds. Library entries still drag directly onto a bar. Cancelled clicks drain their release in both the runtime and module; fresh presses replace any stale release marker. Native chat cleanup preserves ordinary hotbar hover capture so successive macro clicks need no Escape.

Actions & Macros is nonmodal to exposed hotbars: slot drags, bar locks and slot/bar menus remain usable with the library open. The library keeps its search, category, scroll, assignment context and draft behind child menus; dismissing a menu restores it. Window bounds take precedence over covered bars, and an active drag retains its press/release ownership. Existing slot actions and controller Cancel are shared with the child menus. Focused check: `xiui_actionbars_ui_smoke --library-bars-only`.

Clicking a saved macro in Actions & Macros opens its editor without assigning or executing it. Dragging the entry still assigns it to a hotbar slot; controller Confirm retains destination placement and Actions opens the editor. Save, Cancel, Escape and deletion return to the existing library search, category, scroll and slot context. New macro, Single command and Game macro also remember that context while their independent editor is open. Save refreshes results immediately.

`ReleaseChatInput` must preserve action-workspace pointer capture while native chat polls the command menu. The input-ownership fixture places that poll between drag/button press and release, during and after combat, and checks suspension cleanup. Create labels distinguish multi-line New macro, Single command and an existing FFXI Game macro; old imported type names remain accepted.

**Focused check:** `xiui_actionbars_ui_smoke --input-ownership-only`; crossbar normalization uses `xiui_crossbar_input_smoke`.

[Detailed guide](../reference/actionbars.md) · [Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)

[Shared HUD scaling](../reference/hud-scaling.md) keeps element coordinates separate from screen input/overlay anchors. Edit grips use the shared scale gesture; preserve independent scale values when changing layouts or inheritance.
