# Developer documentation

Start with the [module index](modules/README.md) and read only the note for the area you are changing. Each note gives the source owners, important constraints and a focused check.

## Shared guidance

- [Foundations](foundations.md): HUD text, containers, bars, configuration, input and persistence contracts.
- [Development](development.md): build/install, AgentLink, defaults capture and documentation generation.
- [Input ownership](reference/input.md): current mouse, keyboard and controller rules; remaining hardware/software-keyboard acceptance.
- [Configuration](reference/configuration.md) and [settings](reference/settings.md): shared controls, navigation and preference organization.
- [Focused checks](../tests/README.md) and [assets](../assets/README.md): validation routes and generator ownership.
- [Player guide](user-guide.md): commands and player-facing usage.

## Source routes

- HUD feature: [module notes](modules/README.md) → `src/runtime/modules/<feature>/`.
- Shared drawing/text: `src/runtime/design_system.*`, `ui/` and `modules/module.*`.
- Configuration: `config/config_session.hpp`, `hud_runtime_config*.cpp`, `config/config_controls.*` and `config/option_layout.hpp`.
- Input: `src/host/controller_input.*`, `src/shared/controller_input.hpp`, `src/runtime/input/` and `hud_runtime_controller*.cpp`; native mouse/key dispatch starts in `src/host/plugin.cpp`.
- Settings/profiles: `settings/settings_store.*`, `settings/settings_profiles.*` and `hud_runtime_profiles.cpp`; [Lua imports](reference/lua-profile-import.md) use `hud_runtime_lua_import.cpp` and `settings/lua_*` adapters.
- Native hooks/lifetime: `src/host/plugin.*`, `src/host/runtime_host.*`, `src/shared/runtime_api.hpp` and `src/runtime/runtime_exports.cpp`. Read the ABI value from source.
- Build/resources: root `CMakeLists.txt`, `cmake/` and [the asset index](../assets/README.md).

Unqualified source paths above are relative to `src/runtime/`. Search the relevant owner before expanding to all source.

## Where detail belongs

[Module notes](modules/README.md) are short agent entry points. [Reference](reference/README.md) holds detailed current usage and system contracts. [Archive](archive/README.md) preserves old audits, plans, feature records and native investigation evidence; it is not required reading for routine changes.

Update the owning note when behavior or source ownership changes. Keep cross-module rules in the shared guides, and edit generated documentation through its generator.
