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

`build_all.ps1` packages/installs standalone by default; `-SkipInstall` leaves its artifact uninstalled only when explicitly requested. It does not perform the AgentLink unload/reload steps itself.

## Installed data

`settings/storage_paths.*` owns `config/XIUIplus/`: `settings.ini`, `theme.ini`, `profiles/`, `inventories/`, `sounds/` and optional `diagnostics/`. `PrepareStorage` runs before any runtime settings writer starts. It moves the previous `config/xiuinative/` master directory, older `config/xiuinative*` files/directories, development plugin notices and `logs/xiui` reports into that root. Existing destinations win; conflicting originals move to `legacy/`. Same-volume moves do not replace destinations. Partial failures abort initialization before settings are loaded and are safe to retry. A per-install mutex serializes migration across clients; linked paths that escape the data root are rejected.

`storage.version` records the completed layout migration and avoids rereading profile sound references on every startup. Only XIUI-generated WAVs move out of shared `config/sounds`; referenced custom WAVs are copied, and unrelated files remain there. Lua setup discovery still reads `config/addons/xiui`. Profile IDs, character associations, saved values and inventory files retain their contents.

The capture script reads `config/XIUIplus/settings.ini` and `theme.ini`, with a fallback for older installed layouts or an explicitly selected saved snapshot. Repository inputs are `config/XIUIplus-defaults.ini` and `config/XIUIplus-theme.ini`. Installers rely on the runtime to migrate and seed data; ordinary binary installation does not publish a theme over user settings. The standalone installer archives the previous `xiuinative.dll` under `config/XIUIplus/legacy/plugins/`, leaving one active plugin DLL. First upgrades must unload `xiuinative` and update any startup load command to `XIUIplus`.

License notices remain embedded in the DLL. Startup no longer extracts them into the config directory; known generated filenames in the migrated licenses/legacy folders are removed across versions, and unrelated user-added files are preserved.

Focused check: `./scripts/check.ps1 -Name storage-paths`. Standalone packaging also checks the new first-load locations.

## Optional isolated host/runtime development

The two-DLL pair is only for an explicitly selected isolated development install. `./scripts/build_all.ps1 -Development -InstallPath <isolated-Phoenix-path>` installs it. `./scripts/dev.ps1 -InstallPath <isolated-Phoenix-path>` watches it. Never apply these to the normal active standalone install without selecting that mode.

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

For every new release, increment the furthest-right numeric component in root `VERSION`: `0.1.2` → `0.1.3` (and `0.1.9` → `0.1.10`). Keep `VERSION` numeric for CMake/Windows. Alpha tags use the fixed `-alpha` suffix: `v0.1.2-alpha` → `v0.1.3-alpha`. Do not add or increment an alpha counter such as `-alpha.1`.

Generate all version fields from `VERSION`, including Ashita's `GetVersion()`. Ashita displays a numeric compatibility value with two decimals: `major + minor / 10 + patch / 100`, so `0.1.3` displays as `0.13`. The startup message and Windows file/product metadata keep the complete semantic version. The standalone packaging check compares the loaded plugin's displayed value with its Windows version resource.

Preserve the shipped defaults unless a capture was requested. Run `./scripts/package.ps1` and require all packaging checks to pass. Commit and push the intended source on private `main`, then create and push an annotated source tag at that commit. Never push that source tag or private branch into the public repository.

Upload only `dist/XIUIplus.dll` and the generated `dist/SHA256SUMS.txt`; no companion runtime or external assets are needed. With the private source tag pushed and public release notes written:

```powershell
gh release create v0.1.2-alpha dist/XIUIplus.dll dist/SHA256SUMS.txt --repo Team-XIUI/xiuiplus-releases --target main --prerelease --latest=false --title 'XIUIplus 0.1.2-alpha' --notes-file out/release-notes.md
```

GitHub creates the public release tag on the public repository's own `main`; it shares the version name with the private source tag, not its Git history. Use a new tag for each release. Stable releases omit the channel suffix and prerelease flags and use `--latest`. Write public notes from the intended changes and known limitations; the public repository has no development PR history from which to generate them. Verify uploaded asset hashes and release status after publication.

Keep only the newest public release, including when it is an alpha. After publishing and verifying its assets, delete older public releases and their public tags with `gh release delete <old-tag> --repo Team-XIUI/xiuiplus-releases --yes --cleanup-tag`. Retain private source tags. The public README links to the Releases page, without a version-specific download link.

The public checkout is the sibling `../xiuiplus-releases/`, with its own `main` and `origin`. DLLs and checksums are release assets, not committed files. Existing private releases are historical copies; public downloads and future publications use `xiuiplus-releases`.

## Update documentation

```powershell
./scripts/update-docs.ps1
# With an already-current config check executable:
./scripts/update-docs.ps1 -SkipBuild
```

This exports the [settings inventory](reference/settings-inventory.md) from option metadata and checks local file links. It does not run gameplay checks or regenerate assets. `python scripts/check-doc-links.py` checks links alone; local agent folders and vendored upstream documentation are outside its scan. Completed plans and audit records remain available in Git history.
