# Development and maintenance

Run commands from the repository root. Use Visual Studio's x86 C++ tools, CMake/Ninja and an Ashita v4 SDK. `scripts/common.ps1` resolves local tools and `ASHITA4_SDK_PATH`; the SDK location does not identify the running game. Python is needed for resource packing and documentation generation.

## Build and install

Normal installs use one standalone `plugins/XIUIplus.dll`. `./scripts/package.ps1` builds `XIUIplus_release` and its isolated startup/resource/compatibility checks, runs them, then publishes `dist/XIUIplus.dll`. Use `-SkipBuild` only when that same release has already passed those checks. Ordinary builds and commit/push requests retain the current `VERSION`. Change or publish a version only when Cody explicitly requests it.

For targeted work:

```powershell
. ./scripts/common.ps1
Invoke-XiuiBuild -Targets xiui_config_input_smoke
./scripts/check.ps1 -Name minimap -NoBuild
```

Use [the focused check guide](../tests/README.md) to select checks. Runtime changes require a standalone build/install under [AGENTS.md](../AGENTS.md), preserving current settings and positions. The active development install is currently `C:/Games/PhoenixXI-dev/PhoenixXI`; verify it through AgentLink before publishing:

```powershell
& ../phoenixlighting/scripts/game.ps1 '?state','?loaded XIUIplus'
```

Read the returned install path. AgentLink lives in the neighboring plugin repository, not this SDK. The client may be `phoenix-loader` with the character's window title; a timeout or missing `pol` process does not prove the game is closed. Retry and inspect the actual game window/log if needed.

`& ../phoenixlighting/scripts/game.ps1 -Screenshot` captures the current game frame at full resolution and returns the PNG path in `files`. It can also follow diagnostic steps; open the returned image to inspect actual rendering rather than relying on loaded/visible status alone.

After packaging, save and unload before replacing the loaded DLL. Substitute the confirmed path if it differs:

```powershell
& ../phoenixlighting/scripts/game.ps1 '/xiuiplus save sync','wait 200'
# Require the completed-save reply before unloading.
& ../phoenixlighting/scripts/game.ps1 '/unload XIUIplus','wait 800','?loaded XIUIplus'
. ./scripts/common.ps1
Install-XiuiStandalone -Source "$PWD/dist/XIUIplus.dll" -InstallPath 'C:/Games/PhoenixXI-dev/PhoenixXI'
& ../phoenixlighting/scripts/game.ps1 '/load XIUIplus','wait 1500'
& ../phoenixlighting/scripts/game.ps1 '?loaded XIUIplus','/xiuiplus status','wait 400'
```

Require unload confirmation before replacement and a separate loaded/status response afterward. A queued load alone is not success. Older builds without `save sync` need `/xiuiplus save` and an observed completed file write before unload. Do not copy a default INI/theme over existing user files during ordinary installation.

`/xiuiplus startup` reports measured initialization phases, device resources and the first HUD frame. The plugin total includes runtime creation; the runtime total and its phases are nested measurements. These timings start inside the plugin, after Windows loads the DLL. Settings use a single indexed file snapshot per load (`settings/settings_store.cpp`). Native signature discovery uses `src/shared/native_pattern.hpp`, retaining unique-match checks and all caller-specific address validation.

`build-all.ps1` packages/installs standalone by default; `-SkipInstall` leaves its artifact uninstalled only when explicitly requested. It does not perform the AgentLink unload/reload steps itself.

## Installed data

`settings/storage_paths.*` owns `config/XIUIplus/`: `settings.ini`, `theme.ini`, `profiles/`, `inventories/`, `sounds/`, user `icons/` and optional `diagnostics/`. `PrepareStorage` runs before any runtime settings writer starts. It moves the previous `config/xiuinative/` master directory, older `config/xiuinative*` files/directories, development plugin notices and `logs/xiui` reports into that root. Existing destinations win; conflicting originals move to `legacy/`. Same-volume moves do not replace destinations. Partial failures abort initialization before settings are loaded and are safe to retry. A per-install mutex serializes migration across clients; linked paths that escape the data root are rejected.

`storage.version` records the completed layout migration and avoids rereading profile sound references on every startup. Only XIUI-generated WAVs move out of shared `config/sounds`; referenced custom WAVs are copied, and unrelated files remain there. Lua setup discovery still reads `config/addons/xiui`. Profile IDs, character associations, saved values and inventory files retain their contents.

The capture script reads `config/XIUIplus/settings.ini` and `theme.ini`, with a fallback for older installed layouts or an explicitly selected saved snapshot. Repository inputs are `config/XIUIplus-defaults.ini` and `config/XIUIplus-theme.ini`. Installers rely on the runtime to migrate and seed data; ordinary binary installation does not publish a theme over user settings. The standalone installer archives the previous `xiuinative.dll` under `config/XIUIplus/legacy/plugins/`, leaving one active plugin DLL. First upgrades must unload `xiuinative` and update any startup load command to `XIUIplus`.

License notices remain embedded in the DLL. Startup does not extract them into the config directory; known generated filenames in the migrated licenses/legacy folders are removed across versions, and unrelated user-added files are preserved.

Focused check: `./scripts/check.ps1 -Name storage-paths`. Standalone packaging also checks the new first-load locations.

## Optional isolated host/runtime development

The two-DLL pair is only for an explicitly selected isolated development install. `./scripts/build-all.ps1 -Development -InstallPath <isolated-Phoenix-path>` installs it. `./scripts/dev.ps1 -InstallPath <isolated-Phoenix-path>` watches it. Never apply these to the normal active standalone install without selecting that mode.

`-RuntimeOnly` requires `-Development`; ABI changes require matching binaries and drained settings writers. `-SkipDevelopmentTests` (legacy alias `-SkipTests`) applies only to development checks. Standalone packaging retains its required startup check. Different feature definitions and MSVC runtimes prevent blindly sharing all HUD objects.

## Capture current defaults

Capture only when requested. This changes the baseline embedded for new installs and explicit reset; it does not reset the active client or change the immutable sharing preset.

```powershell
./scripts/capture-defaults.ps1
./scripts/check.ps1 -Name defaults
```

The script verifies loaded XIUI+ with AgentLink, requests `/xiuiplus save sync`, and requires its completed-save reply. It snapshots the saved INI and theme, backs up the previous baseline under `out/defaults-capture-*`, then atomically publishes each baseline file. Older builds must be updated before live capture. The companion `config/XIUIplus-defaults.capture.json` records source type, timestamp, counts and hashes after each capture. The saved `layout.viewport` records the game viewport used for those positions; keep it with the captured preset so first loads can fit other resolutions.

The C++ utility uses `settings_store::VisitPreferences`, the same boundary as portable settings, to omit session, profile metadata and local controller-device data. It also removes retired container demos and empty sections while preserving empty preference values, disabled states, Unicode and positions. UTF-8 and UTF-16LE saved files are supported.

Basic's named presets embed `config/hud-xiv.ini` and `config/hud-classic.ini` independently of the latest reset defaults. Refresh a named preset only when requested, using `-OutputPath` for that preset. To author a matching pair, capture the XIV setup first, apply `/xiuiplus party preset classic`, then capture Classic with `-OutputPath "$PWD/config/hud-classic.ini" -ThemeOutputPath "$PWD/out/hud-classic-theme.ini"`. Confirm that only Party preferences differ before rebuilding. The October 20 source captures had 15,705 preferences each, with 79 Party fields changed and no differences elsewhere. The later Classic spacing correction amends only Party layout/offset preferences and adds its migration marker; the companion metadata records that amendment and the updated hash.

For an explicitly selected saved snapshot:

```powershell
./scripts/capture-defaults.ps1 -FromSaved -SettingsPath '<saved.ini>' -ThemePath '<saved-theme.ini>'
```

Use `-OutputPath` and `-ThemeOutputPath` to capture elsewhere. `-SkipBuild` reuses the existing utility. Capture does not package/install: check the changed baseline, then build the standalone DLL normally. Never restore older settings over current user changes as part of cleanup.

The November 1, 2026 live XIV1.0 setup is captured in both reset defaults and the independent XIVClassic preset. Their companion metadata identifies the shared source snapshot. Classic and XIV remain separate XI Theme captures.

## GitHub releases

DLL releases live in the public [xiuiplus-releases repository](https://github.com/Team-XIUI/xiuiplus-releases/releases). Its independent `main` contains public download instructions only; development source and history remain in private `Shuu-37/XIUIplus`. The development checkout's `origin` stays private. Use `--repo Team-XIUI/xiuiplus-releases` explicitly for every GitHub release command.

### Versions

`VERSION` holds one line: the release name exactly as players see it. It is standard `MAJOR.MINOR.PATCH` with an optional channel, such as `0.2.1-alpha`. Nothing else is written by hand:

| Shown where | Value for `0.2.1-alpha` | Derived by |
| --- | --- | --- |
| Source tag and public release tag | `v0.2.1-alpha` | `scripts/release.ps1` |
| Public release title | `XIUI+ 0.2.1-alpha` | `scripts/release.ps1` |
| Startup message, Windows file details | `0.2.1-alpha` | `CMakeLists.txt` |
| Windows numeric file version | `0.2.1.0` | `config/version.rc.in` |
| Ashita plugin list | `0.21` | `src/shared/version.hpp.in` |

Fixes raise PATCH (`0.2.1` → `0.2.2`); feature releases raise MINOR and reset PATCH (`0.2.2` → `0.3.0`). Always write all three numbers. CMake fails the build on any other shape, such as `0.2` or `0.21`.

Ashita can only show one number with two decimals, so its plugin list shows `major + minor / 10 + patch / 100`: `0.2.1` appears as `0.21`. That line is the only place the name is abbreviated, and it carries into the next digit once MINOR or PATCH passes 9. Releases before `0.2.1` used other shapes (`0.2`, `0.1.41`); their tags stay as published.

Use the version Cody requests. Otherwise raise PATCH from the latest published release and keep its channel. Do not bump again when `VERSION` already names an unpublished release. The channel has no counter: `-alpha`, never `-alpha.1`.

### Publishing

Preserve the shipped defaults unless a capture was requested. Set `VERSION`, move the `CHANGELOG.md` notes under that version, write the player-facing notes file, then commit and push private `main`. Publish with:

```powershell
./scripts/release.ps1 -NotesFile out/release-notes.md
```

It refuses to run with uncommitted or unpushed changes, an existing tag or release of that version, or a DLL whose version differs from `VERSION`. It runs `package.ps1` and its checks, tags the source commit in the private repository, creates the public release with `dist/XIUIplus.dll` and `dist/SHA256SUMS.txt`, and verifies the uploaded hash. `-WhatIf` stops before tagging. Every release is marked Latest; the channel is part of the name only. Never push the source tag or private branch into the public repository.

GitHub creates the public release tag on the public repository's own `main`; it shares the version name with the private source tag, not its Git history. Use a new tag for each release. Write short, player-facing patch notes for every new feature and fix since the last published public release. Compare its matching private source tag with the intended release commit; an unpublished development version or changelog heading is not the baseline. The notes file is explicit because the public repository has no development PR history from which to generate notes.

Preserve older public releases, their assets and public tags, including alpha versions, so users can download previous builds. Publishing a new release must not delete, overwrite or clean up older releases or tags. Retain private source tags too. Any historical release removal requires a separate explicit user request. The public README links to the Releases page, without a version-specific download link.

The public checkout is the sibling `../xiuiplus-releases/`, with its own `main` and `origin`. DLLs and checksums are release assets, not committed files. Existing private releases are historical copies; public downloads and future publications use `xiuiplus-releases`.

## Update documentation

```powershell
./scripts/update-docs.ps1
# With an already-current config check executable:
./scripts/update-docs.ps1 -SkipBuild
```

This exports the [settings inventory](reference/settings-inventory.md) from option metadata and checks local file links. It does not run gameplay checks or regenerate assets. `python scripts/check-doc-links.py` checks links alone; local agent folders and vendored upstream documentation are outside its scan. Completed plans and audit records remain available in Git history.
