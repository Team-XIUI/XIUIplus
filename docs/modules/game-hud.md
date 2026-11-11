# Native UI

The main **Native UI** category owns **HUD Windows** and **Menus and Dialogs**. HUD Windows has Visibility, Window Placement and Native Macro Bar pages, including overhead-name visibility and native macro replacement (off by default). Window Placement's Reset All Native Windows and `/xiuiplus nativeui reset` share the same action: clear placement overrides for the complete HUD/menu catalog, including pending legacy placements, without changing visibility. Saved native visibility and command preferences remain compatible.

**Start in:** [game_hud_module.cpp](../../src/runtime/modules/game_hud/game_hud_module.cpp), [native_window_module.hpp](../../src/runtime/modules/game_hud/native_window_module.hpp), [hud_runtime_native.cpp](../../src/runtime/hud_runtime_native.cpp), [native_hud_controller.cpp](../../src/host/native_hud_controller.cpp), [plugin.cpp](../../src/host/plugin.cpp).

Edit Mode's compact right-edge two-icon selector selects XIUI Edit Mode or Native UI Edit Mode. Native editing reveals the client-rendered HUD, adds labels/outlines to native draw submissions, and isolates XIUI module dragging. Pointer, keyboard/controller Utilities and the movement picker share actions. Position controls require supported native primitive geometry; compass/clock visibility remains available without inventing position writes. Editor mode is transient. Placements save under `native.<element>` independently of HUD scale. Menu categories are navigation only; each actual menu/result window has its own placement and visibility.

**Keep intact:** Use supported native hooks and restore owned state on unload. Native chat logs must remain available for NPC dialogue during events, without changing saved visibility choices. Hidden artwork does not mean its input is free. Keep macro-palette suppression separate from ordinary HUD masks.

Native placement prepares the current owner/menu set before the first native draw each frame. Newly submitted primitives, reopened owners, coordinate resets and logical page changes refresh through the same path before drawing. Refreshing geometry does not advance frame visibility or inactive-editor replay. Resolve explicit HUD owners before resource-name fallbacks so Alliance windows keep their own placement. Geometry must be available before applying a custom position; native resets update the saved restoration baseline before computing shared window origins.

Native owner slots supplement the active-menu list for geometry. During native editing, existing inactive primitives are submitted through the client's native renderer without opening input menus. The minus-key Main Menu also supports an isolated native DAT instance when closed, using the client constructor, resource binding, draw callback and destructor. It never joins the game input manager or replaces the real owner's primitive. Observe actual draw submissions before exposing editor hit targets. Other context windows require their client-owned data and geometry. Status Icons bounds use the populated DAT icon components, including their native size and spacing, instead of the thin anchor frame. Saved visibility resumes on exit.

**Focused checks:** `native-ui` and `xiui_native_hud_target_smoke --menus-only`. The menu check exercises first-draw placement/visibility for the complete menu catalog and supported HUD owners, mid-pass creation, page reuse, native resets, attached content and restoration.

Fishing stamina shares the party draw call. Keep party suppression active and expose live stamina through the separate [Fishing module](fishing.md).

[Detailed guide](../reference/game-hud.md) · [Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
