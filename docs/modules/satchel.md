# Satchel

A movable inventory browser supports search, bag/profile/slip pickers, local display ordering and validated item actions.

Item cells share [Equipment's slot frames](../../src/runtime/ui/item_slot_frame.hpp): flat theme backgrounds and thin rounded outlines. Slot background opacity is independent and defaults to 80% for both filled and empty cells; outer-window opacity still controls its frame and slot outlines. A one-time migration updates the previous 55% slot default, including profile overlays. Equipped, sale, flagged and hover markers use the same rounded silhouette. Full native item artwork retains rarity and HQ border details.

Dragging in Custom order softly tints and outlines occupied or empty destination cells. Dropping into an empty cell leaves the source empty; explicit empty positions persist with the per-character/bag layout through refresh and reload. The source and sorted views are not reorder targets. The preview clears with the gesture and never changes assignments before release. Focused check: `xiui_satchel_ui_smoke --slot-reorder-only`.

**Start in:** [satchel_module.cpp](../../src/runtime/modules/satchel/satchel_module.cpp), [satchel_actions.cpp](../../src/runtime/modules/satchel/satchel_actions.cpp), [satchel_source.cpp](../../src/runtime/modules/satchel/satchel_source.cpp), [hud_runtime_satchel.cpp](../../src/runtime/hud_runtime_satchel.cpp).

**Native item order:** [satchel_native_order.cpp](../../src/runtime/modules/satchel/satchel_native_order.cpp) reads the client's per-container list behind the Item menu's Sort → Auto/Manual. FFXiMain keeps 18 sort objects in a table found by signature; each holds its container ID at `+0`, a loaded flag at `+4`, the active list index at `+0xC`, per-list counts at `+0x18` and four 81-entry `{u32 key, u16 item, u16 slot}` lists at `+0x804`. New items are appended to the end. The client saves these lists as `USER/<character hex>/is.dat`, `bs.dat` and so on at logout, not live. Unmatched signatures fall back to slot order.

**Keep intact:** Every item-changing path revalidates captured identity and real native state. Cached/slip views stay read only. Locked HUD and ordinary native menus do not disable pointer controls; popup/search ownership ends on dismissal, with owned releases drained.

Dismissed popups retain native mouse blocking and held movement capture until the owned release.

Item actions use a compact context menu anchored to the pointer or keyboard/controller-selected cell, with flat shared menu rows and grouped separators. Bag and equipment choices use the same popup style; Back/Escape/Cancel returns to the item menu without recapturing its identity. Outside presses dismiss the menu and retain their release. Rows activate only on matching press/release, and unavailable actions are skipped during navigation. Quantity, price and Cancel-first drop confirmation remain separate prompts.

Window drags and resizing commit on their owned mouse-up. Render-frame button polling must not cancel a pending release; lost capture or unavailable input still cancels the gesture.

Layout → Lock position defaults off and is independent of global HUD editing. It blocks title/grip gestures, keyboard/controller movement and shared edit controls while retaining browsing, search and item actions. Both this lock and slot-background opacity are portable preferences; their option IDs follow the existing text controls.

Container opacity defaults to 0.35. A one-time migration replaces the old opaque default while preserving other custom opacities, including profile overlays. Focused check: `xiui_satchel_ui_smoke --container-opacity-only`.

Transfers require both bag changes and the native inventory-completion acknowledgment. Server stacking skips settled bags with no combinable partial stacks; real sorts verify the final stack state as well as completion. Autosort coalesces per destination bag and defers during the native per-bag cooldown without holding the action dispatcher busy. Incomplete live bag reads keep the current action pending.

**Focused check:** `xiui_satchel_ui_smoke --input-ownership-only`; `--autosort-only` checks deferred sorting and cancellation. Item validation and completion use `xiui_satchel_actions_smoke`.

[Detailed guide](../reference/satchel.md) · [Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)

[Shared HUD scaling](../reference/hud-scaling.md) keeps element coordinates separate from screen input/overlay anchors. Edit grips use the shared scale gesture; preserve independent scale values when changing layouts or inheritance.
