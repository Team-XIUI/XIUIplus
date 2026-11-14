# Developer documentation

XIUI+ has three kinds of developer docs:

- **This page**: how to build, and where things live.
- **[Foundations](foundations.md)**: the shared rules every HUD module follows (text, containers, bars, settings, input, persistence).
- **[Reference](reference/README.md)**: one guide per feature, plus guides for the systems they share (input, configuration, settings).

Players should read the [user guide](user-guide.md) instead.

## Build

You need Windows, Visual Studio with the x86 C++ tools, CMake, Ninja, Python and the Ashita v4 SDK. Set `ASHITA4_SDK_PATH` to the SDK folder. Run commands from the repository root.

```powershell
./scripts/package.ps1                 # build and check dist/XIUIplus.dll
./scripts/build-all.ps1               # package, then install into a game folder
./scripts/check.ps1 -List             # list focused checks
./scripts/check.ps1 -Name minimap     # build and run one check
```

`build-all.ps1` installs into the folder given by `-InstallPath` or `XIUI_INSTALL_PATH`. Unload XIUI+ in game before replacing the DLL; your settings and positions are kept.

To build a single target:

```powershell
. ./scripts/common.ps1
Invoke-XiuiBuild -Targets xiui_config_input_smoke
```

See [focused checks](../tests/README.md) for choosing a check. `./scripts/update-docs.ps1` regenerates the [settings inventory](reference/settings-inventory.md) and checks doc links.

## Versions

`config/VERSION` holds the release name on one line: `MAJOR.MINOR.PATCH` with an optional channel, such as `0.2.1-alpha`. Everything shown to players derives from it, and CMake rejects any other shape. Fixes raise PATCH; feature releases raise MINOR and reset PATCH.

## Where things live

Paths without a prefix are relative to `src/runtime/`.

| Area | Start in |
|---|---|
| A HUD feature | its [feature guide](reference/README.md), then `modules/<feature>/` |
| Shared drawing and text | `design_system.*`, `ui/`, `modules/module.*` |
| Settings window | `config/config_session.hpp`, `hud/hud_runtime_config*.cpp`, `config/config_controls.*`, `config/option_layout.hpp` |
| Input | `src/host/controller_input.*`, `src/shared/controller_input.hpp`, `input/`, `hud/hud_runtime_controller*.cpp`; native mouse and key dispatch starts in `src/host/plugin.cpp` |
| Settings and profiles | `settings/settings_store.*`, `settings/settings_profiles.*`, `hud/hud_runtime_profiles.cpp` |
| Installed data (`config/XIUIplus/`) | `settings/storage_paths.*` |
| Action imports | `modules/actionbars/action_import.*`, `settings/lua_data.*`; see [importing](reference/actionbars.md#importing) |
| Native hooks and plugin lifetime | `src/host/plugin.*`, `src/host/runtime_host.*`, `src/shared/runtime_api.hpp`, `runtime_exports.cpp` |
| Build and embedded resources | root `CMakeLists.txt`, `cmake/`, [assets/](../assets/), `scripts/assets/` |
| Localization | [locales/](../locales/README.md) |

Other shared guides: [input ownership](reference/input.md), [controller support](reference/controller-support.md), [configuration](reference/configuration.md), [settings organization](reference/settings.md) and [Ashita compatibility](reference/ashita-compatibility.md).

## Keeping docs current

Update the owning guide when behavior or source ownership changes. Put rules that span modules in [foundations](foundations.md). Edit generated documentation through its generator.
