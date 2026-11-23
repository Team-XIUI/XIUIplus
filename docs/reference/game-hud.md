# Native UI

The main **Native UI** category owns **HUD Windows** and **Menus and Dialogs**. HUD Windows has Visibility, Window Placement and Native Macro Bar pages, including overhead-name visibility and native macro replacement (off by default). Window Placement's Reset All Native Windows and `/xiuiplus nativeui reset` share the same action: clear placement overrides for the complete HUD/menu catalog without changing visibility. Preferences are saved independently for each window. **Keep menus above chat** (on by default) lifts menus the game stacks above its log above XIUI+ chat; see [native menus](commands.md).

**Start in:** [game_hud_module.cpp](../../src/runtime/modules/game_hud/game_hud_module.cpp), [native_window_module.hpp](../../src/runtime/modules/game_hud/native_window_module.hpp), [hud_runtime_native.cpp](../../src/runtime/hud/hud_runtime_native.cpp), [native_hud_controller.cpp](../../src/host/native_hud_controller.cpp), [plugin.cpp](../../src/host/plugin.cpp).

Edit Mode's compact right-edge two-icon selector selects XIUI Edit Mode or Native UI Edit Mode. Native editing reveals the client-rendered HUD, adds labels/outlines to native draw submissions, and isolates XIUI module dragging. Pointer, keyboard/controller Utilities and the movement picker share actions. Position controls require supported native primitive geometry; compass/clock visibility remains available without inventing position writes. Editor mode is transient. Placements save under `native.<element>` independently of HUD scale. Menu categories are navigation only; each actual menu/result window has its own placement and visibility.

**Keep intact:** Use supported native hooks and restore owned state on unload. Native chat logs must remain available for NPC dialogue during events, without changing saved visibility choices. Hidden artwork does not mean its input is free. Keep macro-palette suppression separate from ordinary HUD masks.

Native placement prepares the current owner/menu set before the first native draw each frame. Newly submitted primitives, reopened owners, coordinate resets and logical page changes refresh through the same path before drawing. Refreshing geometry does not advance frame visibility or inactive-editor replay. Resolve explicit HUD owners before resource-name fallbacks so Alliance windows keep their own placement. Geometry must be available before applying a custom position; native resets update the saved restoration baseline before computing shared window origins.

Native owner slots supplement the active-menu list for geometry. The client's 44-byte menu table holds a non-menu debug record (`tkdebug dbdelsel`) before its last entries (Monstrosity, `mnstorag`, `mcrmogc`, `cmbmenu`, `cmbhlst`, `cmbhwin`); scan past it and keep the catalog covering the whole table. During native editing, existing inactive primitives are submitted through the client's native renderer without opening input menus. The minus-key Main Menu also supports an isolated native DAT instance when closed, using the client constructor, resource binding, draw callback and destructor. It never joins the game input manager or replaces the real owner's primitive. Observe actual draw submissions before exposing editor hit targets. Other context windows require their client-owned data and geometry. Status Icons bounds use the populated DAT icon components, including their native size and spacing, instead of the thin anchor frame. Saved visibility resumes on exit.

**Focused checks:** `native-ui` and `xiui_native_hud_target_smoke --menus-only`. The menu check exercises first-draw placement/visibility for the complete menu catalog and supported HUD owners, mid-pass creation, page reuse, native resets, attached content and restoration.

Fishing stamina shares the party draw call. Keep party suppression active and expose live stamina through the separate [Fishing module](fishing.md).

## Native game HUD visibility

For a temporary clean view, use **General → Visibility → Hide all UI** or `/xiuiplus hide`. The configuration action has a red label/button and opens a Cancel-first warning before hiding. It explains that chat and configuration also disappear and gives recovery instructions: press `/`, type `xiui show`, then press Enter. Typing is invisible while hidden.

Hiding XIUI+ and every supported native HUD element together exits edit mode and releases XIUI+ input focus. `/xiuiplus show` restores XIUI+ and the previous native visibility choices; `/xiuiplus toggleui` toggles between them. Module enable switches, layouts and individual native settings remain unchanged. The hidden state survives runtime hot reloads and resets when the plugin is unloaded.

Open `/xiuiplus gamehud`, or select **Native UI → HUD Windows → Visibility** in configuration. **Hide all** hides every supported native HUD element and enables Apply visibility choices. **Show all** restores every element. Both actions update the individual visibility switches and save through the normal settings path. XIUI+ modules retain their visibility and positions.

Each element has a visibility toggle: on leaves it available to the game, off hides it. Missing values and this section's reset use the shipped XIUI+ preset: native player/party/alliance frames, target, cast bar, status icons, compass, clock/weather and chat start hidden; connection, title and help remain available. Explicit saved choices take priority. **Apply visibility choices** pauses or resumes the selected hides without clearing them. Module enable state does not override these toggles. Overhead names are controlled here through the existing game name-display setting. Native palette replacement and opening delay are grouped under **Native Macro Bar**.

### Editing native positions

Turn Edit Mode on, then select the top icon for **XIUI Edit Mode** or the bottom icon for **Native UI Edit Mode** in the compact right-edge selector. Both icons stay visible, with a highlight that slides to the selected mode. Hover help names each icon. It reveals the actual native HUD and labels windows observed in the native draw path. Windows hidden with the editor keep their native-position outlines and Show buttons. Closed menus stay in the settings list. Drag a supported outline, or use Configuration's controller movement picker. Controller Utilities can switch editors and toggle the selected native window. **XIUI Edit Mode** returns to XIUI HUD editing; switching modes does not move either layout.

**HUD Windows → Window Placement** offers a window selector, independent screen-pixel position and Reset position. Native compass/clock visibility is supported; position controls stay unavailable where no supported native rectangle exists. **Menus and Dialogs** includes the minus-key Main Menu and its submenu families, alongside spells/abilities, equipment/inspection, merchants, auctions and other dialogs. Choose the exact **Native window** within a category. The Status selector and its results, equipment, storage, gil and item details each have independent controls. Character Status carries its EXP bar and text, and equipment carries its native numeric labels; see [native menu ownership](commands.md).

Custom placement saves in `native.<element>` preferences, independently of HUD/configuration scale. Reset position clears only placement. **HUD Windows → Window Placement → Reset All Native Windows** restores default positions for every native HUD window, menu and dialog, including closed windows, while keeping visibility choices. Moving one window does not move other windows in its category. The editor selection itself is transient. Moving/hiding artwork leaves native input and game actions intact.

**Keep menus above chat** (on by default, `game_hud.menus_above_chat`) lifts native menus and dialogs that visible XIUI+ chat windows would cover, such as commands, spell lists, shop prompts and NPC choices, to just above chat, wherever the game's own log currently ends. Windows with a custom position, windows outside the chat's width and chat placed in the upper half of the screen are unaffected; turning it off restores native positions.

Native UI Edit Mode reveals the game's own HUD renderer without changing saved visibility choices. Existing inactive windows, including the minus-key Main Menu, draw through the native renderer without opening their input menus. Play Time uses the game's chat output rather than a separate window. XIUI+ adds Title Case labels, edit outlines and Show/Hide controls. Status Icons outlines encompass the populated icon components, including multiple rows. Context windows the client has not created remain in settings until available; no sample contents or invented positions are drawn.

Positions come from validated native rectangles and owner slots. Captured rectangles remain available for position settings, but a cached rectangle alone never creates an on-screen window. Show/Hide applies to the selected native window; leaving native editing restores the saved visibility policy. Macro execution/replacement ownership remains unchanged. `/xiuiplus nativeui edit` enters native editing directly. `/xiuiplus commands status` reports native, captured, custom or unopened geometry.

The section includes player HP/MP/TP bars, party, both alliance lists, target box, cast bar, status icons, compass, clock, connection information, menu title, help bar, both chat windows, and expanded chat. **Chat window 1** hides or restores both the main log and its typing input. Unavailable controls are labeled and cannot be changed. These settings affect the game's built-in HUD; XIUI+ modules retain their own Enabled options. Hiding the target box preserves the native main-target and sub-target arrows above entities.

XIUI+'s target snapshot reads the selected target index and entity name, HP percentage, and distance directly from game state. It never reads the native target window's loaded flag or cached text/HP. Player vitals and XP likewise come from the game memory interfaces, so native HUD visibility does not control XIUI+ module data. Entity indices are checked against the current entity map before access.

Preferences save under `[game_hud]` in `XIUIplus.ini`. Native windows still follow the game's normal lifetime, menu, and login rules when their visibility option is on.

During NPC events/cutscenes, XIUI+ temporarily stops suppressing all three native chat logs so dialogue remains visible even when **Hide during events** hides XIUI+ chat. The saved native visibility choices resume afterward. Explicit **Hide all UI** retains its documented full-hide behavior.

Development commands:

- `/xiuiplus gamehud` opens this config section.
- `/xiuiplus nativeui reset` restores every native HUD, menu and dialog position while keeping visibility choices.
- `/xiuiplus nativeui move <key> <x> <y>` and `/xiuiplus nativeui restore <key>` place or restore one window by its settings key (`equipment`, `window.cmbmenu`) for live checks.
- `/xiuiplus gamehud show all` restores every element.
- `/xiuiplus gamehud hide all` hides every supported element and enables Apply visibility choices.
- `/xiuiplus gamehud hide <element>` hides one supported element.
- `/xiuiplus gamehud show <element>` restores one element.
- `/xiuiplus gamehud status` reports supported controls, active hides, and suppressed draws.

Element keys are `player`, `party`, `alliance1`, `alliance2`, `target`, `castbar`, `status`, `compass`, `clock`, `connection`, `title`, `help`, `chat1`, `chat2`, and `chatlog`.

### Implementation

The stable host owns native suppression. The hot-swappable runtime supplies only a requested visibility mask, so runtime swaps retain the native draw guard. A faulted runtime supplies an empty mask, restoring native rendering.

The host resolves menu-owner slots in the loaded game module. One validated x86 primitive-draw guard skips only objects whose owner still matches a selected native HUD slot. This covers windows created during the current scene. Other windows, XIUI+ rendering, and the game window-update logic continue normally. The target box is handled separately at the same draw boundary: only its frame artwork is disabled and its documented box/icon coordinates are moved off-screen. Its native callback and both arrow anchors remain active. Restoration checks the current owner/primitive and changes only fields still carrying XIUI+'s hidden values; native position updates made while hidden are retained. Compass and connection controls use separately validated one-byte changes, restored only while their current values still match XIUI+'s changes. The clock (time and map grid under the compass, drawn even when the compass is hidden) follows a flag on the client's compass object, located through the client's own clock toggle. The client recreates that object with the clock on at each login, so the host reapplies the choice whenever the flag differs instead of sending a single `/clock off`; restoring a clock hidden by XIUI+ turns it on. Clients without the validated toggle fall back to the `/clock` commands.

Party suppression stays active throughout fishing. The native party draw call includes both party rows and fish stamina; skipping it hides both. **Extras → Fishing** replaces the stamina gauge with an independent, movable XIUI+ bar, enabled by default. The host validates the party owner's update function and primitive attachment before reading its signed stamina percentage at `+0x3A` (0–100 while hooked, -1 otherwise). The alternate party preview flag at `+0x40` is not a fishing mode. Stamina continues updating with native drawing suppressed; native fishing input remains unchanged. Unloading removes the draw hook, restores target-box fields and owned byte changes, and restores a clock hidden by XIUI+. Other addons controlling the same native windows should be unloaded before enabling overlapping controls.

Other plugins detour the same game functions, so the draw and window-move lookups (`FindUniqueEntry`) accept a jump over the function's first instructions and work in either load order. [HeadsUp](https://github.com/Tanyrus/headsup) gates the window move this way; XIUI+ moves still enter through that gate, so HeadsUp may adjust the destination of a window it also places.

Main chat suppression includes the separately resolved `inline` menu owner, using the same guarded draw path as `logwindo`. Input contents, command processing, and native state are not modified. Both owner slots must resolve before this control is available; showing chat, disabling HUD controls, or unloading restores drawing. Secondary and expanded log toggles do not affect input visibility.

The availability and requested-visibility masks are part of host/runtime ABI 5. Updating from an earlier ABI requires replacing both DLLs with the host unloaded once.

### Research and dependency notices

Native menu layouts and signatures follow these public references:

- [Ashita hideparty](https://github.com/AshitaXI/Ashita-v4beta/blob/main/addons/hideparty/hideparty.lua): party/alliance owner locations and native primitive layout.
- [HXIUIBegone](https://github.com/ryuutatsuo23-max/HXIUIBegone): individual native menu records, fishing behavior, and the connection dispatcher.
- [HudBeGone](https://github.com/johndelker/hudbegone): solo/status menu identities and native primitive draw entry/epilogue signatures.
- [Fenestra menu table layout](https://github.com/Windower/Fenestra/blob/e5bfb6442f49bdb4d31859bff6218bbeb1bea620/core/src/hooks/ffximain.cpp#L82-L90).

Those Lua addons are not installed or loaded as dependencies. The host's native implementation uses [MinHook](https://github.com/TsudaKageyu/minhook); its source notices and [license](../../third_party/minhook/LICENSE.txt) are retained in `third_party/minhook`.

Focused check: `xiui_native_hud_target_smoke`.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
