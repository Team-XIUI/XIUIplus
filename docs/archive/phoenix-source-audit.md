# Phoenix source audit for XIUI Native

> Historical record. Use the [current documentation](../README.md) for implementation guidance.

Audited and implemented October 17, 2026. Horizon support is retained. Inventory restrictions are excluded. The findings below describe the pre-change source review; the implementation record follows here.

## Implemented follow-up

1. Ordered skillchain properties now drive prediction. Phoenix hints wait more than three seconds; the display retains a conservative 200 ms closing margin.
2. **Phoenix (75 cap)** joins Retail and Horizon in the shared combat rules. The pinned HeadsUp live settings establish a ToAU/75-cap source profile. Base SQL and effective pet overrides are retained in [assets/phoenix](../../assets/phoenix/README.md).
3. Phantom Roll shares that profile, with Phoenix base powers and era-specific hMP, reflection and pet effect labels. Unknown bonuses remain outside base potency.
4. Phoenix Blind, Paralyze and Silence display base ranges; Poison/Poisonga use their thirty-second base. Expired estimates become unknown until observed removal rather than inventing wear-off. Native local timestamps remain authoritative.
5. The finalized mob snapshot now pins the October 16 HeadsUp export at Phoenix `11c6ee5`. Target supplements it with 708 effective spawn-slot relationships, source chances and base cooldowns.
6. Action execution, inspection and Recast share charge metadata and signed `Calc2` interpretation; Sic is distinguished from learned Ready.
7. Minimap → Markers offers distant party/alliance snapshots: at most one request per ten seconds while needed and visible, twenty-second cache expiry, same-zone/current-member/floor validation and native-entity preference.
8. [Treasure Pool](implementation/07-treasure-pool.md) adds item/winner/history/timer presentation and individual/batch Lot/Pass with shared controller support. Alerts remain in Notifications.

The affected native x86 build and focused Phoenix/Treasure, Skillchain and Minimap checks pass. Live combat, loot, appearance and physical-controller acceptance remain with Cody. No server configuration was changed.

Installed through a coordinated AgentLink unload/load at 18:19 on October 17. Fresh status confirms host **18:11:58**, runtime **18:15:36**, GPU text, visible HUD and Phoenix (75 cap) rules; Treasure Pool is ready and empty. All 10,311 existing settings are retained except the two intentional shared rules values (`0` → `2`); 102 defaults were added and theme bytes are identical. Default profile follows the normal save path. Runtime SHA-256: `387FEE8E04C016B3E90AAAFB3DD7FF8DCD21938D63A07BB296D8C8536EB20AA8`. Backup/report: `out/phoenix-audit-20261017/install-20261017-181817` and `install-report.json`. No live Lot/Pass action was sent. Standalone 1.0.2 predates this implementation.

The subsequent cleanup centralizes typed combat settings, charge helpers and Treasure Pool input/batch paths. Runtime **18:33:05** hot-reloaded at **18:34:52** with the existing host, GPU text, visible HUD and Phoenix rules retained. All 10,413 settings/profile values and exact theme bytes were preserved; Default profile changed whitespace only. Focused checks passed. Runtime SHA-256: `EC76D4FEE3E9BBB0EFD3AFD52ADB039E5A7168CD975F2DCFFD6B93B61ED83C13`; backup/report are under `out/cleanup-20261017`. No loot action was sent during validation.

## Source and scope

Fetched `origin/live` and `origin/beta` from `phoenixffxi/Phoenix`:

- Live: `11c6ee57720a8c6fd79c0fd46ff38ea8e074bf5e`, October 14, 2026.
- Beta: `20751e93307a577a04925e6830f759ad4d3ff68a`, October 16, 2026.
- The earlier audit used local `fbdcc318853b6fda31c03149499049dd6c2eb738`. That checkout's branch is 270 commits behind live and has two commits of its own. It was left on `perf/gil-economy-index`, with its existing untracked directory intact.
- A clean detached worktree of live is retained at `out/phoenix-audit-20261017/source`. Beta was inspected through Git comparisons.

The fetched branch is evidence of published source, not proof of the running server's deployed revision, settings or database migrations. Effective Phoenix rules require the base data, enabled SQL/data/Lua modules and expansion settings. `settings/default/main.lua` alone describes upstream defaults. The tracked `modules/init.txt` is a live example and can differ from deployment.

The initial read-only audit inspected combat rules, Corsair, enfeebling estimates, pet/recast data, mob/spawn information, party map packets and Treasure Pool. Implementation was subsequently authorized and is recorded above; it includes generated assets and a native build, without gameplay mutation probes.

## 1. Fix skillchain property priority before adding more presets

**Confirmed source-level mismatch. Highest priority.**

Phoenix preserves primary, secondary and tertiary properties. `FormSkillchain` tries the opening action's properties in order, then the closing action's properties in order, returning the first valid pair. XIUI currently reduces each action to a bitmask and tries a fixed global combination list. That discards the ordering needed to predict Phoenix's chosen result.

Example: **Shoulder Tackle → Spinning Attack**. Phoenix's ordinary property-resolution path returns **Impaction**. XIUI's current global list selects **Liquefaction** from the same property sets. This is a prediction issue; the observed server skillchain message still drives the resulting live burst display.

The read-only comparison found different named predictions for **1,243 of 39,204 candidate weapon-skill pairs**. This compares the 198 shared base-table entries, normalizes the displayed Light/Light II and Darkness/Darkness II names, and excludes buffs, special actions and deployed-era availability. It is not a gameplay failure rate.

Data-set comparison against the fetched live base SQL found no property-set differences across **198 weapon skills, 58 Blue Magic spells and 27 pet skills** already present in XIUI's Retail table. This comparison covers shared entries, not complete Phoenix action coverage or effective era overrides.

Useful change: retain ordered properties in generated data and use Phoenix's ordered resolver. Share it across Hotbars and Crossbar. Add a focused regression for the example above before extending the data.

Sources: [Phoenix ordered resolver](https://github.com/phoenixffxi/Phoenix/blob/11c6ee57720a8c6fd79c0fd46ff38ea8e074bf5e/src/map/utils/battleutils.cpp#L3413), [XIUI resolver](../../src/runtime/combat/skillchain_source.cpp), [generated-data builder](../../scripts/build-skillchain-data.py).

Local analysis: [comparison script](../../out/phoenix-audit-20261017/compare_source.py), [results](../../out/phoenix-audit-20261017/comparison.json). Rerun with `python -B out/phoenix-audit-20261017/compare_source.py`.

## 2. Add Phoenix combat data from effective rules, preserving shared data

**Verified data sources; deployment-dependent overrides. High priority.**

The matching base properties do not justify treating the entire Retail preset as Phoenix. The pre-SoA Blood Pact SQL removes skillchain properties from Chaotic Strike, Eclipse Bite, Flaming Crush, Mountain Buster, Predator Claws, Rush and Spinning Dive. Horizon's property table and NPC action IDs cannot substitute for a Phoenix export.

The engine also supplies timing behavior: a normal link requires more than three seconds since the preceding action; a new opener starts with ten seconds; successful links shorten that effect by one second. Magic Burst eligibility checks the current effect, its tier and matching element. XIUI currently offers opener hints immediately and uses a 3.5-second continuation delay and a 200 ms shortened close estimate. Some differences may be deliberate client margins, but should be documented separately from server rules and hint readiness.

Useful change: generate a pinned Phoenix data profile from base data plus the actual deployed overrides. Preserve ordered properties, action categories and action IDs. Keep timing and client safety margins explicit. Retain Horizon as a separate supported profile rather than renaming it.

Sources: [Blood Pact overrides](https://github.com/phoenixffxi/Phoenix/blob/11c6ee57720a8c6fd79c0fd46ff38ea8e074bf5e/modules/era/sql/soa/bloodpact_skillchains.sql), [link timing](https://github.com/phoenixffxi/Phoenix/blob/11c6ee57720a8c6fd79c0fd46ff38ea8e074bf5e/src/map/utils/battleutils.cpp#L3430), [Magic Burst eligibility](https://github.com/phoenixffxi/Phoenix/blob/11c6ee57720a8c6fd79c0fd46ff38ea8e074bf5e/scripts/combat/basic/magic_burst.lua), [module selection](https://github.com/phoenixffxi/Phoenix/blob/11c6ee57720a8c6fd79c0fd46ff38ea8e074bf5e/modules/init.txt).

## 3. Make Phantom Roll labels and base potency Phoenix-aware

**Confirmed semantic differences in conditional era overrides. High priority.**

Phoenix's Corsair module changes more than numeric potency. In the pre-Abyssea path, Healer's Roll gives resting MP recovery, Gallant's Roll reflects physical damage proportionally, and Drachen/Puppet effects also change. XIUI's Retail and Horizon tables do not describe that complete combination.

The base server implementation separately applies party-job/gear bonuses, Phantom Roll+ and COR-level correction, rounding the latter upward. The newer live source also checks the originating COR when another player's existing roll would be replaced or doubled. A static potency number cannot account for all of this from the die total alone.

Useful change: import Phoenix base powers and effect labels for the selected era. Continue labeling unknown modifiers honestly. Keep native buff deadlines authoritative; do not replace them with a fixed 300-second duration. A later extension could model visible modifiers, but should not claim exact final potency when server-only/random context is unavailable.

Sources: [era-specific Corsair behavior](https://github.com/phoenixffxi/Phoenix/blob/11c6ee57720a8c6fd79c0fd46ff38ea8e074bf5e/modules/era/lua/globals/job_utils/corsair.lua), [base roll application](https://github.com/phoenixffxi/Phoenix/blob/11c6ee57720a8c6fd79c0fd46ff38ea8e074bf5e/scripts/globals/job_utils/corsair.lua#L766), [current XIUI roll data](../../src/runtime/modules/phantomroll/phantom_roll_data.cpp).

## 4. Represent Phoenix debuff-duration uncertainty

**Confirmed server formulas; exact remote duration remains unavailable. High priority.**

The Phoenix pre-SoA enfeebling override starts Blind/Blind II at a random **80–300 seconds**, Paralyze/Paralyze II and Silence at **30–120 seconds**, and Poison/Poisonga at **30 seconds**. Subsequent duration modifiers and resistance can change the result. This is materially different from displaying one fixed Retail base duration.

The earlier local-source review cited the shared enfeebling base. That was incomplete for effective Phoenix rules: this override must also be considered. A roughly 38-second observed Paralyze is compatible with this random starting range; the source alone does not prove the calculation behind that specific capture.

Useful change: add Phoenix duration rules that distinguish fixed, ranged and unknown estimates. An estimate tooltip or range can communicate uncertainty without inventing an exact deadline. Confirmed wear-off still removes the effect immediately. Keep actual local-player deadlines from `0x063`; party `0x076` has status IDs without matching expiry timestamps.

Sources: [Phoenix duration override](https://github.com/phoenixffxi/Phoenix/blob/11c6ee57720a8c6fd79c0fd46ff38ea8e074bf5e/modules/phoenix/lua/globals/spells/enfeebling_spell.lua#L92), [local status timestamps](https://github.com/phoenixffxi/Phoenix/blob/11c6ee57720a8c6fd79c0fd46ff38ea8e074bf5e/src/map/packets/s2c/0x063_miscdata_status_icons.cpp), [party status packet](https://github.com/phoenixffxi/Phoenix/blob/11c6ee57720a8c6fd79c0fd46ff38ea8e074bf5e/src/map/packets/s2c/0x076_group_effects.cpp), [current estimate lookup](../../src/runtime/combat/status_durations.hpp).

## 5. Refresh and extend mob information using the new spawn-slot data

**New live source makes this more practical. Medium priority.**

Recent live changes moved NM placeholder information into zone YAML and changed lottery triggering to depend on kills. The current source describes spawn-slot members, chances and cooldowns. Phoenix overlays can replace a zone's whole slot list, so importing only the base YAML would miss effective rules.

Concrete example: the South Gustaberg Phoenix overlay groups Rock Lizard `17215867` with two Leaping Lizzy entries, each with `chance: 8` and `cooldown: 3600`. This is a relationship between a spawn slot and several candidates. It is not a guarantee that Lizzy appears one hour after a player's observed kill.

XIUI already embeds a Phoenix snapshot, but it is pinned to `9b93232a...`, generated October 10. Its record has one `placeholderOf` field, which cannot represent every relationship in a slot with multiple NM candidates. YAML also cannot replace the existing runtime-finalized resistance/level dump, because scripts and modules can modify those values.

Useful change: refresh the finalized Phoenix mob dump; supplement it with a separate, versioned spawn-slot relationship table. This can improve Target's PH/NM information and optionally add map context. Label cooldowns/chances as source data and require observation before presenting any personal timer.

Sources: [South Gustaberg overlay](https://github.com/phoenixffxi/Phoenix/blob/11c6ee57720a8c6fd79c0fd46ff38ea8e074bf5e/modules/phoenix/data/nm_respawn/zones/south_gustaberg/mobs.yaml#L154), [current snapshot metadata](../../assets/mobs/phoenix_mobs.meta), [current packer/schema](../../scripts/build_mob_data.py).

## 6. Reuse the server's recast contract across pet and action displays

**Focused follow-up opportunity; a changed ability recast is not automatically a changed charge duration.**

Phoenix's `0x119` producer explains how charged recasts and `Calc2` encode the difference between base and actual charge time. Charge definitions are loaded from `abilities_charges`, separately from ordinary ability `recastTime`. In the fetched source, the Ready charge row remains three charges at 30 seconds, while an era SQL file changes the Ready ability's `recastTime` to 60. Reading only the latter is insufficient to prescribe a 60-second charge pool.

XIUI hardcodes charge pools in both Action Bars inspection and Recast. Timer 102 is also shared with Sic, and earlier-era ability availability changes how the action should be described. These paths deserve one shared interpretation based on actual client recast data and the effective action rules.

Useful change: centralize charge metadata/decoding, verify `Calc2` signed interpretation once, and cover Sic/Ready, Quick Draw and Stratagems with focused examples. Preserve unknown pet lifetime: the inspected `0x068` pet packet gives HP/MP/TP, target and name, not a remaining jug/charm lifetime deadline.

Sources: [recast packet producer](https://github.com/phoenixffxi/Phoenix/blob/11c6ee57720a8c6fd79c0fd46ff38ea8e074bf5e/src/map/packets/s2c/0x119_abil_recast.cpp), [charge definitions](https://github.com/phoenixffxi/Phoenix/blob/11c6ee57720a8c6fd79c0fd46ff38ea8e074bf5e/sql/abilities_charges.sql), [era ability adjustments](https://github.com/phoenixffxi/Phoenix/blob/11c6ee57720a8c6fd79c0fd46ff38ea8e074bf5e/modules/era/sql/soa/job_adjustments.sql), [XIUI Recast](../../src/runtime/modules/recast/recast_source.cpp), [XIUI action inspection](../../src/runtime/modules/actionbars/action_executor.cpp).

## 7. Improve distant party/alliance markers on the minimap

**Verified packet capability; implementation opportunity.**

Phoenix handles the client map-group request `0x0D2` by visiting same-zone, same-Mog-House-instance alliance members and returning server packet `0x0A0` with member ID, zone and X/Y/Z. The response is not limited to members who have a nearby streamed world entity.

XIUI currently adds party/alliance map markers through entity indices and identity checks. A bounded, expiring cache of map-group responses could supply distant markers when world entities are unavailable. Received coordinates are snapshots, not continuously updated positions. Any refresh policy should be explicit and restrained; this audit sent no requests.

Sources: [map-group request](https://github.com/phoenixffxi/Phoenix/blob/11c6ee57720a8c6fd79c0fd46ff38ea8e074bf5e/src/map/packets/c2s/0x0d2_map_group.cpp), [position response](https://github.com/phoenixffxi/Phoenix/blob/11c6ee57720a8c6fd79c0fd46ff38ea8e074bf5e/src/map/packets/s2c/0x0a0_map_group.cpp), [current minimap source](../../src/runtime/modules/minimap/minimap_source.cpp).

## 8. Use Phoenix's packet definitions to complete Treasure Pool

**Verified protocol support for the remaining Lua parity feature.**

Server `0x0D2` carries the pool slot, item, source and drop start time; `0x0D3` carries lot participants, values, winners and resolution state. Client lot/pass use `0x041` / `0x042`, with slot and game-state validation. The server's normal pool lifetime is five minutes, but items can resolve earlier. Packet directions matter: the client map request numbered `0x0D2` is unrelated to the server loot packet with that number.

Useful change: build the missing persistent pool view from these events, sharing native icons and controller-compatible actions. Preserve old-item timing on reconnect; `StartTime` is a server-process-relative timestamp, so exact countdown reconstruction requires clock reconciliation rather than assigning every received item five new minutes. This is a Treasure Pool feature, not an inventory restriction.

Sources: [pool item packet](https://github.com/phoenixffxi/Phoenix/blob/11c6ee57720a8c6fd79c0fd46ff38ea8e074bf5e/src/map/packets/s2c/0x0d2_trophy_list.cpp), [lot/solution packet](https://github.com/phoenixffxi/Phoenix/blob/11c6ee57720a8c6fd79c0fd46ff38ea8e074bf5e/src/map/packets/s2c/0x0d3_trophy_solution.cpp), [pool lifetime](https://github.com/phoenixffxi/Phoenix/blob/11c6ee57720a8c6fd79c0fd46ff38ea8e074bf5e/src/map/treasure_pool.cpp#L36), [existing Native parity record](implementation/07-treasure-pool.md).

## Beta comparison and recommended order

The audited skillchain resolver has no functional live/beta difference; its file difference is removal of a byte-order mark. The inspected Magic Burst, Corsair/era, Phoenix enfeebling, base weapon/Blue/pet property and loot/map response files also have no live/beta difference. Beta includes additional mob-skill definitions and other changes; those should not be labeled live data or folded into a live export silently.

Recommended sequence:

1. Correct ordered skillchain predictions and document hint timing.
2. Establish the deployed Phoenix rules/era and generate its combat/roll profile.
3. Improve debuff uncertainty using Phoenix's ranged duration formulas.
4. Refresh mob data and model the new spawn-slot relationships.
5. Consolidate recast interpretation, then consider distant map markers and Treasure Pool.

The first three improve accuracy in features already installed. The remaining items improve data freshness or add capability. None requires removing Horizon support or implementing inventory restrictions.
