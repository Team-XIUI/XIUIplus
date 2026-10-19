# Native Game HUD

Selectively hides native HUD elements while XIUI replaces them. Command placement and native macro execution have separate owners.

**Start in:** [game_hud_module.cpp](../../src/runtime/modules/game_hud/game_hud_module.cpp), [native_hud_controller.cpp](../../src/host/native_hud_controller.cpp), [plugin.cpp](../../src/host/plugin.cpp).

**Keep intact:** Use supported native hooks and restore owned state on unload. Hidden artwork does not mean its input is free. Keep macro-palette suppression separate from ordinary HUD masks.

**Focused check:** `xiui_native_hud_target_smoke`.

[Detailed guide](../reference/game-hud.md) · [Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
