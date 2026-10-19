# Focused checks

Choose the smallest check for the boundary changed. The default workflow does not require the complete suite or visual/gameplay checks; Cody verifies those in game.

```powershell
./scripts/check.ps1 -List
./scripts/check.ps1 -Name minimap
./scripts/check.ps1 -Name config-ui -NoBuild
```

The wrapper builds only the selected target, then runs its catalogued arguments. `-NoBuild` reuses a current executable. [checks.json](checks.json) is the shared catalog for this wrapper and CMake's `xiui_focus_*` registrations. Checks marked `requires_arguments` need `-Arguments` with fixture paths and are not registered as unattended CTest cases. Do not run every entry for a routine change.

## Useful starting points

- Config controls/input: `config-ui`, `choice-arrows`, `element-tabs`, `input-mode`.
- Config geometry: `config-resize`, `config-sidebar`, `scrollbar`; lookup: `settings-search`.
- Text: `config-text`, `global-text`, `text-rasterization`.
- Preferences/profiles: `settings` (dedicated store executable), `defaults` (passes the shipped INI automatically), `profile-delete`, `party-presets`, `lua-profile-import` (release writer fixtures and isolated import persistence).
- Controller: `controller-input` (host adapter/normalization), `controller-config` (shared config actions), `cursor-ownership` (pointer lifetime). Synthetic checks do not establish physical-device/native-keyboard acceptance.
- Minimap: `minimap`, including named IDs, pinned edge toggles and burnt contour geometry.
- Other feature boundaries: `all-ui-visibility`, `target-rings`, `notification-feedback`, `heal-prediction`, `experience-model`. See `-List` and the dispatcher for the rest.

The dispatcher is [config_input_smoke.cpp](config_input_smoke.cpp). Fixture declarations live in [config_input_fixture.hpp](config_input_fixture.hpp), helpers in [config_check_support.cpp](config_check_support.cpp), and feature methods in `config_control_checks.cpp`, `config_navigation_checks.cpp`, `config_module_checks.cpp`, `config_profile_checks.cpp`, `config_render_checks.cpp` and `config_text_checks.cpp`. Existing specialized `*_checks.cpp` files remain separate. Keep new implementations with their feature rather than expanding the dispatcher.

## Fixtures and dedicated targets

Some flags accept optional installed DATs or captured fixtures. Without them they test deterministic local behavior, not native artwork or live data. Read the dispatcher branch and helper signature before supplying paths. `party-presets` accepts optional default/theme paths; `settings-presentation` requires an output Markdown path and is invoked by `scripts/update-docs.ps1`.

Dedicated targets outside the catalog remain in [cmake/tests.cmake](../cmake/tests.cmake). Examples include `xiui_chat_smoke`, `xiui_actionbars_ui_smoke`, `xiui_crossbar_input_smoke`, `xiui_satchel_ui_smoke`, `xiui_satchel_actions_smoke` and `xiui_satchel_data_smoke`. Inspect each executable's `main` for arguments before running it. To build a selected target:

```powershell
. ./scripts/common.ps1
Invoke-XiuiBuild -Targets xiui_chat_smoke
./bin/tests/xiui_chat_smoke.exe
```

The capture utility has an isolated encoding/filter/preservation check:

```powershell
. ./scripts/common.ps1
Invoke-XiuiBuild -Targets xiui_capture_defaults
./tests/capture_defaults_smoke.ps1 -Executable "$PWD/bin/tools/xiui_capture_defaults.exe"
```

Locked HUD pointer capture and native-menu coexistence have focused runtime checks. They feed the mouse block set by XIUI back into its handlers and cover popup/search ownership, dismissal, owned releases and preservation of a preexisting input block:

```powershell
. ./scripts/common.ps1
Invoke-XiuiBuild -Targets xiui_satchel_ui_smoke,xiui_actionbars_ui_smoke
./bin/tests/xiui_satchel_ui_smoke.exe --input-ownership-only
./bin/tests/xiui_actionbars_ui_smoke.exe --input-ownership-only
```

## Standalone packaging

`./scripts/package.ps1` runs `xiui_resource_smoke` and `xiui_single_dll_smoke` against the standalone DLL before publishing `dist/xiuinative.dll`. These cover resource integrity, Ashita exports, isolated initialization, embedded assets/defaults/theme/notices and saving. They are separate from broad feature checks. Use `-SkipBuild` only after that exact release has passed both checks; see [development](../docs/development.md) for installation and AgentLink confirmation.
