# Satchel

A movable inventory browser supports search, bag/profile/slip pickers, local display ordering and validated item actions.

**Start in:** [satchel_module.cpp](../../src/runtime/modules/satchel/satchel_module.cpp), [satchel_actions.cpp](../../src/runtime/modules/satchel/satchel_actions.cpp), [satchel_source.cpp](../../src/runtime/modules/satchel/satchel_source.cpp), [hud_runtime_satchel.cpp](../../src/runtime/hud_runtime_satchel.cpp).

**Keep intact:** Every item-changing path revalidates captured identity and real native state. Cached/slip views stay read only. Locked HUD and ordinary native menus do not disable pointer controls; popup/search ownership ends on dismissal, with owned releases drained.

Dismissed popups retain native mouse blocking and held movement capture until the owned release.

**Focused check:** `xiui_satchel_ui_smoke --input-ownership-only`; item validation uses `xiui_satchel_actions_smoke`.

[Detailed guide](../reference/satchel.md) · [Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)

[Shared HUD scaling](../reference/hud-scaling.md) keeps element coordinates separate from screen input/overlay anchors. Edit grips use the shared scale gesture; preserve independent scale values when changing layouts or inheritance.
