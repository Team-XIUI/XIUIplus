# P12: Phantom Roll tracker

Status: **Implemented**. Runtime/release build and focused checks passed; gameplay acceptance remains for Cody. Potency is explicitly an unmodified base value; unknown modifiers remain unknown.

Audit date: October 15, 2026. Priority: Medium; independent job feature.

[Parity audit](../feature-parity.md) · [Plan index and shared contract](README.md)

## Current gap

Native now has **Extras → Phantom Roll**, with two roll seats, validated local totals, native buff deadlines, Double-Up countdown and ordinary-die bust probability.

Rules are shared with Action Bars → Skillchains: Retail, Horizon and Phoenix (75 cap). Phoenix imports the pinned pre-SoA/pre-Abyssea base powers and effect labels, including Healer hMP, Gallant reflection and the earlier pet-roll effects. Unknown bonuses remain excluded from base potency. See [Phoenix sources](../../assets/phoenix/README.md).

The module starts disabled to preserve the captured layout. Explicit Preview uses isolated samples even while disabled; ordinary disabled modules show no edit panel or hit bounds.

## Source evidence

- Lua reference: `XIUI/modules/phantomroll/{init,tracker,data,display}.lua`.
- Lua reference: `XIUI/config/phantomroll.lua`.
- Native integration: [src/runtime/combat/action_packet.hpp](../../src/runtime/combat/action_packet.hpp).
- Native integration: [src/runtime/modules/party/party_source.hpp](../../src/runtime/modules/party/party_source.hpp).
- Native integration: [src/runtime/ui/combat_timing_display.hpp](../../src/runtime/ui/combat_timing_display.hpp).

Lua paths are relative to the reference repository identified in the parity audit; brace lists name multiple files.

## Scope

- Two roll seats with name/total/die treatment, lucky/unlucky/11/bust states, potency and independent remaining durations.
- Double-Up countdown and bust probability, plus the Lua retail/Horizon potency/rules preference and edit preview.

## Dependencies

Reuse combat decoding and native player-buff data; no hotbar dependency.

## Implementation

1. [phantom_roll_tracker.cpp](../../src/runtime/modules/phantomroll/phantom_roll_tracker.cpp) consumes the existing bounded `0x028` decoder's local category 6 Roll/Double-Up actions with a local target and plausible total. It reconciles `party_source`'s cached local status IDs/deadlines every update. That source already reads the installed SDK's native status icons/timers and native UTC clock; no duplicate `0x063` parser, host forwarding or guessed timer signature is needed.
2. Live and busted copies retain separate seats. Existing observed bust clocks match first, then pending busts, with each native 309 slot used once. Newly observed actions have a three-second snapshot-arrival grace period; authoritative removal clears an observed seat immediately. A new roll never evicts an unrelated busted seat. Action-only durations are marked `~` until native timing arrives.
3. [phantom_roll_data.cpp](../../src/runtime/modules/phantomroll/phantom_roll_data.cpp) ports all 31 retail entries and 17 Horizon overrides from XIUI's `modules/phantomroll/data.lua`. The UI labels **Base** values; equipment, party-job and special ability modifiers and bust penalties remain unknown. Horizon's level-scaled Chaos base requires the real local level. Companion potency and missing action history remain unknown. An existing icon never invents a total or an eligible Double-Up target. Bust chance is labeled **Base bust chance (ordinary die)** because special ability effects are not included.
4. [phantom_roll_module.cpp](../../src/runtime/modules/phantomroll/phantom_roll_module.cpp) uses shared native containers/meters, independently editable text profiles and state tints via `TintTextProfile`. Dice are procedural native rounded faces and styled totals, so no extra bitmap/cache is needed. Enabled state, position, side-by-side/stacked layout, content scale, retail/Horizon choice and presentation preferences persist. Tracking survives contextual hiding; zone/logout, character/job changes and settings/profile loads clear transient seats. Preview samples never enter the live tracker and clear when configuration closes.

## Input and controller behavior

Display-only tracker. Configuration follows shared controller navigation and never triggers Roll or Double-Up.

## Completion criteria

- [x] Two seats, local action totals, Double-Up window association, bust preservation and authoritative removal implemented.
- [x] Shared native buff deadlines replace estimates and preserve distinct bust clocks.
- [x] Unknown action/modifier state remains explicit; preview uses separate samples.
- [x] Runtime/release build and focused roll-tracking fixture passed.
- [ ] Cody's live Corsair behavior/layout check.

## Focused validation

[tests/phantom_roll_checks.cpp](../../tests/phantom_roll_checks.cpp) covers unrelated/invalid actions, deduplication, late snapshots, authoritative timing, two same-named live/busted seats, independent bust clocks, removal, unknown-history recovery, Double-Up association/expiry, retail/Horizon base values and ordinary-die probability boundaries. `RunPhantomRollChecks()` passed through `xiui_config_input_smoke --new-features-only`; runtime/release build and isolated standalone startup passed. No in-game validation is claimed.

Apply the [shared implementation contract](README.md#shared-implementation-contract), then update this plan and the audit when the feature is integrated.

