# Development and maintenance

Run commands from the repository root. Use Visual Studio's x86 C++ tools, CMake/Ninja and an Ashita v4 SDK. `scripts/common.ps1` resolves local tools and `ASHITA4_SDK_PATH`; the SDK location does not identify the running game. Python is needed for resource packing and documentation generation.

## Build and install

Normal installs use one standalone `plugins/xiuinative.dll`. `./scripts/package.ps1` builds `xiuinative_release` and its isolated startup/resource checks, runs them, then publishes `dist/xiuinative.dll`. Use `-SkipBuild` only when that same release has already passed those checks. Change root `VERSION` only when cutting a version.

For targeted work:

```powershell
. ./scripts/common.ps1
Invoke-XiuiBuild -Targets xiui_config_input_smoke
./scripts/check.ps1 -Name minimap -NoBuild
```

Use [the focused check guide](../tests/README.md) to select checks. Runtime changes require a standalone build/install under [AGENTS.md](../AGENTS.md), preserving current settings and positions. The active development install is currently `C:/Games/PhoenixXI-dev/PhoenixXI`; verify it through AgentLink before publishing:

```powershell
& ../phoenixlighting/scripts/game.ps1 '?state','?loaded xiuinative'
```

Read the returned install path. AgentLink lives in the neighboring plugin repository, not this SDK. The client may be `phoenix-loader` with the character's window title; a timeout or missing `pol` process does not prove the game is closed. Retry and inspect the actual game window/log if needed.

`& ../phoenixlighting/scripts/game.ps1 -Screenshot` captures the current game frame at full resolution and returns the PNG path in `files`. It can also follow diagnostic steps; open the returned image to inspect actual rendering rather than relying on loaded/visible status alone.

After packaging, save and unload before replacing the loaded DLL. Substitute the confirmed path if it differs:

```powershell
& ../phoenixlighting/scripts/game.ps1 '/xiui save sync','wait 200'
# Require the completed-save reply before unloading.
& ../phoenixlighting/scripts/game.ps1 '/unload xiuinative','wait 800','?loaded xiuinative'
. ./scripts/common.ps1
Publish-XiuiAtomic -Source "$PWD/dist/xiuinative.dll" -Destination 'C:/Games/PhoenixXI-dev/PhoenixXI/plugins/xiuinative.dll'
& ../phoenixlighting/scripts/game.ps1 '/load xiuinative','wait 1500'
& ../phoenixlighting/scripts/game.ps1 '?loaded xiuinative','/xiui status','wait 400'
```

Require unload confirmation before replacement and a separate loaded/status response afterward. A queued load alone is not success. Older builds without `save sync` need `/xiui save` and an observed completed file write before unload. Do not copy a default INI/theme over existing user files during ordinary installation.

`build_all.ps1` packages/installs standalone by default; `-SkipInstall` leaves its artifact uninstalled only when explicitly requested. It does not perform the AgentLink unload/reload steps itself.

## Optional isolated host/runtime development

The two-DLL pair is only for an explicitly selected isolated development install. `./scripts/build_all.ps1 -Development -InstallPath <isolated-Phoenix-path>` installs it. `./scripts/dev.ps1 -InstallPath <isolated-Phoenix-path>` watches it. Never apply these to the normal active standalone install without selecting that mode.

`-RuntimeOnly` requires `-Development`; ABI changes require matching binaries and drained settings writers. `-SkipDevelopmentTests` (legacy alias `-SkipTests`) applies only to development checks. Standalone packaging retains its required startup check. Different feature definitions and MSVC runtimes prevent blindly sharing all HUD objects.

## Capture current defaults

Capture only when requested. This changes the baseline embedded for new installs and explicit reset; it does not reset the active client or change the immutable sharing preset.

```powershell
./scripts/capture-defaults.ps1
./scripts/check.ps1 -Name defaults
```

The script verifies loaded XIUI with AgentLink, requests `/xiui save sync`, and requires its completed-save reply. It snapshots the saved INI and theme, backs up the previous baseline under `out/defaults-capture-*`, then atomically publishes each baseline file. Older builds must be updated before live capture. The companion `config/xiuinative-defaults.capture.json` records source type, timestamp, counts and hashes after each capture. The saved `layout.viewport` records the game viewport used for those positions; keep it with the captured preset so first loads can fit other resolutions.

The C++ utility uses `settings_store::VisitPreferences`, the same boundary as portable settings, to omit session, profile metadata and local controller-device data. It also removes retired container demos and empty sections while preserving empty preference values, disabled states, Unicode and positions. UTF-8 and UTF-16LE saved files are supported.

Basic's named presets embed `config/hud-xiv.ini` and `config/hud-classic.ini` independently of the latest reset defaults. Refresh a named preset only when requested, using `-OutputPath` for that preset. To author a matching pair, capture the XIV setup first, apply `/xiui party preset classic`, then capture Classic with `-OutputPath "$PWD/config/hud-classic.ini" -ThemeOutputPath "$PWD/out/hud-classic-theme.ini"`. Confirm that only Party preferences differ before rebuilding. The October 20 source captures had 15,705 preferences each, with 79 Party fields changed and no differences elsewhere. The later Classic spacing correction amends only Party layout/offset preferences and adds its migration marker; the companion metadata records that amendment and the updated hash.

For an explicitly selected saved snapshot:

```powershell
./scripts/capture-defaults.ps1 -FromSaved -SettingsPath '<saved.ini>' -ThemePath '<saved-theme.ini>'
```

Use `-OutputPath` and `-ThemeOutputPath` to capture elsewhere. `-SkipBuild` reuses the existing utility. Capture does not package/install: check the changed baseline, then build the standalone DLL normally. Never restore older settings over current user changes as part of cleanup.

## GitHub releases

Releases live in the [private repository](https://github.com/Shuu-37/xiui-plugin/releases). Keep root `VERSION` numeric for CMake/Windows; alpha tags append the channel, such as `v1.0.5-alpha.1`. Preserve the shipped defaults unless a capture was requested.

Run `./scripts/package.ps1` and require both packaging checks to pass. Commit and push the intended source on `main`, then create and push an annotated tag at that commit. Upload only `dist/xiuinative.dll` and `dist/SHA256SUMS.txt`; no companion runtime or external assets are needed. Write the checksum as `<lowercase SHA-256>  xiuinative.dll`.

With the tag pushed and release notes written:

```powershell
gh release create v1.0.5-alpha.1 dist/xiuinative.dll dist/SHA256SUMS.txt --verify-tag --prerelease --latest=false --title 'XIUI Native 1.0.5 Alpha 1' --notes-file out/release-notes.md
```

Use a new tag for each alpha. Stable releases omit the channel suffix and prerelease flags. [.github/release.yml](../.github/release.yml) groups generated notes by PR label; manually include direct-commit changes and known limitations. Verify uploaded asset hashes and prerelease status after publication.

## Update documentation

```powershell
./scripts/update-docs.ps1
# With an already-current config check executable:
./scripts/update-docs.ps1 -SkipBuild
```

This exports the [settings inventory](reference/settings-inventory.md) from option metadata and checks local file links. It does not run gameplay checks, regenerate assets or refresh historical acceptance records. `python scripts/check-doc-links.py` checks links alone. The archived feature view is rebuilt only when needed with `python scripts/render_feature_parity.py`; `--export-diagram` additionally writes the personal diagrams copy.
