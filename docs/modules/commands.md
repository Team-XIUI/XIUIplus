# Command / Details

Controls native command-menu placement and the XIUI editing placeholder. Native menu text and game actions remain client-owned.

Commands support moving and centering only. They have no scale setting or edit grip; global and saved element scales do not change their native-sized placement preview.

**Start in:** [command_menu_module.cpp](../../src/runtime/modules/commands/command_menu_module.cpp), [native_menu_controller.cpp](../../src/host/native_menu_controller.cpp).

**Keep intact:** Keep menu positioning separate from native visibility and input ownership. Placeholder styling must not imply replacement command execution.

Native placement uses the current menu rectangle for height and the creation rectangle for horizontal offsets. Keep bottom anchors fixed when contents grow; a panel taller than the space above its bottom clips at the screen top.

The main Buy/Sell choice (`shopmain`), sell confirmation (`shopsell`) and bazaar comments (`comment`) follow command-menu placement. The other shop panels (`shop`, `shopbuy`) and their `iteminfo`/`money` panels retain native positions and stay out of saved command bounds. Detect all open shop menus for native input ownership. Ordinary bottom-anchored item details remain movable outside shops. Native purchase, quantity and Cancel input remains client-owned.

Equipment (`equip`) and inspection (`inspect`) stay at native positions, including their shared item-description panel. Keep detecting these native input owners while excluding their geometry from command placement.

With command placement enabled, suppress empty read-only comment windows at the native draw guard and omit them from command bounds. Validate the comment renderer and read the active three-line buffer, ignoring bytes after each terminator. Keep nonempty comments, comment editors and unsupported layouts visible; native input ownership remains unchanged.

**Focused check:** `xiui_native_hud_target_smoke --menus-only` and `xiui_casting_smoke --commands-only`.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
