# Developer navigation

Start here for source ownership. [The user guide](user-guide.md) describes player features and commands. [Development](development.md) covers builds, installation and defaults; [focused checks](../tests/README.md) lists validation routes.

## Shared contracts

Read [foundations](foundations.md) before HUD changes: text profiles, containers, bars, config presentation, sounds and persistence. [Module foundations](reference/module-foundations.md) holds feature-specific details. For interactions, read [controller implementation](controller-implementation.md) and apply [the controller skill](../.agents/skills/xiui-controller-support/SKILL.md). [Remaining controller acceptance](controller-plan.md) distinguishes implemented behavior from hardware/native-keyboard limits.

## Task routes

- **HUD text, meters, native windows:** `src/runtime/modules/module.*`, `src/runtime/ui/`, `src/runtime/design_system.*`. Read [foundations](foundations.md#hud-text); start with `config-text`, `global-text` or `tp-meter` checks.
- **Configuration controls and layout:** `config/config_session.hpp` owns active module, focus, scroll, search and editors. `hud_runtime_config.cpp` selects/pages options; `hud_runtime_config_input.cpp` handles pointer actions; `hud_runtime_config_render.cpp` draws them. `config/config_controls.*`, `config/option_layout.hpp` and `config/config_layout.hpp` share geometry. Start with `config-ui`, `config-resize`, `settings-search` or `element-tabs`.
- **Settings, defaults and profiles:** `settings/settings_store.*`, `settings/settings_profiles.*`, `hud_runtime_profiles.cpp`, `config/profile_*`. Read [settings organization](settings-organization.md), [presets](presets.md) and [capture workflow](development.md#capture-current-defaults). Use `settings`, `defaults` or `profile-delete`.
- **Mouse, keyboard and controller ownership:** `src/host/controller_input.*`, `src/shared/controller_input.hpp`, `src/runtime/input/`, `hud_runtime_controller*.cpp` and `hud_runtime.cpp`. Read [controller implementation](controller-implementation.md); use `controller-input`, `controller-config` or `cursor-ownership` for the changed boundary.
- **Action Bars and crossbar:** `modules/actionbars/`, `input/crossbar_*`, `hud_runtime_actionbars.cpp`. Read [Action Bars](actionbars.md); use the dedicated targets in [the check guide](../tests/README.md).
- **Chat:** `modules/chat/`, `chat/`, `src/shared/chat_data.hpp`. Read [chat windows](chat-windows.md), [input](chat-input.md), [filters](chat-filters.md) and [controller implementation](controller-implementation.md); use the dedicated chat target.
- **Satchel:** `modules/satchel/`, `inventory/`, `hud_runtime_satchel.cpp`. Read [Satchel](satchel.md); use its dedicated UI/action/data targets.
- **Minimap:** `modules/minimap/minimap_module.*` owns options/rendering; `minimap_model.hpp` geometry; `minimap_maps.*` archive/texture/alpha; `ui/contour.hpp` shared contour drawing. Read [Minimap](implementation/28-minimap.md); use `minimap`. Keep named option IDs and saved INI keys stable.
- **Native hooks and lifetime:** `src/host/plugin.*`, `src/host/runtime_loader.*`, `src/shared/runtime_api.hpp`, `src/runtime/entry.cpp`, `hud_runtime.cpp`. The ABI version lives in source. Use the standalone startup check; isolated host/runtime work has failure fixtures in `tests/hot_reload_smoke.cpp`.
- **Assets and generated data:** start with [the asset index](../assets/README.md), then `cmake/resources.cmake` and `src/runtime/resources/`. Edit inputs/generators rather than generated tables.
- **Build graph:** root `CMakeLists.txt` configures the toolchain; `cmake/` separates models, host, resources, runtime sources/targets and checks. `xiui_models` and `xiui_models_release` reuse pure code with their respective dynamic/static MSVC runtime; HUD test definitions remain separate.

Paths under `config/`, `modules/`, `input/` and similar source areas above are relative to `src/runtime/` unless otherwise specified. Search narrowly with `rg` before expanding to all source.

## Documentation ownership

- Current contracts: this index, [foundations](foundations.md), [development](development.md), [controller implementation](controller-implementation.md) and feature guides linked above.
- Feature records: [implementation index](implementation/README.md) and [feature-parity audit](feature-parity.md). Check implemented behavior and acceptance limits before treating proposals as available APIs.
- Derived views: [settings inventory](settings-audit.md) and [feature HTML](feature-parity.html). Run `./scripts/update-docs.ps1`; edit their source metadata/Markdown.
- Dated evidence: [history](history/README.md) and [organization audit](audits/2026-10-18-codebase-organization.md). Old line references and build observations describe that snapshot, not current acceptance or standing task authorization.

Keep current guidance in one place and link to it. Preserve historical native evidence and meaningful decisions without duplicating completion narratives.
