# Satchel

A movable inventory browser supports search, bag/profile/slip pickers, local display ordering and validated item actions.

Item cells share [Equipment's slot frames](../../src/runtime/ui/item_slot_frame.hpp): flat theme backgrounds and thin rounded outlines. Slot background opacity is independent and defaults to 80% for both filled and empty cells; outer-window opacity still controls its frame and slot outlines. A one-time migration updates the previous 55% slot default, including profile overlays. Equipped, sale, flagged and hover markers use the same rounded silhouette. Full native item artwork retains rarity and HQ border details.

Dragging in Custom order softly tints and outlines occupied or empty destination cells. Dropping into an empty cell leaves the source empty; explicit empty positions persist with the per-character/bag layout through refresh and reload. The source and sorted views are not reorder targets. The preview clears with the gesture and never changes assignments before release. Focused check: `xiui_satchel_ui_smoke --slot-reorder-only`.

**Start in:** [satchel_module.cpp](../../src/runtime/modules/satchel/satchel_module.cpp), [satchel_actions.cpp](../../src/runtime/modules/satchel/satchel_actions.cpp), [satchel_source.cpp](../../src/runtime/modules/satchel/satchel_source.cpp), [hud_runtime_satchel.cpp](../../src/runtime/hud/hud_runtime_satchel.cpp).

**Native item order:** [satchel_native_order.cpp](../../src/runtime/modules/satchel/satchel_native_order.cpp) reads the client's per-container list behind the Item menu's Sort → Auto/Manual. FFXiMain keeps 18 sort objects in a table found by signature; each holds its container ID at `+0`, a loaded flag at `+4`, the active list index at `+0xC`, per-list counts at `+0x18` and four 81-entry `{u32 key, u16 item, u16 slot}` lists at `+0x804`. New items are appended to the end. The client saves these lists as `USER/<character hex>/is.dat`, `bs.dat` and so on at logout, not live. Unmatched signatures fall back to slot order.

**Keep intact:** Every item-changing path revalidates captured identity and real native state. Cached/slip views stay read only. Locked HUD and ordinary native menus do not disable pointer controls; popup/search ownership ends on dismissal, with owned releases drained.

Dismissed popups retain native mouse blocking and held movement capture until the owned release.

Item actions use a compact context menu anchored to the pointer or keyboard/controller-selected cell, with flat shared menu rows and grouped separators. Bag and equipment choices use the same popup style; Back/Escape/Cancel returns to the item menu without recapturing its identity. Outside presses dismiss the menu and retain their release. Rows activate only on matching press/release, and unavailable actions are skipped during navigation. Quantity, price and Cancel-first drop confirmation remain separate prompts.

Window drags and resizing commit on their owned mouse-up. Render-frame button polling must not cancel a pending release; lost capture or unavailable input still cancels the gesture.

Layout → Lock position defaults off and is independent of global HUD editing. It blocks title/grip gestures, keyboard/controller movement and shared edit controls while retaining browsing, search and item actions. Both this lock and slot-background opacity are portable preferences; their option IDs follow the existing text controls.

Container opacity defaults to 0.35. A one-time migration replaces the old opaque default while preserving other custom opacities, including profile overlays. Focused check: `xiui_satchel_ui_smoke --container-opacity-only`.

Transfers require both bag changes and the native inventory-completion acknowledgment. Server stacking skips settled bags with no combinable partial stacks; real sorts verify the final stack state as well as completion. Autosort coalesces per destination bag and defers during the native per-bag cooldown without holding the action dispatcher busy. Incomplete live bag reads keep the current action pending.

**Focused check:** `xiui_satchel_ui_smoke --input-ownership-only`; `--autosort-only` checks deferred sorting and cancellation. Item validation and completion use `xiui_satchel_actions_smoke`.

## Behavior

Open or close the inventory browser with `/xiuiplus satchel`. Use `/xiuiplus satchel open`, `close`, `config` or `status` for explicit commands. Its settings are under **Inventory → Satchel**, alongside Dots, Equipment and Gil. Satchel retains its own position, text profiles and visibility rules.

### Browsing

Choose a bag or All bags from the left menu, which shows occupied-slot counts beside each bag and also holds the storage-slip picker. The profile icon at the top right opens a character dropdown beneath the button; it opens upward when needed to stay on-screen. Its tooltip identifies the current live or saved character. Search below the title bar by item name or ID; search can include every bag. Scroll over the grid or use Page Up/Down to change pages. The grid uses native item artwork, stack counts and equipped, bazaar and locked-state borders. Gil appears at the bottom right. Hover, click or focus the ? beside Close for the item-action hints. Item details show available descriptions, equipment slots, jobs/levels, Rare/Ex flags, enchantment charges and supported system-1 augments. Unsupported extended data is omitted.

Drag the title bar to move the open window, or use the bottom-right grip to resize it freely, pixel by pixel. Item spacing tightens slightly before removing a row or column, keeping the artwork and frames at full size. Expanding adds rows/columns when their normal spacing fits. The grid reserves a scrollbar gutter only while overflowing. Small windows scroll the left bag/slip menu independently; the item grid shows a scrollbar when it has more pages. Both scrollbars support wheel input, track clicks and thumb dragging. Controller navigation keeps the focused bag or slip visible. Position and window dimensions save on release. Escape or input suspension cancels an unfinished move/resize. The character dropdown closes on an outside click or Cancel without changing characters. Mouse interactions hide navigation focus rings; keyboard/controller navigation reveals them.

Satchel moves and resizes while global HUD Edit Mode is off. **Inventory → Satchel → Layout → Lock position** independently freezes its position and resize controls, including shared edit-mode and controller movement. It starts unlocked. Search, bag selection, item actions and minimize/restore remain available while locked.

The **−** button immediately left of Close collapses Satchel to its title bar. It becomes **+** to restore the previous grid size, bag, search and page. The collapsed state stays local to the session settings and survives runtime reloads. Ctrl+F also restores the window and focuses search. Hidden contents cannot receive clicks or controller focus.

**Appearance → Window → Slot background opacity** controls filled and empty item cells equally, independently of the outer window. It defaults to 80% for a darker backing, retaining the shared rounded outlines and complete native item artwork.

Display sorting in Satchel settings offers Custom order, Name, Item ID, Quantity and Game order. Live bags follow the native Item menu's list (its Auto/Manual sort), which differs from server slot order; Game order shows that list unchanged and Custom order starts from it. Name, Item ID and Quantity break ties by it. Items the client has not listed yet follow in slot order. Saved characters keep the order observed last. Drag onto an occupied or empty grid cell to arrange the display; a soft highlight marks the destination before release. Dropping into an empty cell leaves the source cell empty, and those empty positions survive inventory refreshes and reloads. This does not move items on the server. Custom layouts stay local to the character and bag/slip view. Item fingerprints preserve order through slot compaction where possible; identical stacks use a deterministic occurrence order.

### Item actions

Right-click an item to open a compact context menu beside the pointer. Keyboard/controller activation anchors it to the selected item instead. The menu flips or moves inward near screen edges. Bag destinations and equipment slots appear as matching popups; Back, Escape or controller Cancel returns to the item actions. Click outside to dismiss the menu.

Choose a destination and quantity to move items, split stacks, use an eligible item on yourself, choose an equipment slot, set/remove a bazaar price, or drop a selected quantity. Quantity and price use separate entry prompts. Drop opens a separate confirmation with Cancel selected. Items requiring another use target remain available through the game's native item menu.

Drag an item onto a bag in the left menu to transfer it. Double-click targets Satchel for Inventory items and Inventory for other bags; an unavailable destination opens the destination picker. All paths use the same service and recheck the real item slot, character, loading state, locks, equipped/bazaar state, destination access, capacity and stack compatibility immediately before sending.

Server Sort is separate from display sorting and combines eligible partial stacks. Already-stacked bags complete immediately without a server request. Optional autosort runs after a confirmed Satchel move/split; repeated transfers share one pending sort per destination bag. The server's per-bag sort cooldown defers sorting without blocking transfers. Sorting does not repeat from its own updates. Item actions are disabled in cached/slip views and when Read only is enabled. Temporary items cannot transfer between bags. Home storage requires observed Mog House access. A runtime first loaded inside a Mog House may require leaving and reentering before home-storage transfers become available.

Transfers remain pending until both source reduction and destination growth are observed and the server confirms inventory completion for the affected bags. Server Sort waits for completion and verifies that all eligible partial stacks have been combined, including multi-stack sorts that send intermediate completion packets. Other actions require their appropriate inventory/equipment changes. Temporary incomplete bag reads retain the pending action; unconfirmed actions still time out. Zoning, character changes and hidden/suspended input cancel observation and deferred autosorts. Cancellation cannot undo a packet already submitted.

### Saved characters and storage slips

The character picker lists locally observed characters. After inventory loading has completed and a snapshot has settled, XIUI+ saves bags, gil and full item extra data under `config/XIUIplus/inventories/<server-or-world-character>.ini`. Each character has an independent asynchronous atomic writer, so different game clients cannot replace each other's character files. Up to 64 characters are retained. Existing valid snapshots survive incomplete loading; unsupported/corrupt files are skipped.

Saved characters are read only and show a Last observed time. Their inventory is refreshed when that character is logged in with XIUI+. Stable world/character identity is preferred. Private clients without native world identity can use the current process's observed login-server endpoint, requiring matching established data/auth connections on the standard 54230/54231 ports. Ambiguous local identity alone is never cached. Observation files and custom item layouts are excluded from portable HUD profiles and share keys.

The slip picker lists owned Porter Moogle slips and decodes the stored-item bits for all 28 supported catalogs. Stored items share the search, paging and tooltip presentation, including saved characters' slips. Viewing a slip does not deposit or withdraw items. Truncated or unknown slip payloads produce no fabricated contents. Catalog attribution and the upstream MIT notice are embedded with the plugin.

### Input

Use `/xiuiplus focus satchel` or choose Satchel in `/xiuiplus focus` for controller browsing. Directions navigate items across pages, Next/Previous tab switches bags, and Cycle window moves through the grid, bags, character/slip pickers, search, help, minimize/restore, title bar, resize grip and Close. Up/Down moves through the focused bag menu. Confirm/Actions opens the focused control; Cancel backs out or closes. Confirm on the title bar or resize grip starts adjustment: directions move or resize, Confirm saves, and Cancel restores the starting layout. Minimized browsing cycles through visible title-bar controls only. Numeric dialogs support direction adjustments; search accepts keyboard typing. Focused items show the same details as pointer hover.

Ctrl+F focuses search whenever Satchel is open and available for input, without requiring a prior click. It selects the current query for replacement. Escape leaves search and returns movement to the game while retaining the query; Enter explicitly enters keyboard grid browsing. Arrow keys, Enter, Escape, Tab/F6 and Page Up/Down share its operations while browsing.

Mouse clicks use temporary input ownership for an active editor, popup or window gesture. Dismissing or selecting a mouse-opened popup returns movement immediately; clicking bags, items, Help, scrollbars or blank space does not enter keyboard browsing. Clicking away from search releases its text focus. Ending controller browsing closes its transient editors/dialogs, and lost mouse capture cancels an unfinished gesture. Mouse controls remain available with the HUD locked or a native game menu open. Unclaimed keyboard/controller navigation stays with the native menu; clicking a Satchel editor or popup gives that control its own keyboard focus until dismissal. Native text entry, chat typing, targeting, cutscenes, zoning and lost foreground focus suspend Satchel input. Owned button/key releases are still consumed after dismissal. Search and focus do not activate an item. Held confirmation cannot repeat an item action or accept a newly opened drop confirmation.

The native `/satchel` and `/satch` aliases are intercepted only when **Override /satchel** is explicitly enabled in Satchel settings.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)

[Shared HUD scaling](hud-scaling.md) keeps element coordinates separate from screen input/overlay anchors. Edit grips use the shared scale gesture; preserve independent scale values when changing layouts or inheritance.
