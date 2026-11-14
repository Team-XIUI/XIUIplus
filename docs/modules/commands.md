# Native menus and dialogs

**Native UI → Menus and Dialogs** uses categories for navigation. The **Native window** selector chooses an individual window, with its own position, visibility, reset and editor outline. Categories never define shared drag bounds. Mouse, keyboard and controller use the same selected window and movement actions.

**Start in:** [window identities](../../src/shared/native_menu_windows.hpp), [navigation categories](../../src/shared/native_dialog.hpp), [command_menu_module.cpp](../../src/runtime/modules/commands/command_menu_module.cpp), [native_window_module.cpp](../../src/runtime/modules/game_hud/native_window_module.cpp), [native_menu_controller.cpp](../../src/host/native_menu_controller.cpp).

## Window ownership

The native resource catalog names individual windows across Main Menu submenus, equipment, inventory, merchants, auctions, maps, party, search, linkshell, configuration and other in-game dialogs. Native DAT/owner identity selects a window; broad resource prefixes do not merge menus or result panels. Equipment, inspection, the character panel, storage list, item descriptions and gil can each be placed and hidden independently.

Status has a separate right-side **Status Menu** and individual **Character Status**, **Profile**, **Job Levels**, **Master Levels**, **Combat Skills**, **Magic Skills**, **Craft Skills**, currency, Unity, merit and job-point windows. Character Status includes its adjoining EXP/limit-point primitive. Currencies and Currencies 2 share a client DAT/owner but have separate identities, selected from the validated renderer's page mode. Reusing that native object must not carry the old page's position or hide into the new page. **Play Time** invokes the game's `/playtime` chat output; there is no separate result window to reproduce.

Aliases are limited to alternate DATs for the same window and inseparable visual parts, such as compact/long variants of a macro palette, the Character Status EXP panel and the map image/frame. Header/help bars, native input and game actions keep their existing ownership.

## Native content and lifetime

The host validates primitive/resource/owner and logical page identity before moving, hiding or replaying a window. Native positioning refreshes ordinary child components and owner callbacks. For owners that cache a rectangle only at creation, reuse the client's frame reader after positioning. Validate the short initializer's exact frame-read call or a recognized complex initializer before writing that cache. This covers EXP/limit-point bars, Profile and skill/master/merit result text. Equipment's ten native numeric labels use their own validated renderer and relative coordinates. Restore these caches with the game position; do not redraw their text in XIUI+.

Native Edit Mode adds labels, outlines and Show/Hide controls to actual native submissions. Existing inactive primitives use the client renderer without opening input menus. The minus-key Main Menu also has an isolated native DAT instance while closed or suspended behind a submenu. It never joins the game's input manager and is destroyed when editing ends or the real menu is visible. Other context windows require the client's data and geometry. Never create sample results or cached-rectangle placeholder frames.

Opening windows acquire their saved position and visibility before their first native draw. The render preparation pass discovers complete groups, including Character Status/EXP and equipment content; a validated draw-time fallback handles primitives created later in that pass. Page reuse and game coordinate resets use the same discovery and positioning code without clearing frame-submission history.

Saved positions use only `native.<window>` preferences. Each window reads and saves its own position and visibility directly. There is no shared command anchor, grouped-position import or automatic placement of sibling windows. Resetting a window clears only that window's placement.

**Keep menus above chat** (HUD Windows → Window Placement, on by default) emulates the game's log anchor for windows without a custom position. The game rests these windows on the native log's live frame top, which grows and shrinks two pixels a frame with its message count whenever the client's minimum and maximum log lines differ, and re-places them every frame it moves. The host therefore takes the stack from that live top: lower-half windows ending on it, plus windows resting on those, that overlap the saved bounds of visible XIUI+ chat windows horizontally. They share one upward shift that rests the lowest window two pixels above chat, so the stack keeps its native order, stays on screen and does not move while the log resizes. A window detached from that stack is never lifted, a custom-positioned window still anchors the windows resting on it, and a moved window that leaves the menu list or whose native object is reused returns to the game's position first. The editor reports the shifted rectangle so enabling a custom position starts where the window appears. The native log and HUD windows never shift.

NPC choices, guild/reward menus, item descriptions and Command Menu keep independent positions and visibility. Native input ownership is observed from live open menu owners before applying placement or visibility preferences. A hidden or moved menu still owns its native input; item descriptions alone do not claim menu input.

**Focused checks:** `native-ui`, `xiui_casting_smoke --commands-only` and `xiui_native_hud_target_smoke --menus-only` cover selectors/results, content coordinates, native page reuse, guild/reward choices with all three item-description windows, input ownership, visibility, restoration, keeping menus above chat and controller routes.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
