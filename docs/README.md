# Developer documentation

Start with the [module index](modules/README.md) and read only the note for the area you are changing. Each note gives the source owners, important constraints and a focused check.

## Shared guidance

- [Foundations](foundations.md): HUD text, containers, bars, configuration, input and persistence contracts.
- [Development](development.md): build/install, AgentLink, defaults capture and documentation generation.
- [Input ownership](reference/input.md): current mouse, keyboard and controller rules; remaining hardware/software-keyboard acceptance.
- [Ashita compatibility](reference/ashita-compatibility.md): supported 4.16/4.30 interfaces, SDK access and mouse normalization.
- [Controller support](reference/controller-support.md): interaction design, native ownership and focused validation requirements.
- [Configuration](reference/configuration.md) and [settings](reference/settings.md): shared controls, navigation and preference organization.
- [Focused checks](../tests/README.md): validation routes.
- [Player guide](user-guide.md): commands and player-facing usage.

## Source routes

- HUD feature: [module notes](modules/README.md) → `src/runtime/modules/<feature>/`.
- Shared drawing/text: `src/runtime/design_system.*`, `ui/` and `modules/module.*`.
- Configuration: `config/config_session.hpp`, `hud_runtime_config*.cpp`, `config/config_controls.*` and `config/option_layout.hpp`.
- Input: `src/host/controller_input.*`, `src/shared/controller_input.hpp`, `src/runtime/input/` and `hud_runtime_controller*.cpp`; native mouse/key dispatch starts in `src/host/plugin.cpp`.
- Settings/profiles: `settings/settings_store.*`, `settings/settings_profiles.*` and `hud_runtime_profiles.cpp`. [Action imports](reference/actionbars.md#importing) use `modules/actionbars/action_import.*` and the restricted `settings/lua_data.*` reader.
- Native hooks/lifetime: `src/host/plugin.*`, `src/host/runtime_host.*`, `src/shared/runtime_api.hpp` and `src/runtime/runtime_exports.cpp`. Read the ABI value from source.
- Build/resources: root `CMakeLists.txt`, `cmake/` and [assets/](../assets/).

Unqualified source paths above are relative to `src/runtime/`. Search the relevant owner before expanding to all source.

## Where detail belongs

[Module notes](modules/README.md) are short development entry points. [Reference](reference/README.md) holds detailed current usage and system contracts. Completed plans and old investigation records remain available in Git history.

Update the owning note when behavior or source ownership changes. Keep cross-module rules in the shared guides, and edit generated documentation through its generator.
