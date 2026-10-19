# General settings

Owns shared appearance, global/config typography, visibility preferences, input presentation and common focus styling.

**Start in:** [general_module.cpp](../../src/runtime/modules/general/general_module.cpp), [global_text.cpp](../../src/runtime/config/global_text.cpp), [module_visibility.hpp](../../src/runtime/config/module_visibility.hpp), [hud_runtime_visibility.cpp](../../src/runtime/hud_runtime_visibility.cpp).

**Keep intact:** Global text changes only the chosen property across HUD profiles. Presentation mode must not change native device settings. Event visibility uses the complete native predicate, not its system-enable byte. Explicit Hide all releases focus; native Window Type remains authoritative.

**Focused check:** `./scripts/check.ps1 -Name global-text`, `input-mode` or `all-ui-visibility`, according to the change.

[Detailed guide](../reference/configuration.md) · [Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
