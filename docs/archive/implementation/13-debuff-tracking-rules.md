# P13: Status data and XIUI duration estimates

> Historical record. Use the [current documentation](../../README.md) for implementation guidance.

Status: **Implemented**. Updated October 16, 2026 after Cody selected the original XIUI database as the primary duration catalog for combat-observed effects.

[Parity audit](../feature-parity.md) · [Native-data findings](../native-mob-data-findings.md) · [Plan index](README.md)

## Result

Confirmed combat effects use fixed durations imported from the original Lua XIUI database. Shared Target, Enemy List and Pet status views show estimated countdowns with plain labels such as `1:30`; a smaller tooltip note says "Estimate: Effects may end early". Actual local-player icon/timestamp pairs retain native deadlines. Authoritative local/party snapshots still determine which effects are present, including empty lists; the five remote party snapshots supply no timers of their own.

The live Paralyze capture supplied a successful application and removal about 38 seconds later, without an expiry timestamp. Consequently, the database's 120-second Paralyze value is an estimate. Observed wear-off clears it immediately instead of waiting for the estimate to end.

The October 17 Phoenix (75 cap) profile overrides applicable bases: Blind/Blind II 80–300 seconds, Paralyze/Paralyze II/Silence 30–120 seconds, Poison/Poisonga 30 seconds. Ranged labels count down as `30-120`, then `<80`; tooltips explain that modifiers/resistance can change the base. After a Phoenix estimate ends, presence remains with an unknown timer until observed removal or lifecycle reconciliation. Native local timestamps remain authoritative. Retail/Horizon retain the catalog behavior below. Profile selection is shared with Action Bars → Skillchains and Phantom Roll; see [Phoenix sources](../phoenix-source-audit.md).

## Source and implementation

- Retained source: `assets/status/debuff_retail.lua` from the local tirem/XIUI checkout, with its attribution, license and exact source hash.
- Importer: `scripts/build-status-rules.py`; generated native data: `src/runtime/resources/xiui_status_durations.hpp`.
- Lookup: `src/runtime/combat/status_durations.hpp`; separate spell, weapon-skill, ability, pet and additional-effect namespaces, per-effect fixed overrides and XIUI's known status-duration fallback.
- Tracking: `combat_source::ApplyStatus` starts or refreshes an estimated deadline only for confirmed status-gain evidence. `combat_status::estimated` distinguishes it from native expiry.
- Presentation: shared `ui/status_strip.hpp` renders plain countdown labels and estimated-duration tooltips with the existing text profiles and inspection paths.
- No Lua interpreter, runtime file reads or new source-selection settings are needed. The catalog and notices are embedded in the normal DLL.

## Rules and limits

1. Actual successful applications start or refresh estimates. Damage, misses, resists and no-effect messages do not add or refresh an effect.
2. Observed wear-off, removal and death clear affected records immediately. Zoning, logout, character/entity changes and existing lifecycle resets clear transient tracking.
3. Estimated expiry removes a known observed effect, including mob self-buffs whose wear-off may not be sent to this player.
4. Effects absent from the catalog remain untimed and can retain the existing optional `?` marker.
5. Loading midway through a fight does not reconstruct a missing application time. No countdown restarts from drawing an icon or observing spikes damage.
6. Only fixed literal durations and fixed per-effect overrides are imported. TP formulas, gear/merit/job-point models, server overlays and unconfirmed secondary-effect inference remain out of scope.
7. Collections stay bounded to 128 entities and 32 effects per entity. Native local duplicates retain their own paired deadlines.

## Input and controller behavior

Tracking performs no gameplay action. Existing timer visibility/placement and tooltip controls remain in use. Shared pointer/controller inspection shows the same estimated-duration explanation without casting, targeting or canceling effects.

## Completion criteria

- [x] Original XIUI duration data is the primary catalog for confirmed combat effects.
- [x] Database timers are explicitly estimated; native local deadlines stay exact.
- [x] Paralyze, Blind and Blaze Spikes resolve from the catalog.
- [x] Ability/weapon ID collisions and fixed per-effect overrides are handled.
- [x] Confirmed early removal overrides an estimate; successful reapplication refreshes it.
- [x] Damage/resist/no-effect messages cannot restart a timer.
- [x] Unknown durations remain unknown, and known estimates expire.
- [x] Existing status controls, styles, positions and native snapshot preference are retained.

## Focused validation

`xiui_module_parity_smoke --status-timers-only` checks duration lookup, action namespaces, overrides, estimated/native/unknown labels, non-application rejection, refresh, the captured early Paralyze removal, bounded tracking and expiry. The x86 runtime and this focused target passed on October 16; AgentLink confirmed the installed runtime hot reload. Cody checks appearance/gameplay.

See [Status Effects](../../reference/status-effects.md) for usage and [timer findings](../native-mob-data-findings.md) for packet evidence and the distinction between estimates and authoritative expiry.
