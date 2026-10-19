# Command / Details

Controls native command-menu placement and the XIUI editing placeholder. Native menu text and game actions remain client-owned.

**Start in:** [command_menu_module.cpp](../../src/runtime/modules/commands/command_menu_module.cpp), [native_menu_controller.cpp](../../src/host/native_menu_controller.cpp).

**Keep intact:** Keep menu positioning separate from native visibility and input ownership. Placeholder styling must not imply replacement command execution.

Native placement uses the current menu rectangle for height and the creation rectangle for horizontal offsets. Keep bottom anchors fixed when contents grow; a panel taller than the space above its bottom clips at the screen top.

**Focused check:** `xiui_native_hud_target_smoke --menus-only` and `xiui_casting_smoke --commands-only`.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
