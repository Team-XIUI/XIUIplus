# Global settings

The top-right globe opens the shared language menu in Basic and Advanced settings. English and Japanese apply immediately and persist locally across profiles, presets and resets. Mouse, keyboard and controller share the header menu actions. [Localization](../../locales/README.md) owns the UTF-8 catalogs, presentation boundary and English fallback; client chat/resources and user content remain verbatim. Builds check shared UI message coverage and consistent numbered Hotbar captions. Focused check: `localization`.

Basic's Theme control and Global → Appearance → Theme select XI Theme or XIV 1.0 Theme. `/xiuiplus theme xiv1` and `/xiuiplus theme xi` use the same persisted preference (`appearance.theme`); `theme native` remains an alias. XIV 1.0 Theme applies embedded navy fabric and aged-metal frames, copper gauges, serif HUD text, transparent chat, feathered minimap and resource/target layout overlays. Switching themes keeps native Window Type, positions, scales and per-module styles intact. Native menus outside XIUI+ retain the game's Window Type. Focused check: `xiv1-theme`.

[First-load setup](../reference/setup.md) lives in `hud_runtime_setup.cpp`, with copy and shared state in `config/setup_wizard.hpp`. It previews modules through their normal renderer, saves choices through the current profile, and keeps completion/progress local to the installation. Existing installs enter only with `/xiuiplus setup`. Focused check: `setup-wizard`.

[Basic/Advanced and HUD scaling](../reference/hud-scaling.md) use `hud_runtime_basic.cpp`, `hud_runtime_scale.cpp` and the shared module scale controls. General remains the internal ID; Global is the visible category.

Basic settings compose the existing native panel, buttons, slider, percentage editor and profile controls. `config/basic_settings_layout.hpp` owns its responsive geometry; preset samples use the same theme and meter renderer as the applied styles. The compact scale row shares drawing and input geometry; controller focus targets only its visible controls. Attribution sits to the left of minimize in both settings headers. The minus button collapses the panel; plus restores it.

Settings start with Edit Mode OFF. `ConfigPreview` and `ModuleDisplayed` in `hud_runtime_visibility.cpp` separate explicit previews from live visibility across rendering, pointer controls and controller movement. The shared footer has an ON/OFF dot toggle; selecting Advanced settings previews the selected element. Hover editing remains available on visible modules. Preview state is transient. Focused check: `edit-mode`.

Global → Extra Settings → Follow moved module settings starts enabled. Turning it off keeps the current Advanced settings page while dragging or scaling HUD modules, including chat. Explicit sidebar and controller selections still navigate normally. The preference uses `general.follow_moved_module_settings` and participates in normal resets and profiles.

Basic and Advanced share footer geometry and rendering through `config_layout` and `DrawSettingsFooter`: profiles on the left, a settings-mode icon followed by Edit Mode at the far right. Advanced navigation and content reserve that space, including its minimum height. Classic and XIV restore their complete captures with XI Theme. XIVClassic restores Classic with XIV1.0 Theme and one combined All chat window. Reset defaults have their own capture, independent of these named presets.

Preset samples use isolated `party_module` instances configured once on entering Basic. `ConfigurePresetSample` loads Party from the same full captures; `RenderSample` draws one entry through the normal party renderer using the preset's own theme. These samples do not alter live Party settings. `config/hud_presets.hpp` owns names, commands, resource IDs and sample themes; `HudPresetOption` preserves existing action IDs around the Theme control. The `hud-presets` check covers all saved preferences and the independent reset baseline.

Configuration hover help resets before either settings view draws. Modal overlays and game-window focus loss suppress underlying tooltips; deliberate controller focus supplies its own contextual help.

Configuration dragging follows the search control's visible width, including its animation. Closing search releases the expanded field's former area; a collapsed title bar reserves only its visible header buttons.

Global uses the shared second navigation column for Appearance, Global Text, Config Text, Enabled Modules, Grid, Input and Visibility. Appearance opens window styling without another submenu. Controller Focus, HP Feedback and Native Menus live under Extras. Native Menus retains the `gamehud` module ID and saved visibility preferences. Attribution opens only from the top-right button and keeps its credit-page tabs. Preserve original topic IDs, appearance-tab selection, pointer/controller actions and preview space. Pages reserve top space only for visible tabs, category controls or previews.

Native Menus includes **Instant native macro bar**, off by default, saved as `game_hud.instant_macro_bar`. It removes the native Ctrl/Alt palette opening delay while native macros are enabled, even with every XIUI+ action bar disabled. Replace native macros pauses it without changing the saved choice. Visibility controls remain independent. The host verifies both delay branches before changing code, restores only owned bytes on disable/unload and preserves existing macrofix patches. Unsupported or changed code disables the setting. Focused checks: `xiui_native_hud_target_smoke --macros-only` and `xiui_actionbars_ui_smoke --input-ownership-only`.

Owns shared appearance, global/config typography, visibility preferences, input presentation and common focus styling.

XIV 1.0 background opacity defaults to 40% and is adjustable under Appearance → Window. It affects shared container backgrounds while retaining each module's opacity, frame and text preferences.

**Start in:** [general_module.cpp](../../src/runtime/modules/general/general_module.cpp), [global_text.cpp](../../src/runtime/config/global_text.cpp), [module_visibility.hpp](../../src/runtime/config/module_visibility.hpp), [hud_runtime_visibility.cpp](../../src/runtime/hud_runtime_visibility.cpp).

**Keep intact:** Global text changes only the chosen property across HUD profiles. Presentation mode must not change native device settings. Event visibility uses the complete native predicate, not its system-enable byte. Explicit Hide all releases focus; native Window Type remains authoritative.

**Focused check:** `./scripts/check.ps1 -Name global-text`, `input-mode` or `all-ui-visibility`, according to the change.

Installed data paths and legacy migration live in `settings/storage_paths.*`; see [installed data](../development.md#installed-data). Settings and theme are `config/XIUIplus/settings.ini` and `theme.ini`; profiles, inventories, sounds and diagnostics share that root. Focused check: `storage-paths`.

Profile menu imports use `hud_runtime_profiles.cpp` and `hud_runtime_lua_import.cpp`. [Lua profile import](../reference/lua-profile-import.md) documents discovery, reports and optional activation; `lua-profile-import` is its focused check. Lua scans and staging share the background `profile_transfer` worker. Normalization uses an isolated runtime with no game core or device. `settings_profiles::StageImport` writes the private group and resources; `CommitImport` validates and publishes it on the HUD thread. Cancellation destroys unpublished staging, and valid individual profiles can be selected from a setup that also contains damaged profiles.

Portable sharing exports self-contained `.xiuiplus` files and accepts older `.xiui` files through `settings/profile_file.*` and the native Save/Open dialogs in `hud_runtime_profiles.cpp`. Files contain the complete portable INI, profile name, format version and checksum; reads are bounded and writes publish atomically. `settings/profile_transfer.*` owns background file pickers, serialization, disk access and legacy decoding. Export captures live preferences once without saving/draining settings files. Workers own their data and never touch live HUD/game state (Lua staging may flush the thread-safe settings writers); the render thread takes completed results. Busy controls allow Cancel only. Closing, changing source/profile or unloading cancels pending work and discards late results; shutdown closes any picker and joins the worker. Pickers have no game-window owner, so Windows does not disable the game during selection. Invalid imports clear the pending preview, and creating the imported profile preserves existing profiles. Both files and legacy keys support up to 4 MiB of preferences.

`settings/profile_share.*` and `profile_share_delta.*` retain XIUI1/XIUI2 compatibility through **Paste old key**; new exports never generate keys. Keep the XIUI2 preset immutable. Focused check: `profile-sharing`, including current defaults, large file round trips, failed writes, damaged files, Unicode paths, legacy keys and shared controller navigation. Native file pickers require Windows pointer/keyboard interaction; opening them from controller focus does not establish controller-only file selection.

[Detailed guide](../reference/configuration.md) · [Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
