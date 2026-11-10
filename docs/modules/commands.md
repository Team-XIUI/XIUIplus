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

Saved positions use `native.<window>` preferences. A versioned split migration turns older family placements into individual positions while retaining each window's native relative offset. Pending offsets persist for unopened windows. The old shared command anchor is converted to independent placements and is not exported as live runtime coupling. Resetting a window clears only that window's placement; its siblings stay unchanged.

**Focused checks:** `native-ui` and `xiui_native_hud_target_smoke --menus-only` cover independent selectors/results, content coordinates, native page reuse, visibility, restoration, migration and controller routes.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
