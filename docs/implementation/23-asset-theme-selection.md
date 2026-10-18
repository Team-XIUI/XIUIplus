# P23: Status and job icon themes

Status: **Implemented**. Runtime/release build, focused checks and isolated standalone startup passed; appearance acceptance remains for Cody.

Audit date: October 15, 2026. Priority: Lower; customization parity.

[Parity audit](../feature-parity.md) · [Plan index and shared contract](README.md)

## Current gap

Native now offers global status and job icon choices, plus independent status-list overrides. Current native status art and XIVParty job icons remain the defaults. The requested scope is icons only; background themes and cursor or selection artwork are deferred.

## Source evidence

- Lua reference: `XIUI/config/global.lua: Status Icon Theme and Job Icon Theme`.
- Lua reference: `XIUI/config/partylist.lua: Status Theme`.
- Lua reference: `XIUI/handlers/statushandler.lua: theme discovery`.
- Lua reference: `XIUI/assets/{status,jobs}/` at revision `a9e8d0c93710e5838749f8afc36f0058451c9a7e`.
- Native integration: [src/runtime/ui/game_icon_cache.hpp](../../src/runtime/ui/game_icon_cache.hpp).
- Native catalog: [src/runtime/ui/icon_themes.hpp](../../src/runtime/ui/icon_themes.hpp).
- Shared status controls: [src/runtime/ui/status_strip.hpp](../../src/runtime/ui/status_strip.hpp).
- Native integration: [src/runtime/modules/party/party_module.cpp](../../src/runtime/modules/party/party_module.cpp).
- Native integration: [assets/jobs/README.md](../../assets/jobs/README.md).
- Imported source assets and notices: [assets/icon-themes/README.md](../../assets/icon-themes/README.md).

Lua paths are relative to the reference repository identified in the parity audit; brace lists name multiple files.

## Scope

- Global status choices: Native, HD, Tetsouou and XIView.
- Global job choices: XIVParty, Classic, FFXI and FFXIV-1. Party, Alliance and Ready Check share the job choice.
- Status-list choices: Follow global or an explicit Native, HD, Tetsouou or XIView override. Target, Enemy List, Buffs, Debuffs and Party expose these through their status controls.
- Party's combined list retains its own override. Split Buff and Debuff rows each retain an independent override. Debuffs can inherit Buffs presentation through the existing Follow Buffs settings control.
- Background themes and cursor/selection artwork are outside this implementation.

## Dependencies

The fixed catalog uses the existing game-icon cache and shared status renderer. No external theme directory or plugin system is required.

## Implementation

1. Imported the three status and three job packs from the local Lua XIUI source. Original PNGs and pack credits remain in source ZIPs, together with the upstream GPL-3.0 license and concise attribution notices. Existing XIVParty licensing remains intact.
2. Packed status art into three fixed 1024×1024 atlases and job art into three 512×256 atlases. The retained builder regenerates these and status-presence metadata. Smaller status images receive transparent padding; 32-pixel job art expands with nearest-neighbor sampling into existing 64-pixel cells.
3. Cached all embedded atlases once per device/resource reset. Changing a choice selects a cached texture; it performs no file IO or texture upload during drawing. Resource IDs 800–805 hold art, and 806–807 hold license and notices.
4. Added General → Appearance → Window choices, saved in `[appearance.icons]` as `status_theme` and `job_theme`. Shared status strips save `icon_theme`; Party's combined list saves `status_icon_theme` in its existing Party section. Catalog positions remain stable.
5. Kept native art for missing effects or unavailable status packs. Alternate job atlases include the existing XIVParty art for jobs absent from the selected pack, and the cache uses XIVParty if an entire job atlas is unavailable. Unknown saved choices remain saved while rendering the default art.

## Input and controller behavior

Theme choices use the existing shared choice controls, including keyboard, mouse and controller navigation. Icon artwork changes preserve status hit targets, tooltip focus, timer text profiles and existing layout settings.

## Completion criteria

- [x] Global status and job choices retain the current Native/XIVParty defaults.
- [x] Per-list and split-row status overrides persist independently of geometry.
- [x] Missing effects/packs use current native fallbacks without repeated per-frame IO.
- [x] Original source assets, license and pack credits are retained.
- [x] Existing window art and controller focus preferences remain independent.
- [x] Runtime/release build and focused icon-theme fixture passed.
- [ ] Cody's in-game appearance check.

## Focused validation

[tests/icon_theme_checks.cpp](../../tests/icon_theme_checks.cpp) exports `RunIconThemeChecks()`, which passed through `xiui_config_input_smoke --new-features-only`. It covers catalog/fallback decisions, unknown saved choices, global preferences, standalone Buffs/Debuffs, Party combined/split overrides and preserved strip geometry. Runtime/release build and isolated single-DLL startup passed. Cody checks appearance.

Apply the [shared implementation contract](README.md#shared-implementation-contract), then update this plan and the audit when the feature is integrated.

