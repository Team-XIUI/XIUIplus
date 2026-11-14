# XIUI+

XIUI+ is an in-game HUD for Final Fantasy XI, built as a plugin for Ashita v4.

## Install

1. Download `XIUIplus.dll` from the [Releases page](https://github.com/Team-XIUI/xiuiplus-releases/releases).
2. Copy it into the `plugins` folder of your Ashita install.
3. In game, run `/load XIUIplus`.

To load it automatically, add `/load XIUIplus` to your Ashita startup script.

A setup guide opens the first time the plugin loads. Run `/xiuiplus` to open settings at any time.

## Update

1. In game, run `/unload XIUIplus`.
2. Replace `plugins/XIUIplus.dll` with the new download.
3. Run `/load XIUIplus`.

Your settings, profiles and positions are kept.

### Coming from XIUI Native

Unload `xiuinative`, replace its DLL with `XIUIplus.dll`, and change any `/load xiuinative` startup line to `/load XIUIplus`. Your existing data moves to `config/XIUIplus/` the next time the plugin loads.

## Learn more

- [User guide](docs/user-guide.md): features and commands.
- [Changelog](CHANGELOG.md): what changed in each release.

## Development

You need Windows, Visual Studio with the x86 C++ tools, CMake, Ninja, Python and the Ashita v4 SDK (set `ASHITA4_SDK_PATH`).

```powershell
./scripts/build-all.ps1
```

This builds `dist/XIUIplus.dll` and installs it into the development game folder. Unload XIUI+ in game first.

Start with the [developer docs](docs/README.md); [development](docs/development.md) covers builds and installs, and [focused checks](tests/README.md) covers testing.
