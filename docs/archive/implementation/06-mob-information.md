# P06: Mob information

> Historical record. Use the [current documentation](../../README.md) for implementation guidance.

Status: **Implemented** in the Native working tree. Build/install results are recorded in the audit.

Updated October 15, 2026. Priority: Medium.

[Parity audit](../feature-parity.md) · [Plan index](README.md)

## Delivered behavior

Mob information is integrated into **Target**, backed by a shared database. **Target → Mob Info → Common** owns its master switch, optional name-level label, row spacing and shared typography. Sub-tabs select **Behavior**, **Links**, **Detection**, **Physical damage**, **Element damage**, **Element ranks**, **Status ranks**, **Immunities**, **Server ID** and **Spawn interval**. Subtarget and Target of Target retain their own optional level labels.

Each group has its own visibility, **Together / Own row / Independent** placement, width, icon size, cell spacing, wrap spacing and X/Y offsets. Damage/rank/immunity headings can be hidden. Text follows Common by default; turning that off enables the group's full shared text profile. Group offsets move icons and text together. Together continues the preceding row; Own row starts a new line. Defaults preserve the old arrangement and migrate the old behavior/damage/rank switches into their corresponding groups.

The October 15 22:40 default capture enables additional information with Cody's selected groups and presentation; existing installations keep their own choices. Attached rows follow Target and contribute to its measured footprint and linked-child placement. Independent groups leave that footprint and use the existing drag, snap, Center X/Y and controller Movement controls, with separately saved positions. First detachment starts at the last attached position; reattaching retains a customized independent position. Independent groups hide on target loss, unknown data, Target disable or contextual hiding. Only groups with available target data enter Movement. Clicking a detached group opens its Target → Mob Info sub-tab. There is no separate top-level Mob Info category or browsing-focus surface.

Common and per-group text controls live together in Mob Info; the duplicate Text → Mob info selector is removed. Shared controls provide mouse, keyboard and controller access. Legacy standalone display choices remain fallbacks, while current Target/group choices win. Retired standalone position/size never move Target.

Only validated monster entities in the current zone receive dataset records. Unmatched IDs, wrong-zone records, players and NPCs add no invented information or empty panel. Job/family and exact live levels are absent from the dataset and are omitted.

Damage, elemental ranks, status ranks and immunities use icon/value cells. Element/status artwork comes from the installed client; original XIUI/MobDB physical-damage symbols are embedded with their MIT notice. Light Sleep and Dark Sleep use native Sleep artwork with different element badges. Cells wrap within the selected width; unavailable artwork falls back to text. Common or group text effects and offsets remain intact. The HUD has no snapshot/source footer; provenance stays in documentation and notices.

Behavior, Links and Detection use original XIUI/MobDB icons: Passive/Aggressive (including NM variants), Links, Sight/True Sight, Sound, Scent, Magic, Abilities and Low HP. True sound uses the reference's Sound artwork. Only recorded flags add icons, with no Detects heading or reserved label padding. Placeholder text remains available where recorded.

## Data and provenance

[HeadsUp](https://github.com/Tanyrus/headsup/tree/80320a89a08290389496d5534bfaa8c3f3339d51) supplies the inspected Phoenix map-server dump: **64,702 mobs across 216 zones**, generated October 10, 2026, from Phoenix commit `9b93232a0cc1e4a5a50b0f988f37d7ae63b8fcfa`. This reflects that server snapshot; it does not reveal hidden current server values.

The build tool packs sorted fixed-size records and a deduplicated name table into the embedded resource. Runtime lookup uses binary search. It performs no online requests, YAML parsing, server queries, name fallback or respawn estimation. Target/Subtarget/recipient controls reuse the validated lookup and omit unavailable levels.

Damage multiplier is `1 + SDT / 10000`. Elemental/status resistance ranks retain signed values and remain separate from damage adjustment and explicit immunity flags. Light Sleep and Dark Sleep have separate ranks and immunity bits. The spawn interval is captured base data without a countdown.

## Implementation files

- [Database and formats](../../../src/runtime/modules/mobinfo/mob_database.hpp)
- [Target information settings/rendering](../../../src/runtime/modules/target/target_mob_info.hpp)
- [Native Target/child integration](../../../src/runtime/modules/target/target_module.cpp)
- [Snapshot, regeneration and GPL notices](../../../assets/mobs/README.md)
- [Build tool](../../../scripts/build_mob_data.py)
- [Focused lookup/layout/settings checks](../../../tests/mob_info_checks.cpp)
- [HeadsUp/native data findings](../native-mob-data-findings.md)

Single-DLL packaging embeds data, provenance and licenses; users need no source checkout or Python. Runtime seeds the notices beside its other bundled notices.

## Completion criteria

- [x] Offline server-ID lookup; unknown mobs remain unknown.
- [x] Separate resistance ranks, damage adjustments and sleep immunity flags.
- [x] Target-owned text, measured layout, visibility and shared config navigation.
- [x] Common and ten group sub-tabs, with optional independent rows/movement.
- [x] Legacy preferences migrate without importing retired standalone positions.
- [x] Single-DLL packaging includes the snapshot and required notices.

`xiui_config_input_smoke --mob-info-only` covers known/unmatched IDs, zone/entity validation, signed fields, damage/element/status/behavior/detection icon mapping, NM/true-detection/NoAggro flags, distinct sleep immunity, removal of headings/footer, legacy migration, group routing, measured layout, independent position/size/offset persistence and reattachment. It also covers World Target Bar settings/projection. Native appearance/gameplay acceptance remains for Cody.
