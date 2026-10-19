# Action Bars and crossbar

Hotbars, controller crossbar, reusable macros, palettes and their management windows share one action store and executor.

**Start in:** [actionbars_module.hpp](../../src/runtime/modules/actionbars/actionbars_module.hpp), [actionbars_display.cpp](../../src/runtime/modules/actionbars/actionbars_display.cpp), [hud_runtime_actionbars.cpp](../../src/runtime/hud_runtime_actionbars.cpp).

**Keep intact:** Keep native macro suppression separate from crossbar activation. All execution uses the validated executor. Pointer management can work over native menus; unclaimed keyboard/controller navigation remains native. Shared library edits must not silently assign slots.

**Focused check:** `xiui_actionbars_ui_smoke --input-ownership-only`; crossbar normalization uses `xiui_crossbar_input_smoke`.

[Detailed guide](../reference/actionbars.md) · [Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
