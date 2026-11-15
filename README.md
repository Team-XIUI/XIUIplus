# XIUI+

XIUI+ is an in-game HUD for Final Fantasy XI, built as a plugin for Ashita v4.

> **Disclaimer:** XIUI+ is built entirely with AI. The code, tests and documentation in this repository were written by AI coding agents under human direction.

## Install

1. Download `XIUIplus.dll` from the [Releases page](https://github.com/Shuu-37/XIUIplus/releases).
2. Copy it into the `plugins` folder of your Ashita install.
3. In game, run `/load XIUIplus`.

To load it automatically, add `/load XIUIplus` to your Ashita startup script.

A setup guide opens the first time the plugin loads. Run `/xiuiplus` to open settings at any time.

## Update

1. In game, run `/unload XIUIplus`.
2. Replace `plugins/XIUIplus.dll` with the new download.
3. Run `/load XIUIplus`.

Your settings, profiles and positions are kept.

## Learn more

- [User guide](docs/user-guide.md): features and commands.
- [Releases](https://github.com/Shuu-37/XIUIplus/releases): what changed in each version.

## Development

You need Windows, Visual Studio with the x86 C++ tools, CMake, Ninja, Python and the Ashita v4 SDK (set `ASHITA4_SDK_PATH`).

```powershell
./scripts/build-all.ps1
```

This builds `dist/XIUIplus.dll` and installs it into the development game folder. Unload XIUI+ in game first.

Start with the [developer docs](docs/README.md); [focused checks](tests/README.md) covers testing.

## License

XIUI+ is licensed under the [GNU GPL v3.0 or later](LICENSE). Bundled third-party components are listed in the [third-party notices](docs/third-party-notices.md).
