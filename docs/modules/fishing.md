# Fishing

**Extras → Fishing** displays a hooked fish's remaining stamina, including 0%, independently of native party visibility. Enabled by default, it previews in edit mode and saves its position, width, meter style and label/percentage text profiles. New positions adapt to the current viewport.

**Start in:** [fishing_module.cpp](../../src/runtime/modules/fishing/fishing_module.cpp), [native_hud_controller.cpp](../../src/host/native_hud_controller.cpp), [runtime_api.hpp](../../src/shared/runtime_api.hpp).

The host reads the native party owner's `+0x3A` percentage only after validating its layout and primitive attachment. Invalid/inactive values, zoning and logout produce no bar. Both fishing update paths maintain this field without drawing the native party window. Never use the alternate party preview flag or SDK bite timers as stamina. Fishing keys/controller actions remain native; configuration and movement use the shared controls.

**Focused checks:** `xiui_native_hud_target_smoke --fishing-only` and `xiui_config_input_smoke --fishing-only`.
