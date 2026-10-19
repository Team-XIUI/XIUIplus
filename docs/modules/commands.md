# Command / Details

Controls native command-menu placement and the XIUI editing placeholder. Native menu text and game actions remain client-owned.

**Start in:** [command_menu_module.cpp](../../src/runtime/modules/commands/command_menu_module.cpp), [native_menu_controller.cpp](../../src/host/native_menu_controller.cpp).

**Keep intact:** Keep menu positioning separate from native visibility and input ownership. Placeholder styling must not imply replacement command execution.

**Focused check:** `xiui_native_hud_target_smoke`; layout cases also live in `xiui_world_smoke`.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
