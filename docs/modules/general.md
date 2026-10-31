# Global settings

Basic's Theme control and Global → Appearance → Theme select XI Theme or XIV 1.0 Theme. `/xiui theme xiv1` and `/xiui theme xi` use the same persisted preference (`appearance.theme`); `theme native` remains an alias. XIV 1.0 Theme applies embedded navy fabric and aged-metal frames, copper gauges, serif HUD text, transparent chat, feathered minimap and resource/target layout overlays. Switching themes keeps native Window Type, positions, scales and per-module styles intact. Native menus outside XIUI retain the game's Window Type. Focused check: `xiv1-theme`.

[First-load setup](../reference/setup.md) lives in `hud_runtime_setup.cpp`, with copy and shared state in `config/setup_wizard.hpp`. It previews modules through their normal renderer, saves choices through the current profile, and keeps completion/progress local to the installation. Existing installs enter only with `/xiui setup`. Focused check: `setup-wizard`.

[Basic/Advanced and HUD scaling](../reference/hud-scaling.md) use `hud_runtime_basic.cpp`, `hud_runtime_scale.cpp` and the shared module scale controls. General remains the internal ID; Global is the visible category.

Basic settings compose the existing native panel, buttons, slider, percentage editor and profile controls. `config/basic_settings_layout.hpp` owns its responsive geometry; preset samples use the same theme and meter renderer as the applied styles. The compact scale row shares drawing and input geometry; controller focus targets only its visible controls. The minus button collapses the panel; plus restores it.

Settings start with Edit Mode OFF. `ConfigPreview` and `ModuleDisplayed` in `hud_runtime_visibility.cpp` separate explicit previews from live visibility across rendering, pointer controls and controller movement. The shared footer has an ON/OFF dot toggle; selecting Advanced settings previews the selected element. Hover editing remains available on visible modules. Preview state is transient. Focused check: `edit-mode`.

Basic and Advanced share footer geometry and rendering through `config_layout` and `DrawSettingsFooter`: profiles on the left, a settings-mode icon followed by Edit Mode at the far right. Advanced navigation and content reserve that space, including its minimum height. Classic and XIV restore their complete captures with XI Theme. XIVClassic restores Classic with XIV1.0 Theme and one combined All chat window. Reset defaults have their own capture, independent of these named presets.

Preset samples use isolated `party_module` instances configured once on entering Basic. `ConfigurePresetSample` loads Party from the same full captures; `RenderSample` draws one entry through the normal party renderer using the preset's own theme. These samples do not alter live Party settings. `config/hud_presets.hpp` owns names, commands, resource IDs and sample themes; `HudPresetOption` preserves existing action IDs around the Theme control. The `hud-presets` check covers all saved preferences and the independent reset baseline.

Configuration hover help resets before either settings view draws. Modal overlays and game-window focus loss suppress underlying tooltips; deliberate controller focus supplies its own contextual help.

Configuration dragging follows the search control's visible width, including its animation. Closing search releases the expanded field's former area; a collapsed title bar reserves only its visible header buttons.

Global uses the shared second navigation column for Appearance, Global Text, Config Text, Enabled Modules, Grid, Input and Visibility. Appearance opens window styling without another submenu. Controller Focus, HP Feedback and Native Menus live under Extras. Native Menus retains the `gamehud` module ID and saved visibility preferences. Attribution opens only from the top-right button and keeps its credit-page tabs. Preserve original topic IDs, appearance-tab selection, pointer/controller actions and preview space. Pages reserve top space only for visible tabs, category controls or previews.

Owns shared appearance, global/config typography, visibility preferences, input presentation and common focus styling.

**Start in:** [general_module.cpp](../../src/runtime/modules/general/general_module.cpp), [global_text.cpp](../../src/runtime/config/global_text.cpp), [module_visibility.hpp](../../src/runtime/config/module_visibility.hpp), [hud_runtime_visibility.cpp](../../src/runtime/hud_runtime_visibility.cpp).

**Keep intact:** Global text changes only the chosen property across HUD profiles. Presentation mode must not change native device settings. Event visibility uses the complete native predicate, not its system-enable byte. Explicit Hide all releases focus; native Window Type remains authoritative.

**Focused check:** `./scripts/check.ps1 -Name global-text`, `input-mode` or `all-ui-visibility`, according to the change.

Profile menu imports use `hud_runtime_profiles.cpp` and `hud_runtime_lua_import.cpp`. [Lua profile import](../reference/lua-profile-import.md) documents discovery, reports and optional activation; `lua-profile-import` is its focused check.

Portable sharing exports self-contained `.xiui` files through `settings/profile_file.*` and the native Save/Open dialogs in `hud_runtime_profiles.cpp`. Files contain the complete portable INI, profile name, format version and checksum; reads are bounded and writes publish atomically. Invalid imports clear the pending preview, and creating the imported profile preserves existing profiles. Both files and legacy keys support up to 4 MiB of preferences.

`settings/profile_share.*` and `profile_share_delta.*` retain XIUI1/XIUI2 compatibility through **Paste old key**; new exports never generate keys. Keep the XIUI2 preset immutable. Focused check: `profile-sharing`, including current defaults, large file round trips, failed writes, damaged files, Unicode paths, legacy keys and shared controller navigation. Native file pickers require Windows pointer/keyboard interaction; opening them from controller focus does not establish controller-only file selection.

[Detailed guide](../reference/configuration.md) · [Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
