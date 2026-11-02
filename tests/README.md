# Focused checks

Choose the smallest check for the boundary changed. The default workflow does not require the complete suite or visual/gameplay checks; Cody verifies those in game.

```powershell
./scripts/check.ps1 -List
./scripts/check.ps1 -Name minimap
./scripts/check.ps1 -Name config-ui -NoBuild
```

The wrapper builds only the selected target, then runs its catalogued arguments. `-NoBuild` reuses a current executable. [checks.json](checks.json) is the shared catalog for this wrapper and CMake's `xiui_focus_*` registrations. Checks marked `requires_arguments` need `-Arguments` with fixture paths and are not registered as unattended CTest cases. Do not run every entry for a routine change.

## Useful starting points

- Global XIV 1.0 appearance: `xiv1-theme` (persistence, native Window Type isolation, controller access and text fade preservation).

- Explicit Edit Mode: `edit-mode` (live visibility, selected previews, hover controls, hidden input targets and shared toggle).
- Full / Compact typing: `xiui_chat_smoke --full-keyboard-only` (first-key composition, native draft handoff, shortcut ownership, numpad movement and editor isolation).
- First-load setup and native chat defaults: `setup-wizard` (navigation, exactly-once activation, native fallback, local progress and saved chat preservation).
- HUD scale and Basic/Advanced: `hud-scaling` (geometry, shared gestures, representative input and preset preservation).
- Complete captured setups: `hud-presets` (all preferences, Party-only Classic differences, independent reset baseline).
- Edit-mode module resets: `module-menu` (shared dropdown, owned clicks, scoped resets and Cancel-first confirmation).
- Config controls/input: `config-ui`, `choice-arrows`, `element-tabs`, `input-mode`.
- Config geometry: `config-resize`, `config-sidebar`, `scrollbar`; lookup: `settings-search`.
- Config navigation memory: `config-memory` (mode, last page, scroll and window placement through close/reopen and reload).
- Text: `config-text`, `global-text`, `text-rasterization`.
- Installed directories and migration: `storage-paths` (byte preservation, inventories, source archives, conflicts, shared resources and interrupted-load retry).
- Preferences/profiles: `settings` (dedicated store executable), `defaults` (passes the shipped INI automatically), `profile-sharing` (background files, cancellation, captured snapshots, corruption, failed writes, Unicode paths, controller routes, legacy keys and isolated imports), `profile-delete`, `party-presets`, `lua-profile-import` (release writer fixtures, strict data parsing, mixed damaged/valid discovery, background scans, staged cancellation and isolated persistence).
- Controller: `controller-input` (host adapter/normalization), `controller-config` (shared config actions), `cursor-ownership` (pointer lifetime). Synthetic checks do not establish physical-device/native-keyboard acceptance.
- Window focus and chat: `window-input` (hex editing, foreground draft/focus suspension, F → Enter over the combat menu, native full-log fallback and menu handoff); `xiui_keyboard_suppression_smoke` covers DirectInput reads before WM delivery and returning a fresh Enter to gameplay. `controller-input` also checks native cycle press/hold/release in both backends.
- Minimap: `minimap`, including named IDs, pinned edge toggles and burnt contour geometry.
- Fishing: `fishing` (module, Extras, controller settings and persistence), `native-fishing` (validated stamina reads with party suppression active).
- Other feature boundaries: `all-ui-visibility`, `target-rings`, `notification-feedback`, `heal-prediction`, `experience-model`. See `-List` and the dispatcher for the rest.
- Target of Target theme defaults: `target-recipient-theme` (XIV 1.0 stacking, XI inline layout, explicit configuration choices and persistence).
- World ring cameras: `target-ring-camera` exercises the host on an isolated Direct3D device with fixed-function and terrain-shader cameras, scene-end/copy boundaries, early-label rejection, frame lifetime and opt-in shader report saving/grouping/automatic completion.

The dispatcher is [config_input_smoke.cpp](config_input_smoke.cpp). Fixture declarations live in [config_input_fixture.hpp](config_input_fixture.hpp), helpers in [config_check_support.cpp](config_check_support.cpp), and feature methods in `config_control_checks.cpp`, `config_navigation_checks.cpp`, `config_module_checks.cpp`, `config_profile_checks.cpp`, `config_render_checks.cpp` and `config_text_checks.cpp`. Existing specialized `*_checks.cpp` files remain separate. Keep new implementations with their feature rather than expanding the dispatcher.

## Fixtures and dedicated targets

Some flags accept optional installed DATs or captured fixtures. Without them they test deterministic local behavior, not native artwork or live data. Read the dispatcher branch and helper signature before supplying paths. `party-presets` accepts optional default/theme paths; `settings-presentation` requires an output Markdown path and is invoked by `scripts/update-docs.ps1`.

Startup discovery: `xiui_native_pattern_smoke` compares unique matches with the SDK, checks guarded memory boundaries and reports a module-sized search comparison. `settings` covers snapshot parsing against Windows INI behavior and reports loading 2,700 sections versus repeated Windows section reads.

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

Locked HUD pointer capture and native-menu coexistence have focused runtime checks. They feed the mouse block set by XIUI+ back into its handlers and cover popup/search ownership, dismissal, owned releases and preservation of a preexisting input block:

```powershell
. ./scripts/common.ps1
Invoke-XiuiBuild -Targets xiui_satchel_ui_smoke,xiui_actionbars_ui_smoke
./bin/tests/xiui_satchel_ui_smoke.exe --input-ownership-only
./bin/tests/xiui_actionbars_ui_smoke.exe --input-ownership-only
./bin/tests/xiui_actionbars_ui_smoke.exe --macro-editor-only
```

## Standalone packaging

`./scripts/package.ps1` runs `xiui_resource_smoke` and `xiui_single_dll_smoke` against the standalone DLL before publishing `dist/XIUIplus.dll`. These cover resource integrity, Ashita exports, isolated initialization, embedded assets/defaults/theme/notices and saving. They are separate from broad feature checks. Use `-SkipBuild` only after that exact release has passed both checks; see [development](../docs/development.md) for installation and AgentLink confirmation.
