# XIUI+

XIUI+ is an in-client Final Fantasy XI HUD for Ashita v4, rendered with Direct3D8.

Download `XIUIplus.dll` from [GitHub Releases](https://github.com/Shuu-37/xiui-plugin/releases) (repository access required). Copy it into Ashita's `plugins` folder and run `/load XIUIplus`. Existing settings are preserved. Local packaging produces [dist/XIUIplus.dll](dist/XIUIplus.dll). See [the changelog](CHANGELOG.md) and [the user guide](docs/user-guide.md) for features and commands.

When upgrading from XIUI Native, unload `xiuinative`, replace its DLL with `XIUIplus.dll`, and update any startup `/load xiuinative` line to `/load XIUIplus`. The next plugin load moves existing data into `config/XIUIplus/`, preserving settings, profiles, inventories and sounds. Existing destination files take priority; conflicts are archived under `legacy/`. `/xiuiplus` is the main command; `/xiuip`, `/xiui+`, `/xiui` and earlier aliases still work. New profile exports use `.xiuiplus`; existing `.xiui` files and XIUI1/XIUI2 share keys remain importable.

## Working on XIUI+

Start with [AGENTS.md](AGENTS.md), then [the task-to-file guide](docs/README.md). Read only the foundations and feature references relevant to the task.

- [Development](docs/development.md): toolchain, standalone install, AgentLink and isolated hot reload.
- [Shared foundations](docs/foundations.md): rendering, text, controls, containers and persistence contracts.
- [Module notes](docs/modules/README.md): short agent overviews, source owners and focused checks.
- [Input ownership](docs/reference/input.md): mouse, keyboard, controller and remaining hardware limits.
- [Focused checks](tests/README.md): build targets, arguments and required fixtures.
- [Assets and generators](assets/README.md): generated files, source provenance and notices.
- [Detailed references](docs/reference/README.md) and [archived records](docs/archive/README.md): current deep dives versus historical evidence.

## Build and install

Windows, Visual Studio C++ with x86 tools, CMake/Ninja, Python and the Ashita v4 SDK are required. While XIUI+ is unloaded:

```powershell
./scripts/build_all.ps1
```

Normal builds install one DLL in the selected Phoenix setup. `ASHITA4_SDK_PATH` selects headers and libraries; it does not choose the running game. [Development](docs/development.md) explains explicit install paths and the separate two-DLL development mode.

## Structure

- `src/host/`: Ashita callbacks, snapshots, native integration and runtime loading.
- `src/shared/`: host/runtime ABI and shared input contracts.
- `src/runtime/modules/`: HUD feature models, sources, configuration and rendering.
- `src/runtime/config/`, `input/`, `ui/`: shared configuration, input and rendering foundations.
- `src/runtime/settings/`: persistence, profiles and portable preference boundaries.
- `cmake/`: source groups, resource generation, runtime targets and focused checks.
- `docs/`: current guides, feature records and dated historical evidence.

Party artwork is bundled from [XIVParty](assets/jobs/README.md); other artwork/data provenance is recorded in [the asset index](assets/README.md). Window themes and native item artwork are read from the installed client.
