# XIUI Lua setup migration plan

> Historical record. Use the [current documentation](../README.md) for implementation guidance.

This is a task-scoped migration record. Its historical scope selections are not standing authorization for unrelated additions or live probes. Current behavior is documented in the feature guides linked from [the index](../README.md).

Updated October 17, 2026. Capability audit complete; the user selected the action/macro, palette/crossbar, action-bar, pet, song-duration and manual Fast Cast gaps for implementation. Other gaps are deferred. See the [migration capability audit](lua-profile-migration-audit.md) for the concrete differences and suggested priorities.

## Scope

Provide migration for the whole XIUI Lua setup: every module's preferences and placement, all profiles, character associations, action bars and libraries, bindings, supported resources and XIUI-owned saved data. Hotbars and crossbar are one part of this work.

Use existing Native features. A missing Lua equivalent is a decision to present, not automatic authorization to invent or expand Native features. The earlier plan's requirement to implement every missing capability is superseded. Only the selected capabilities in the audit scope decision are authorized Native additions. Background/font/artwork, notification/treasure, party/enemy, HP-band, Mob Info and Satchel extras remain deferred.

For every meaningful persisted preference, record whether it can be converted exactly, needs a supported approximation, has no chosen Native equivalent, or is blocked by missing source data. Explain the last three in the preview and final report. Do not silently lose action data or present visual substitution as exact preservation.

Retain Horizon support. Do not add inventory restrictions. Native-only modules retain normal defaults. Transient casts, live notifications, held inputs, open dialogs and running macros are not settings to recreate.

## Verified release baselines

Support [1.8.4 stable](https://github.com/tirem/XIUI/releases/tag/v1.8.4) and [1.8.5 beta](https://github.com/tirem/XIUI/releases/tag/v1.8.5). Research used actual regular release ZIPs downloaded October 17, including the beta asset updated October 9:

- `XIUI-1.8.4.zip`: 31,972,712 bytes; SHA-256 `138d9236b5571fbd0693a4b8dc66ba723408014388d75311085f5a2a553d2be4`.
- `XIUI-1.8.5.zip`: 33,002,718 bytes; SHA-256 `80347ea06ba1f74223ce6f2b8980563f603da52da19c82b7f813f89bdd84bfaf`.

Sources and research artifacts are under ignored `out/lua-migration-research/`. Generated writer fixtures and expanded-default inventory are in its `audit-artifacts/` directory. Earlier draft code is archived separately there; it is not part of the plugin or an approved implementation.

## Current progress

The approved Native destinations are implemented, packaged and installed; the audit records the focused checks and runtime confirmation. The existing action importer now handles real Lua saved profiles and the selected action metadata. The full-profile discovery/conversion/preview workflow below remains to be implemented. Deferred capabilities stay deferred.

## 1. Finalize the field and data manifest

Inventory both versions' expanded defaults, factories, migrations, configuration controls and actual read/write sites. Include dynamic collections, runtime-created preference keys and data outside profiles. Resolve legacy aliases with the source's precedence rules. Counts of default leaves are not import coverage percentages.

Cover Player; Target/Subtarget/Target of Target; Mob Info; Enemy List; party/alliance lists and templates; Pet/Pet Target; Casting/Cast Cost; EXP/Gil; every storage tracker; Satchel; Notifications/groups; Treasure Pool; Ready Check; Blue Magic; Phantom Roll; all action bars/palettes/macros/bindings; Skillchain/Magic Burst; profile/character/layout state; compatible resources and saved inventory/slip data.

Record source path/type/default/unit/version/inheritance, existing Native destination, conversion disposition and a focused verification case. Keep source-specific logic in the import layer. Implement only the selected Native changes recorded in the audit scope decision.

## 2. Build one restricted reader and discovery path

Reuse the action importer's data-reading foundation. Accept returned literal tables and the actual writer's restricted assignment format, including `userSettings`, numeric/string keys, decimal byte escapes and escaped newlines. Parse data without executing Lua, functions or addon code.

Folder discovery finds all profiles and ordering, character selections, modern `windowPositions`, relevant legacy XIUI ImGui positions, Satchel caches and referenced resources. A single-profile import reports related data it cannot discover. Pin the source version and inspect a consistent snapshot.

Use measured bounded limits and explicit errors. Respect existing Native capacities; do not enlarge them implicitly or silently truncate a source that exceeds them.

## 3. Convert through existing foundations

Use explicit module adapters with shared color, enum, scale, text, meter and visibility conversions. Start with embedded Native defaults, then apply supported effective Lua settings. Resolve global/local overrides and selected layout templates before converting.

Translate saved positions and geometry through existing movement/clamping behavior. Account for source scale, padding, attachment and anchoring. Report viewport adjustment; do not invent a source resolution.

Keep assignments across all ten hotbars, stored slots, six supported crossbar banks, job/subjob/pet layouts and disabled bars. Preserve typed action identity, targets, labels, supported icons, macro commands/waits and explicit empty slots. Import reusable text records and references that fit the existing model. Surface unsupported typed-library metadata, separate palette order and Shared-bank conflicts according to the selected scope; never silently merge unrelated records.

Translate logical key/controller bindings separately from device-local mappings. Report collisions and incompatible hardware selections. Commands and native game macro references are data only; do not execute them during import.

Convert profile preferences separately from historical Satchel caches. Handle 1.8.4 character settings and 1.8.5 `inventory_cache.lua`; preserve item/container identity, counts, gil and slip bytes where available. Choose the unknown-age/capacity policy before implementation. Do not fabricate metadata or replace newer Native observations.

Copy custom sounds only when compatible with existing Native WAV support. Report missing files and unsupported artwork/fonts; adding loaders or new formats requires a separate choice. Keep the distribution as one standalone DLL with optional user-owned config/resources.

## 4. Add the import workflow

Use **Import from XIUI Lua** in the profile three-dot menu: select detected install/folder/file, select profiles and related data, review the conversion report, then import as new profiles. Offer optional activation after success. The existing action import entry can reuse the same pipeline for an actions-only selection.

Use shared Native controls, keyboard actions and controller navigation. Preserve the current limits of text entry rather than inventing a new input system.

Preflight the entire bundle before committing. Handle duplicate names and Lua Default without overwriting Native Default. Stage writes, publish registrations only after dependencies succeed and preserve existing profiles/settings/positions on cancellation or failure. Keep source fingerprints and destination IDs so retrying does not accidentally duplicate a batch.

Full-setup discovery remains the default even if some settings are explicitly unsupported. Partial selection is an option, not a replacement for auditing every module. Save an itemized migration report with exact conversions, substitutions, omitted preferences and source problems.

## 5. Verify the chosen migration scope

Offline writer-format characterization has run; the live Lua-to-Native journey has not. Once scope is chosen and implementation is ready:

1. Confirm the active Phoenix install through AgentLink. Back up current Lua/Native settings and record loaded state. Preserve startup configuration and concurrent user activity.
2. Unload Native before loading the pinned Lua addon because both own `/xiui`. Create disposable populated profiles through the real addon/save path, covering every module and additional profiles for mutually exclusive options. A temporary fixture helper may use the real defaults/profile APIs.
3. Include all action types, shared/unassigned records, job/subjob/pet layouts, banks, differing palette orders, cleared slots and bindings. Use harmless echo commands for any execution checks. Do not perform automated combat, loot or inventory actions.
4. Save/reload Lua, capture actual files, then import through the Native UI. Compare supported values and references against the manifest. Confirm unsupported choices appear in the report instead of silently disappearing. Save/reload Native and verify persistence.
5. Perform focused checks for actual conversion failures, cancellation, malformed files, name collisions and missing dependencies. Check representative geometry and the import dialog's controller path as part of this migration test; leave ordinary visual/gameplay review to the user.
6. Install the finished standalone DLL into the active setup and confirm it through AgentLink. Restore the user's prior loaded/profile state while preserving any newer user activity. Keep fixtures/reports available; remove temporary diagnostic helpers.

Build only affected targets and the needed standalone integration. Do not turn this into a broad test or gameplay sweep.

## Delivery gate

The chosen scope is complete when all meaningful source preferences are classified; supported conversions survive reload; action/reference losses are prevented or explicitly resolved; the preview accurately describes remaining differences; historical data keeps honest provenance; and cancellation/failure preserves the existing setup.

The final report must state which live checks actually ran. New Native feature work remains a separate user decision based on the audit.
