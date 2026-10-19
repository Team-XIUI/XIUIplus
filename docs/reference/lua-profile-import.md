# Importing XIUI Lua profiles

Open XIUI configuration, select the profile **three-dot menu → Import profile → XIUI Lua**. The existing **Share key** option still imports Native profiles.

The Lua page scans the active install's `config/addons/xiui` folder automatically. Paste another installation/settings folder and select **Scan**, or use **Browse** to select a saved `.lua` profile. Selecting `profiles/profilelist.lua` discovers the whole setup. Supported source defaults are pinned to XIUI 1.8.4 and 1.8.5; a missing version manifest uses 1.8.4 and reports that choice.

Use the arrows to choose **All profiles** or one profile. Review the scrollable report before importing. It lists converted fields, approximations, unsupported settings, missing resources and blocking errors. Mouse wheel, Page Up/Down and controller report focus scroll the same report. Keyboard/controller navigation shares the dialog's actions; typing uses the keyboard.

Import creates new Native profiles, with unique names. It preserves Default and leaves the current setup selected unless **Activate single import** is checked. Retrying an unchanged source does not create duplicate profiles. Supported settings include module visibility and placement, available text/meter styling, actions, macros, palettes, hotbar bindings, pet resource/recast styles, song bonuses and manual Fast Cast values. Compatible custom ready-check WAVs are copied into `config/sounds`.

The optional choices default off:

- **Activate single import** applies one imported profile immediately.
- **Link unassigned chars** copies matching numeric character associations within the current Native identity scope. Existing associations stay intact.
- **Controller options** applies source trigger thresholds, timing and expanded/double-tap behavior when activating one profile. Hardware identities and physical button mappings remain local.

Native-only features start with Native defaults. Unsupported per-module skins, arbitrary fonts and other Lua-only customizations are reported; this is not exact visual parity. Historical inventory/cache data is archived without replacing Native's live observations. Single files may lack the manifest, character selections, sounds or legacy positions available through folder discovery.

Each committed import is stored under `config/xiuinative-profiles/lua-<batch>/`: new profile INIs, `report.txt`, `sources.txt`, frozen source files and optional controller values. The report records destination names and source ordering; Native's selector keeps its usual alphabetical order. Source Lua is parsed as restricted saved data and never executed. Imports stage as a group before becoming visible, and failed staging leaves existing profiles intact.

Implementation owners: `hud_runtime_lua_import.cpp`, `config/lua_import_layout.hpp`, `settings/lua_setup_import.*`, `settings/lua_profile_import.*`, the `settings/lua_import_*` adapters and `settings/settings_profiles.*`, relative to `src/runtime/`. CMake generates the pinned defaults header with `scripts/build-lua-profile-schema.py`; fixture provenance lives in [the fixture note](../../tests/fixtures/lua/README.md).

Focused check: `./scripts/check.ps1 -Name lua-profile-import`. It covers both release writers, conversion units, discovery, preview/controller routing and isolated persistence. Live visual/gameplay acceptance remains with Cody.
