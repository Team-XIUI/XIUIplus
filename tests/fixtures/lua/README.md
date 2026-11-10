# XIUI action-import fixtures

Generated from the original XIUI 1.8.4 and 1.8.5 release defaults and saved-settings writer, downloaded October 17, 2026. Each version contains expanded `defaults.json`, the writer's `defaults.lua`, and `populated.lua` with representative non-default settings, actions and library records. These are saved data, not executable addon code.

Original project and contributors: [tirem/XIUI](https://github.com/tirem/XIUI), GPL-3.0. The existing [XIUI attribution](../../../assets/actionbars/NOTICES.txt) applies to derived data. The original license is distributed with the plugin's XIUI+ assets.

Pinned release archives:

- 1.8.4: `XIUI-1.8.4.zip`, 31,972,712 bytes; SHA-256 `138d9236b5571fbd0693a4b8dc66ba723408014388d75311085f5a2a553d2be4`.
- 1.8.5: `XIUI-1.8.5.zip`, 33,002,718 bytes; SHA-256 `80347ea06ba1f74223ce6f2b8980563f603da52da19c82b7f813f89bdd84bfaf`.

The [action import contract](../../../docs/reference/actionbars.md#importing) imports macros and hotbar/crossbar palettes only. The populated writer fixtures deliberately retain unrelated source settings so the focused `actions-import` check can verify that they do not enter the action store. Defaults and their expanded JSON remain as fixture provenance; no full Lua settings converter or generated defaults schema is compiled into XIUI+.

Source Lua is parsed as restricted saved data and never executed. Local source downloads and writer harness artifacts remain under ignored `out/lua-migration-research/`.
