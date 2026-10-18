# P16: Player resource display and state colors

Status: **Implemented** in the Native working tree. Focused checks passed and runtime installed; gameplay acceptance remains with Cody.

Updated October 15, 2026. Priority: Medium.

[Parity audit](../feature-parity.md) · [Plan index and shared contract](README.md)

## Delivered behavior

Player Bar retains job identity, MP auto-hide, staged TP and attack/rest timers. General now gives HP and MP independent Number, Percent, Number (percent), Percent (number), Current / max and Hidden formats. Player shares Party's local native resource samples, which validate maxima against native percentages instead of retaining a previous job's totals. A native 100% sample confirms the exact maximum; subsequent damage/spending can retain it while the same job/level and percentage remain consistent. Character, zone and job changes invalidate retained maxima. Unknown maxima use native fill/percentage and display `?` in Current / max; they never supply a fabricated MP preview segment. Snapshot fallback remains available when the native source is unavailable.

Text has independent HP, MP and TP label/value profiles, with the normal font, size, colors, effects and offsets. Existing HP/shared profile keys and the original six selector indices remain stable. A one-time `[player] resource_text_version=1` migration copies the fully loaded legacy label/value profiles to MP and TP, including custom gradients/effects/positioning. Subsequent edits persist independently under `text.player.mp.labels`, `text.player.mp.values`, `text.player.tp.labels` and `text.player.tp.values`. New profiles participate in global text controls. Measured resource text contributes to minimum sizing and edit bounds.

Colors includes optional HP warning/critical gradients with inclusive percentage thresholds and optional HP value tinting. Normal HP retains its existing gradient. TP has optional ready-value tinting at 1,000 TP, a custom upper-layer gradient and a configurable ready-flash color/strength. The three 1,000-TP fill stages remain intact, including a distinct third layer; actual numeric TP is unchanged. State text uses `TintTextProfile`, preserving gradients and effects.

State palettes and text tints start disabled so existing appearances retain their saved colors. The existing TP ready-blink preference remains authoritative. All bars selection preserves existing resource settings; a subsequent shared style edit changes only that property. Gradient controls retain paired top/bottom rows.

## Source evidence

- Lua reference: `XIUI/config/playerbar.lua`.
- Lua reference: `XIUI/modules/playerbar.lua`.
- Lua reference: `XIUI/core/settings/user.lua: playerBarHpDisplayMode and playerBarMpDisplayMode`.
- Lua reference: `XIUI/core/settings/colors.lua`.
- Native integration: [src/runtime/modules/player/player_module.hpp](../../src/runtime/modules/player/player_module.hpp).
- Native integration: [src/runtime/modules/player/player_module.cpp](../../src/runtime/modules/player/player_module.cpp).
- Native integration: [src/runtime/modules/party/party_source.hpp](../../src/runtime/modules/party/party_source.hpp).
- Native integration: [src/runtime/ui/tp_meter.hpp](../../src/runtime/ui/tp_meter.hpp).
- Focused checks: [tests/player_resource_checks.cpp](../../tests/player_resource_checks.cpp).

Lua paths are relative to the reference repository identified in the parity audit; brace lists name multiple files.

## Dependencies

The Player formatter reuses Party's resource-formatting function while supplying exact local maxima. P21 covers shared visibility. [P17](17-mp-cost-preview.md) covers the separately integrated selected-spell MP range overlay; it does not change the underlying MP values or these profiles.

## Implementation

The Player module owns formatting, settings and state-color selection. Shared `TPMeterLayers` accepts an optional color palette; existing callers without one retain the original behavior. Shared `module_render_context::DrawMeter` continues to draw track, fill stages and text through the established renderer. No native vitals, preview data or resource calculations are written to the client.

## Input and controller behavior

All options use shared selectors/sliders/colors and the current controller config graph. Resource displays themselves remain display-only.

## Completion criteria

- [x] All value formats calculate from exact local maxima and handle zero maxima without division errors.
- [x] Existing saved styles migrate without losing custom gradients, shadow, stroke, glow or positioning.
- [x] TP state colors/blink and HP thresholds are configurable while numeric TP and fill stages remain correct.

## Focused validation

`xiui_config_input_smoke --player-resources-only` calls the focused `RunPlayerResourceChecks()` fixture. It covers all formats, zero/large maxima, inclusive HP boundaries, legacy migration, independent profile persistence, shared text visitation, nonmutating All bars selection, changed-property copying, paired gradient controls, TP stages/custom flash colors and measured text sizing. It also checks stale job maxima, retained confirmed totals, native percentage fallback, source preservation on reset and unavailable-source fallback. Empty caster MP remains distinct from a character with no MP maximum.

The coordinating agent runs the affected build/check and performs installation. Native visual/gameplay behavior remains for Cody to inspect; this implementation does not claim a physical-controller or live gameplay acceptance check.


Coordinated validation: affected runtime and focused executable built; `xiui_config_input_smoke --resource-target-features-only` and the settings presentation audit passed. Installed runtime hot reload was observed through AgentLink on October 15 at 20:44:13; saved current layouts/styles were retained. No native gameplay or physical-controller acceptance was performed.
