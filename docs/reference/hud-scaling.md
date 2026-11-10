# Basic settings and HUD scale

New installations open **Basic Settings**. Existing installations retain Advanced until you switch. Both views share a bottom footer: profile dropdown, create and actions on the left, a settings icon to switch Basic/Advanced, and **Edit Mode** at the far right. Hover the settings icon for its destination. Switching modes retains the active profile and does not apply a preset. The choice stays local and is excluded from shared profiles.

Basic contains **Classic**, **XIV**, **XIVClassic**, **Theme**, **Global Scale**, and **Reset global scale**. Theme selects **XI Theme** or **XIV1.0 Theme** without applying a full preset. Profile utilities remain available. Ctrl+F opens Advanced search. Use the eye button to collapse the panel while arranging HUD elements.

The Basic page groups preset samples above compact theme and scale controls, with the profile dropdown and Advanced button in its footer. The scale label, slider, percentage field and Reset button share one line; explanatory text uses hover help. Search is rendered and hit-tested only in Advanced. Classic's sample uses the compact party entry with HP/MP and a TP number; XIV shows HP, MP and TP bars side by side. XIVClassic shows Classic's layout with XIV1.0 Theme. Each sample uses its own preset theme through the normal party renderer, independently of the live theme. Mouse, keyboard and controller use the same buttons, theme control and percentage editor. Hover help clears on every settings frame and stays behind modal overlays.

The former General sidebar category is now **Global**. Existing commands and saved category/topic identities remain compatible.

## Full HUD presets

Classic, XIV and XIVClassic use the normal confirmation dialog, with Cancel selected. Applying one restores its complete authored HUD preferences and layout, including enabled modules, positions, global/local scales, theme, text, bars, chat filters and action assignments. The active profile, saved profile collection, local input-device configuration and Basic/Advanced navigation state remain local.

- **XIV:** Cody's complete XIV setup captured on October 20, 2026 in [hud-xiv.ini](../../config/hud-xiv.ini).
- **Classic:** The same complete setup with only the Party list changed to the XI/Classic Party preset, captured in [hud-classic.ini](../../config/hud-classic.ini). Both Alliance lists and every other module match XIV.
- **XIVClassic:** Cody's complete XIV1.0 setup captured on November 1, 2026 in [hud-xivclassic.ini](../../config/hud-xivclassic.ini), including the current target/ToT layout, compact Party with XIV job icons, chat styling, shared Enemy List container and rounded Equipment grid. It selects XIV1.0 Theme and the single All chat window. This is an independent capture rather than a derivative of Classic.

Classic and XIV explicitly select XI Theme; XIVClassic selects XIV1.0 Theme. All retain the captured text effects and the game's current native Window Type. The preset buttons do not report a persistent selected state after custom edits. Captured layout viewport metadata lets the normal layout loader fit the saved positions to another game viewport.

Named presets explicitly retain their authored chat-window count and the migrated 4× minimap starting zoom. Their provenance records these compatibility amendments; fresh/reset defaults retain their own native chat-window adoption behavior.

New installs and **Reset to defaults** use the latest capture in [XIUIplus-defaults.ini](../../config/XIUIplus-defaults.ini). This baseline is independent of the named presets, so recapturing a customized setup does not change their Party layouts.

Presets load their saved values through the shared settings/profile boundary; they do not synthesize fonts, meter styles or a partial appearance overlay. Basic's samples read the Party settings from these same full captures. The immutable XIUI2 sharing baseline remains separate.

`/xiuiplus preset classic`, `/xiuiplus preset xiv` and `/xiuiplus preset xivclassic` apply the same complete presets directly. `/xiuiplus party preset classic|xiv` changes only Party through its normal preset loader, preserving other modules; the Advanced Party preset cards retain their existing Party/Alliance behavior.

Focused checks: `hud-presets` compares every persisted HUD preference against the complete presets and verifies theme ownership, the narrow XIVClassic derivation and the independent reset baseline; `defaults` covers embedded defaults, fresh installs and reset persistence. Capture or authorship provenance is recorded beside each INI in its `.capture.json` file. Regenerate live captures through [the maintained workflow](../development.md#capture-current-defaults).

## Global and element scale

**Global → Appearance → Window → Global Scale (%)** is the same value as Basic's slider. Its range is 50–200%, with direct numeric entry and a separate reset to 100%.

Each independently placed HUD element defaults to a single **Scale (%)** control with **Lock proportions** enabled near its layout/window controls. Local ranges are 25–300%. Global Scale is always uniform and multiplies both axes. For example, 150% global and 120% local produce 180% effective scale.

Width/height controls change the authored layout; scale leaves those values intact. Unlocking proportions explicitly exposes **Scale X (%)** and **Scale Y (%)** for stretching the whole element, including text, icons, padding and interaction regions. Locking again preserves the current ratio. Reset scale restores both axes to 100% and locks proportions. The first scale format migrates once to locked proportions while preserving its X/Y values; later explicit unlocks persist. Phantom Roll's existing Content scale remains an internal layout preference beneath the new whole-element scale.

Disabled elements keep their local scale. Attached child windows follow their owner's scale; detach them to use their retained independent scale. Hotbars, chat windows, Alliance lists, inventory dots, notification groups, and independent world-info parts have distinct scale values.

Native FFXI windows retain their existing controls. World-space [target rings](target-rings.md) have separate model-relative radius controls. World Target Bar and Chat Bubbles scale about their projected world anchors. World Target Bar has a stationary edit sample that cannot move its live world anchor. Chat Bubbles have no edit sample or corner handle; their scale remains available in settings.

## Edit controls

The footer's **Edit Mode ON/OFF** button uses a green filled dot for ON and a dark dot for OFF. Opening settings starts OFF: modules follow their live visibility, so a closed Satchel or empty Enemy List stays hidden. Hover a visible module to reveal its edit frame and grip; drag its body to move it. Selecting a module or topic in Advanced previews that element until another is selected. ON previews all enabled modules. Turning it OFF clears previews; reopening settings also starts clean. These choices are temporary and never change enabled modules or saved positions.

Hidden modules have no edit frames, grips, pointer targets, movement toolbar or controller movement entries. The toggle appears in both Basic and Advanced and is available under controller Utilities. Focused check: `xiui_config_input_smoke --edit-mode-only config/XIUIplus-defaults.ini`.

Visible editable elements have a top-right scale grip. The opposite bottom-left corner stays fixed while dragging outward grows the element upward and rightward. Uniform scaling projects pointer movement onto that diagonal, avoiding axis switches and large jumps on thin bars. Unlocking proportions permits independent axes with the same anchor. Viewport limits stop growth without sliding the element away.

Scale previews use a fixed drag-start snapshot, including content offsets, so changing live content does not change the drag ratio or grip. Releasing commits the new scale and its corner-anchored position; Cancel restores both. Entering a grip plays the shared native hover sound once; stationary hover and dragging remain quiet.

Chat uses its existing top-right handle for viewport resizing in both edit mode and the locked HUD, with no additional handle. It changes width/height and line capacity while keeping fonts unchanged, preserving the bottom-left anchor. The movement toolbar and controller utility call this action **Resize**. Whole-chat **Scale (%)** and Global Scale remain uniform and resize fonts normally; chat has no text-stretch controls. Legacy unequal chat scales convert to ordinary viewport dimensions, preserving their footprint subject to the window minimum. Chat supports heights down to 64 authored pixels.

- Pointer release commits a completed scale drag. Escape cancels it.
- The movement toolbar's **Scale** button or **Alt+F9** starts directional adjustment for the selected element. Right/Up grow the selected axes and Left/Down shrink them; proportions remain locked by default. Space switches fine/coarse steps, Enter commits, and Escape restores.
- Controller **Utilities → Scale** uses the same gesture. Directions adjust X/Y, Actions switches steps, Confirm commits once, and Cancel restores. The Movement picker enters scale directly for world-anchored preview samples.

An unfinished gesture is canceled when ownership is lost, its element becomes unavailable, settings/profiles change, or the UI suspends. Autosaves capture committed values while a preview is pending. Chat's edit-mode resize uses the shared gesture snapshot and existing manual-height/collapse policy.

Right-click a movable module in edit mode to open the shared dropdown with **Reset position**, **Reset scale** and **Reset settings**. Position restores its shipped coordinates fitted to the viewport; scale restores its local 100% uniform scale while retaining Global Scale. Commands omit the scale action. Reset settings restores the selected module's shipped settings and layout after a **THIS WILL RESET ALL SETTINGS!** warning naming the module, with Cancel selected and a red confirm button. Independent hotbars, notification groups and detached elements reset through their settings owner.

Keyboard users can open the same menu for the selected module with the Menu key or Shift+F10. Controller users choose **Actions** in the Movement region before entering a move gesture. Dropdown navigation, cancellation and confirmation use the shared input routes; pointer actions wait for a matching release and retain ownership when dismissed. Focused check: `./scripts/check.ps1 -Name module-menu`.

## Shared implementation

[hud_transform.hpp](../../src/runtime/ui/hud_transform.hpp) defines forward/inverse coordinate mapping. [hud_runtime_scale.cpp](../../src/runtime/hud_runtime_scale.cpp) resolves element transforms, viewport reachability, and scale gestures. Shared module controls append their option IDs after each module's existing options.

Saved dimensions and positions stay in authored coordinates. Screen bounds derive from global/local scale and a temporary reachability offset. Corner drags update placement to retain their opposite anchor; numeric and global scale changes retain their existing placement. Pending gestures never enter unrelated autosaves, and Cancel restores the original scale and position without cumulative rounding.

All shared drawing primitives pass through the scoped design-system transform. Text measurement, wrapping, baseline and every effect pass use the final native pixel size. For example, a configured 12-pixel font at 150% requests an 18-pixel font from the rasterizer. Glyph quads use those native pixels with placement rounded on the final screen grid, avoiding enlargement of the original small bitmap. Gradients, tabular numbers, stroke, shadow, glow and sheen retain their profiles. The glyph atlas admits larger pages within its existing 48 MiB texture budget; font faces and glyph entries remain bounded. XIUI+ icons select cached 32/64/128-pixel masks from their original supersampled paths, and vector edges retain a screen-pixel antialias fringe. Native game artwork retains its source resolution. Tooltips, management dialogs, configuration, cursor and edit controls retain screen sizing, with anchors mapped from the owning element.

Local scale values use portable `hud.scale.<stable-element-id>` sections. Global Scale uses `appearance.hud`; Basic/Advanced uses local `session.config`. Missing values load as 100%. The immutable XIUI2 sharing baseline remains unchanged.

Configuration has its own **Menu Scale (%)** under Global → Config Settings. It enlarges the settings window and its dialogs from 100% to 200% independently of Global Scale. See [Configuration](configuration.md) for local persistence and viewport fitting.

Focused check: `./scripts/check.ps1 -Name hud-scaling`. It covers geometry, shared gestures, committed snapshots, representative input regions, mode navigation, explicit preset ownership, and persistence. Cody verifies visual/gameplay behavior and physical controller acceptance.
