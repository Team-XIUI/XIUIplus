# P21: Shared contextual visibility

> Historical record. Use the [current documentation](../../README.md) for implementation guidance.

Status: **Implemented** in the Native working tree. Live transitions remain for Cody to check.

Updated: October 15, 2026. Cross-cutting foundation for existing and future HUD modules.

[Parity audit](../feature-parity.md) · [Plan index and shared contract](README.md)

## Implemented behavior

General → Visibility provides one **Apply to** selector for All modules or an individual HUD module, including separate alliance panels, storage trackers and Chat 2. It uses the normal shared config controls for mouse, keyboard and controller input.

- **Hide during events** and **Hide with menus** apply globally or to the selected module. A shared hiding rule also applies when the module's own switch is off.
- **Keep visible with macros** preserves a module during native macro-palette display when menu hiding applies. Both the shared and module preferences must permit the exception. The control reports unavailable when native palette identity cannot be read.
- **Keep Party with menus** leaves the main Party panel visible. Party's menu-hiding preference applies to the alliance panels; other modules retain their own rules.
- **Hide Player in party** uses real active party/alliance membership, including remote members. Edit-preview rows do not count.
- Native map display and interface hiding suppress HUD presentation. Edit mode suspends contextual hiding so enabled module previews remain editable.

Temporary hiding preserves enabled flags, positions, presentation settings and native HUD choices. Data sources, status tracking, notification expiry and session counters continue updating while presentation is hidden.

## Native state

`module_visibility_source` resolves the existing XIUI menu, event and interface signatures once, accepting only unique matches. Pointer reads are guarded; unavailable readers retain explicit availability flags. Menu identity distinguishes the four native macro palettes and map windows. Chat entry, player/combat command lists, magic, abilities and mount selection do not trigger menu-based hiding.

When those native readers are unavailable, menu hiding falls back to the SDK's native menu-input flag and event hiding falls back to the player's cutscene status. Unsupported map/interface/palette state is not inferred from a timer. Native HUD text uses the shared Native renderer, so visibility does not depend on Lua font objects or Ashita FontManager state.

## Settings and migration

Shared preferences persist in `[visibility]`; each module uses `[visibility.<module id>]`. The common keys are `hide_events`, `hide_menu` and `keep_macro`. Shared special rules use `alliance_only_menu` and `player_in_party`.

Existing Party/alliance `hide_events` and Inventory/Gil `hide_menu` values supply compatibility defaults when the corresponding shared-policy keys are absent. Party and alliance panels retain their previous default of hiding during events; other new event/menu rules start off. Later saves write the common visibility sections. The old local controls and render checks were removed, with option numbering preserved.

## Input and controller behavior

Rendering, chat history, native draft adoption, pointer targets, Ready Check, hover sounds, tooltips, controller entry and controller focus use the same contextual policy. Hidden chat preserves its draft while releasing text focus. Hidden pointer interactions are canceled and their owned button release is drained. Hidden controller surfaces leave navigation and release focus through the existing held-input drain.

Party entry and group switching skip unavailable or context-hidden panels. A native menu still owns its input even when a presentation exception keeps a HUD panel visible.

## Source references

- [Shared policy, controls and persistence](../../../src/runtime/config/module_visibility.cpp)
- [Native context reader and runtime ownership](../../../src/runtime/hud_runtime_visibility.cpp)
- [General configuration](../../../src/runtime/modules/general/general_module.cpp)
- [Rendering and pointer/keyboard routing](../../../src/runtime/hud_runtime.cpp)
- [Controller routing](../../../src/runtime/hud_runtime_controller.cpp)
- Lua reference: `XIUI/core/gamestate.lua`, `core/moduleregistry.lua`, `core/settings/user.lua` and `config/playerbar.lua` in the reference repository identified by the parity audit.

## Validation

The x86 runtime build and `xiui_module_parity_smoke --hud-parity-only` passed on October 15, 2026. Focused checks include policy migration, macro exceptions, global/per-module rules and status-surface ownership release.

Focused source review and whitespace checks were completed during implementation. Live menu, macro, map, event and interface transitions, including controller hardware behavior, remain for Cody to check. No game-context manipulation or visual validation was performed.

