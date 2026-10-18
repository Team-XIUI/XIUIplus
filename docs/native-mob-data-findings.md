# Native mob data and status timers

Researched October 15, 2026 for the existing-module cleanup. Shared mob information embeds the pinned dataset for Target and its child views; it is not a separate HUD module. The data distinctions below remain authoritative.

## HeadsUp reference

The FFXI plugin is [Tanyrus/headsup](https://github.com/Tanyrus/headsup), inspected at commit [80320a89a08290389496d5534bfaa8c3f3339d51](https://github.com/Tanyrus/headsup/tree/80320a89a08290389496d5534bfaa8c3f3339d51).

Its mob information comes from a pinned Phoenix map-server run, rather than client memory or YAML alone. The [dump module](https://github.com/Tanyrus/headsup/blob/80320a89a08290389496d5534bfaa8c3f3339d51/tools/phoenix/headsup_dump/headsup_dump.cpp) visits loaded mobs, runs otherwise unspawned mobs through the server's spawn path, and captures script-adjusted attributes. [refresh.sh](https://github.com/Tanyrus/headsup/blob/80320a89a08290389496d5534bfaa8c3f3339d51/tools/phoenix/refresh.sh) performs the offline server/database work; it is a data-maintenance tool, not something the client plugin runs.

The inspected [metadata](https://github.com/Tanyrus/headsup/blob/80320a89a08290389496d5534bfaa8c3f3339d51/data/phoenix_mobs.meta) pins Phoenix commit 9b93232a0cc1e4a5a50b0f988f37d7ae63b8fcfa and records 64,702 mobs across 216 zones from five dumps. That snapshot is not proof of the current live server's state.

[MobRecord](https://github.com/Tanyrus/headsup/blob/80320a89a08290389496d5534bfaa8c3f3339d51/src/mobdata.h) contains server ID, name, level range, aggression/detection flags, respawn interval, level modifier, placeholder relationships, resistance/immunity references and drops. [MobById](https://github.com/Tanyrus/headsup/blob/80320a89a08290389496d5534bfaa8c3f3339d51/src/mobdata.cpp) uses a sorted compiled table and binary search. Native Mob Info now follows that simple model with an embedded pinned snapshot and exact server-ID lookup; its selected fields and availability limits are documented in [P06](implementation/06-mob-information.md).

Do not copy the previous proposal's name-fallback/provider framework by default. A renamed client mob does not change its server ID, and another server can use different data. Keep unknown records unknown. Preserve HeadsUp/Phoenix GPL notices and asset attributions if reusing their data or code.

## Which status data the client actually has

The installed Ashita SDK exposes the following distinct sources:

- IPlayer::GetStatusIcons and GetStatusTimers supply paired local-player icon IDs and real timer timestamps. GetBuffs supplies the current local icon list. Reuse the existing native clock conversion in [experience_source](../src/runtime/modules/experience/experience_source.cpp); do not reconstruct duration from spell tables or use PC-clock guesses.
- IParty::GetStatusIconsServerId, GetStatusIconsTargetIndex, GetStatusIconsBitMask and GetStatusIcons expose the five other members' packed icon IDs. These are actual observed statuses, but the party packet does not include each effect's remaining duration.
- IResourceManager::GetStatusIconByIndex/GetStatusIconById and buffs.names provide real client artwork, descriptions and names. They describe effects; having a resource record does not prove that an entity currently has the effect.
- IEntity does not expose a complete arbitrary-mob buff/debuff list or per-effect expiration timestamps in the inspected SDK. Confirmed action/removal messages can establish individual observed mob effects. A base-duration table cannot turn that observation into an exact timer.

The server sources agree. Phoenix [0x063 Status Icons](https://github.com/phoenixffxi/Phoenix/blob/fbdcc318853b6fda31c03149499049dd6c2eb738/src/map/packets/s2c/0x063_miscdata_status_icons.cpp) builds the local player's icon/timestamp pairs in 60 Hz units, using 0x7FFFFFFF when an effect has no visible timer. [0x076 Group Effects](https://github.com/phoenixffxi/Phoenix/blob/fbdcc318853b6fda31c03149499049dd6c2eb738/src/map/packets/s2c/0x076_group_effects.cpp) sends member identity, icon high bits and icon bytes, with no duration list. The inspected local Phoenix tree is commit fbdcc318853b6fda31c03149499049dd6c2eb738; it differs from HeadsUp's dataset pin.

Consequently, real local expiry times and remote estimates must remain distinct. Native access does not reveal server-only remote deadlines. Following the investigation below, Cody requested the original XIUI database as the primary duration catalog for combat observations. The implemented catalog uses fixed literal durations, identifies estimates in hover tooltips, and clears them on observed removal or estimated expiry. Party snapshots still provide IDs without deadlines. Gear/merit/JP/TP formulas and inferred secondary effects remain outside this implementation.

## Implemented: original XIUI timer catalog

`assets/status/debuff_retail.lua` retains the exact imported source from the local tirem/XIUI checkout, with attribution and GPL license. `scripts/build-status-rules.py` imports fixed spell, weapon-skill, ability, pet, additional-effect and fallback durations into an embedded native header. Runtime lookup uses separate action namespaces and per-effect fixed overrides, then XIUI's known status-duration fallback. No Lua runtime or external data files are needed in the game.

Confirmed gain messages start or refresh estimates; resists, no-effect messages and damage alone do not. Early wear-off/removal/death wins over a later estimate. Estimated expiry removes stale observed icons, including self-buffs whose wear-off was not sent to this player. Missing entries stay untimed. Actual local-player timer pairs retain their exact native deadlines and authoritative local/party lists still decide presence. The historical capture sections below explain why these database timers are estimates.

`xiui_module_parity_smoke --status-timers-only` checks Paralyze/Blind/Blaze Spikes lookup, action-ID separation, per-effect overrides, estimate labels, native/unknown distinctions, no-effect/resist/damage rejection, confirmed refresh, the captured early Paralyze removal and estimated expiry.

## October 16 timer recheck: Paralysis and Blindness

Rechecked the current Native source, original Lua XIUI, installed Ashita SDK, and local Phoenix checkout at `fbdcc318853b6fda31c03149499049dd6c2eb738`. A temporary read-only addon was loaded and unloaded through AgentLink in the active development client. It did not apply effects, change settings or alter packets. The report is retained locally in `out/status-timer-probe.txt`.

- Installed status resources for Paralysis (4), Blindness (5), Poison, Silence, Sleep, Petrification, Stun, Bind, Weight and Slow all reported `HideTimer=0`. Their artwork metadata is not suppressing countdowns.
- Phoenix's `data/status_effects.yaml` also leaves `hide_timer` unset for Paralysis and Blindness. When either has a finite duration on the local player, the inspected `0x063` builder sends its actual expiry. Runtime effect flags can still hide an individual timer.
- The live client clock was readable. Only Transcendency (434) was active during this inspection, with raw timestamp `0x7FFFFFFF`, the explicit no-timer/hidden sentinel. Paralysis and Blindness were not active, so their actual received deadlines were not sampled in this session.
- Native's `party_source` pairs local IDs/timestamps, checks the clock and resource flag, and supplies valid deadlines to Buffs, Debuffs, the local Party row and local Target views. Missing native deadlines do not produce `?` in these snapshots. The shared marker requires combat-observation evidence or an explicit uncertain record.
- Before the later XIUI catalog change, `combat_source::ApplyStatus` deliberately stored `expires=0` for every enemy observation. `ui::StatusTimerText` displayed `?` when Unknown timer markers was enabled. This caused the originally reported unknown enemy Paralysis/Blindness countdowns.

The durations do exist in the server's `CStatusEffect`, including its start time and final duration. The inspected enfeebling code uses base values of 120 seconds for Paralyze and 180 seconds for Blind, then applies relevant duration modifiers and the server's resist result before adding the effect. Those base numbers are not an exact remaining timer. They also do not cover all abilities, items and additional effects that apply the same status.

No missed expiry field was found in the relevant native packets. [0x063 type 9](https://github.com/atom0s/XiPackets/tree/main/world/server/0x0063#type-0x09---status-icons) contains local icon/timestamp pairs; [0x076](https://github.com/atom0s/XiPackets/tree/main/world/server/0x0076) contains the other five party members' icon IDs without timestamps. In [0x028](https://github.com/atom0s/XiPackets/tree/main/world/server/0x0028#info), the spell action's time field is recast time. It is not the duration of the status on its recipient. Confirmed gain/removal messages establish observed presence, not expiry.

The original Lua `handlers/debuffhandler.lua` obtains enemy countdowns from `handlers/database/debuff_retail.lua`, optional server overlays, and modifier formulas. These are modeled durations, not another native expiry source.

The existing Unknown timer markers control can hide remote `?` labels. Exact remote countdowns would require an additional server message carrying authoritative expiry data. The subsequent XIUI catalog implementation above supplies explicitly labeled estimates instead; it does not change this source limitation.

## Live Paralyze application and wear-off

A separate AgentLink capture on October 16 followed Cody's actual Paralyze cast on the same Valkurm Goblin Gambler (`17199368`). The monitor was ready at 11:39:45 and recorded:

- **11:39:58 application:** `0x028`, actor Shuu (`6282`), category 4, spell 58, target `17199368`, parameter 4 (Paralysis), message 236 (successful enfeeble), modifier 0. Action info was 8, the spell recast in seconds; no effect expiry was supplied.
- **11:40:36 removal:** `0x029`, actor/origin `6282`, target `17199368`, parameter 4, second parameter 0, message 204 (no longer afflicted). This supplies the end event, not an earlier countdown deadline.
- The observed application-to-removal interval was **38 seconds**. This is a retrospective packet interval, not a received timer or proof of the server's duration calculation. The capture contained one successful Paralyze application before this removal.
- Silence also landed at 11:40:05, with spell 59, effect 6, message 236, modifier 0 and recast 8; it did not supply a remote expiry either.

This verifies the remote gain/removal path with a real player cast. The base Paralyze duration of 120 seconds would not have matched this observation, and the 8-second action field is not its lifespan. A precise future countdown still needs authoritative server data. The temporary addon was unloaded and removed after capture. Raw evidence is retained locally in `out/status-packets-paralyze-goblin.txt`, with the extracted pair in `out/paralyze-live-findings.json`.

## Goblin Gambler: Blaze Spikes follow-up

On October 16, AgentLink identified the live target as Goblin Gambler, server ID `17199368`, index 264, in Valkurm Dunes. The local Phoenix zone data maps this ID to the level 22–23 BLM/BLM Goblin Gambler template. Its zone script adds no special Blaze Spikes duration logic.

The normal Blaze Spikes spell (249) applies effect 34. Phoenix's `scripts/globals/spells/enhancing_spell.lua` supplies a 180-second base, then resolves enhancing-duration modifiers and applicable effects before storing the final duration on the server. The original XIUI `durations.mobBuff[34] = 180` fallback is also a modeled duration. Neither is a received expiration timestamp. An observed successful cast can support a labeled estimate; discovering an already-active buff cannot establish when its 180-second window began.

A temporary read-only packet capture followed this exact target without changing gameplay, the target or UI preferences. At 11:33:36, the Goblin's Bind completion carried effect 11 and an action-info value of 40 (spell recast). A separate `0x063` local status snapshot then supplied effect 11 with raw expiry `3992216660`; a later snapshot removed it. At 11:34:00 the Goblin's Choke completion likewise carried effect 130 and recast 10, followed by a local snapshot with raw expiry `3992218640`. These directly demonstrate the extra expiry information supplied when the recipient is the local player. They do not supply a timer for a status on the Goblin.

No successful Paralyze application or Blaze Spikes cast was observed during this capture. The pre-existing Blaze Spikes observation therefore has no application time established by this probe. The capture addon was unloaded and removed; the local packet report is retained at `out/status-packets-goblin-gambler.txt`.

There is an additional remote-tracking limitation in `CStatusEffectContainer::RemoveStatusEffect`: the non-player removal path sends the wear-off notice to the originating player, if that player exists in the zone and can see the entity. For a mob's self-cast buff, the origin is the mob, so that player lookup supplies no notification. Consequently, a combat-observed Blaze Spikes icon can remain after its actual expiry when no removal/dispel/death/reset observation reaches this client. The observed icon is not a complete current server status snapshot.

An exact current Blaze Spikes countdown requires a server-provided expiry. A derived 180-second countdown should start only from the observed successful application and must be labeled as estimated; it must not restart merely because the icon is displayed or spikes damage occurs. No such countdown was enabled by this investigation.

## What HeadsUp's mob timers mean

[ph_timers](https://github.com/Tanyrus/headsup/blob/80320a89a08290389496d5534bfaa8c3f3339d51/src/ph_timers.cpp) handles lottery-placeholder respawns, not buff expiration:

1. An observed defeat message identifies a known placeholder or an NM associated with placeholders.
2. Its due time is observation time minus the one-second defeat-message delay, plus the fifteen-second death-to-despawn interval and the dataset's respawn interval.
3. Seeing the entity alive clears that timer. An overdue entry displays “up” temporarily, then expires.

The local Phoenix [mob death path](https://github.com/phoenixffxi/Phoenix/blob/fbdcc318853b6fda31c03149499049dd6c2eb738/src/map/entities/mob_entity.cpp) uses a fifteen-second death state and queues its defeat/reward handling after m_DropItemTime, initialized to one second. The respawn schedule and lottery choice remain server state. A countdown derived from those rules is an observed-death estimate, not a native per-mob expiration value; unseen deaths and server changes invalidate assumptions.

Observed-death respawn/placeholder countdowns remain outside this change. Mob Info shows only the optional captured base spawn interval, clearly labeled without a countdown.

## Backlog changes

- [P06 Mob Info](implementation/06-mob-information.md): the pinned database is shared by Target/child level labels and optional Target information rows, without a separate module or provider framework.
- [P13 status data](implementation/13-debuff-tracking-rules.md): actual client lists/local timers and confirmed combat observations with the requested XIUI duration estimates.
- [P14 status presentation](implementation/14-status-controls-and-tooltips.md): use client icon/name/description resources and truthful timer availability.
- [P21 visibility](implementation/21-shared-visibility-rules.md): consolidate the existing context rules instead of adding a second hide system.
