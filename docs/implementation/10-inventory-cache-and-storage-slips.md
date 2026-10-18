# P10: Offline inventories and storage slips

Status: **Implemented first pass**. Focused source/cache/slip checks and the native runtime build passed; multi-character gameplay acceptance remains with Cody.

`satchel_source` publishes settled records with per-item generations and all 28 native extra bytes. `satchel_cache` keeps up to 64 world/character snapshots, each in its own atomic asynchronous file with last-observed wall time and local visual layouts. Incomplete loading cannot replace valid data; independent writers prevent different clients from overwriting other characters. The Satchel picker reuses search, paging and tooltips for read-only cached bags and all 28 Porter Moogle catalogs. Unknown/truncated slip data produces no fabricated items. Cache/layout files stay outside portable profiles. MIT attribution from MalRD's Find catalog is embedded. See [Satchel](../satchel.md).

Audit date: October 15, 2026. Priority: Medium after P08.

[Parity audit](../feature-parity.md) · [Plan index and shared contract](README.md)

## Original audited gap

Native does not cache other characters' inventories/gil or decode and browse Porter Moogle storage slips.

## Source evidence

- Lua reference: `XIUI/modules/satchel/{altcache,slipslogic,layoutstate,searchlogic}.lua`.
- Lua reference: `XIUI/modules/satchel/init.lua: alt picker and slip windows`.
- Native integration: [src/runtime/modules/inventory/inventory_source.hpp](../../src/runtime/modules/inventory/inventory_source.hpp).
- Native integration: [src/runtime/notifications/inventory_tracker.hpp](../../src/runtime/notifications/inventory_tracker.hpp).
- Native integration: [src/runtime/settings/settings_writer.hpp](../../src/runtime/settings/settings_writer.hpp).

Lua paths are relative to the reference repository identified in the parity audit; brace lists name multiple files.

## Scope

- Local per-character settled inventory/gil snapshots, a character picker, read-only bag search/tooltips and last-observed metadata.
- Storage-slip catalogs, extended-data bit decoding, paged stored-item views and independent visual arrangement, including cached characters' slips.

## Dependencies

P08 item browser and rich item records. P09 must explicitly reject cached/slip action targets.

## Original implementation plan

1. Inspect Lua altcache identity and slip mappings, preserving source attribution. Define a versioned local cache keyed by stable server/character identity with observation time, containers and supported extended item data.
2. Capture only settled inventory generations; never replace a complete cache with a partially loaded login/zoning snapshot. Write atomically with bounded/debounced IO outside rendering.
3. Port slip catalog and decoding into a pure bounded helper, retaining empty/unknown/versioned behavior when a slip ID or payload is unsupported.
4. Reuse P08 for character and slip views with a permanent read-only source flag. Store their layouts locally and keep observation caches out of portable HUD profiles/share keys.
5. Handle old/corrupt cache files without losing current live data; label snapshots as last observed rather than live cross-character access.

## Input and controller behavior

Character/slip pickers, page changes, search and tooltips use shared controller navigation. Read-only views expose no game-changing context action.

## Completion criteria

- [ ] A partially loaded character cannot erase a previously valid snapshot; characters on different servers remain distinct.
- [ ] Slip bit fixtures produce the expected stored-item pages and reject short data.
- [ ] Cached views clearly retain their snapshot time and cannot reach P09 dispatch.

## Focused validation

`xiui_satchel_data_smoke` passed cache/extra/layout round trips, incomplete/ambiguous identity rejection, wall timestamps, independent concurrent-client records, writer draining and all 28 slip catalogs with truncated-payload rejection. UI checks cover read-only cached/slip actions. Native runtime build passed. The unchecked criteria above retain Cody's multi-character live acceptance work.

Apply the [shared implementation contract](README.md#shared-implementation-contract), then update this plan and the audit when the feature is integrated.

