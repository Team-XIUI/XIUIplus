# P17: MP cost preview on resource bars

Status: **Implemented** in the Native working tree. Focused checks passed and runtime installed; live presentation remains for Cody to check.

Updated: October 15, 2026. Reuses the existing Cast Cost native selection source.

[Parity audit](../feature-parity.md) · [Plan index and shared contract](README.md)

## Implemented behavior

**Player Bar → General → Spell MP cost preview** controls Player's MP segment. **Party → Resources → Spell MP cost preview** independently controls the local member's Party MP segment. Both start enabled and persist as `[player] mp_cost_preview` and the selected Party list's `mp_cost_preview`. Portable profiles and global Reset to defaults use these module settings; General has no MP preview option. Cast Cost can be hidden or disabled while previews remain enabled.

The segment spans the current MP down to the amount remaining after the selected spell's base cost. An affordable selection uses the theme accent gradient; insufficient MP uses red and clips the segment to the available MP. Numeric MP values remain unchanged. Zero-cost selections are valid source data but draw no spend segment. Zero maximum MP, stale/unavailable selection and other party members draw none.

The source reads the native spell resource's **base ManaCost**. The installed SDK exposes no adjusted spell-cost getter. Job traits, Arts, buffs and other cost modifiers are not estimated; a modified cost may differ from this preview. Both controls describe the segment as base MP cost.

## Source evidence

- Lua reference: `XIUI/modules/castcost/shared.lua`.
- Lua reference: `XIUI/modules/playerbar.lua: GetMpCost preview`.
- Lua reference: `XIUI/modules/partylist/display.lua: local-player MP preview`.
- Lua reference: `XIUI/core/settings/user.lua: showMpCostPreview`.
- Native integration: [src/runtime/modules/castcost/cast_cost_source.hpp](../../src/runtime/modules/castcost/cast_cost_source.hpp).
- Native preview geometry: [src/runtime/modules/castcost/mp_cost_preview.hpp](../../src/runtime/modules/castcost/mp_cost_preview.hpp).
- Native integration: [src/runtime/modules/player/player_module.cpp](../../src/runtime/modules/player/player_module.cpp).
- Native integration: [src/runtime/modules/party/party_module.cpp](../../src/runtime/modules/party/party_module.cpp).
- Native integration: [src/runtime/modules/module.cpp](../../src/runtime/modules/module.cpp).
- Native themed/modern segment rendering: [src/runtime/design_system.cpp](../../src/runtime/design_system.cpp).

Lua paths are relative to the reference repository identified in the parity audit; brace lists name multiple files.

## Shared source and rendering

`cast_cost_source::MPPreview(now)` exposes native availability, selected-spell validity, player identity, base cost and sample time. It reuses the existing spell selection and resource lookup. Closing the native menu clears the selection immediately on the next source update; job/level, character and zone transitions clear cached state. Samples older than 250 ms are rejected. Existing zoning packet handling also clears the source.

Player and Party share `MPPreviewRanges`. Party matches the source's local-player identity and requires an exact native maximum MP; generated edit-preview rows and estimated remote maximums are excluded. The source links survive module resets and Party preset/copy operations; preview preferences belong to each module, and presets restore their enabled defaults.

Shared meter drawing accepts optional normalized color ranges. Window-theme ranges use the existing native fill texture and content geometry; Modern ranges follow the existing fill shape. Segments clip to the visible fill, leave meter caps, borders, labels and values intact, and retain the existing MP animation. No MP value is subtracted from the HUD snapshot.

Future hotbar selection can feed this same source contract when P02 is implemented.

## Input and controller behavior

Preview is display-only. Native spell-list selection supplies its context for mouse, keyboard and controller use. The shared toggle uses normal config navigation and captures no additional gameplay controls.

## Completion criteria

- [x] Base spell cost uses exact local maximum MP without changing the displayed current value.
- [x] Only the local Party row gets a preview; unavailable/zero-max MP and invalid selections produce no segment.
- [x] Source and both independent module preferences are independent of Cast Cost visibility; both bar appearances use the common renderer.

## Focused validation

[tests/mp_cost_preview_checks.cpp](../../tests/mp_cost_preview_checks.cpp) provides focused fixtures for affordable/insufficient spending, zero cost/max MP, identity, stale/missing source data, themed fill clipping and mirrored Modern geometry. These checks were added but not run by the implementing agent; the coordinated build owns execution. Source invalidation was reviewed in code. Cody checks live menu transitions, cost-modifier limitations and visual presentation.

The [shared implementation contract](README.md#shared-implementation-contract) applies to later consumers of the preview source.


Coordinated validation: affected runtime and focused executable built; `xiui_config_input_smoke --resource-target-features-only` and the settings presentation audit passed. Installed runtime hot reload was observed through AgentLink on October 15 at 20:44:13; saved current layouts/styles were retained. No native gameplay or physical-controller acceptance was performed.

The preview toggle now lives in Player Bar → General and Party → Resources, with independent persistence and enabled defaults. The focused `--mp-preview-only` check covers separate saves and reset defaults, and the refreshed settings audit passed. AgentLink confirmed the installed runtime at 20:50:31 on October 15.
