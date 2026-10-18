# P04: Action palettes, macros and imports

Status: **Implemented** in the Native working tree; live macro/action acceptance remains with Cody.

October 16 implementation: named palette CRUD/reordering/cycling, independent hotbar/crossbar selection, global/main-job/main+subjob storage with fallback, pet overrides, shared macro library, native macro references and previewed imports. Macro sequences support waits and native subtarget handoff. Original XIUI literal data, tHotBar/tCrossBar assignments and native INI macro text are parsed without running Lua. Imports create new palettes and preserve supported source job/subjob context. See [Action Bars usage](../actionbars.md).

Code: `src/runtime/modules/actionbars/{action_store,action_import,action_executor,native_macros}.cpp`. Action data follows portable profiles; controller device/replacement preferences and all transient input/execution state remain local. `xiui_actionbars_data_smoke` covers scope/pet fallback, CRUD/cycling, shared macro references, persistence and import rejection boundaries.

Audit date: October 15, 2026. Priority: High after P02.

[Parity audit](../feature-parity.md) · [Plan index and shared contract](README.md)

## Current gap

The original gap was action data beyond HUD presentation profiles. The implementation now adds palettes, shared macro entries and literal-data imports without changing XIUI2's immutable share dictionary.

## Source evidence

- Lua reference: `XIUI/modules/hotbar/{palette,petpalette,petregistry,macropalette,data}.lua`.
- Lua reference: `XIUI/config/palettemanager.lua`.
- Lua reference: `XIUI/config/hotbar.lua: action editor and import dialog`.
- Lua reference: `XIUI/libs/ffxi/macros.lua`.
- Native integration: [src/runtime/settings/settings_profiles.hpp](../../src/runtime/settings/settings_profiles.hpp).
- Native integration: [src/runtime/settings/profile_share.cpp](../../src/runtime/settings/profile_share.cpp).
- Native integration: [src/runtime/hud_runtime_profiles.cpp](../../src/runtime/hud_runtime_profiles.cpp).

Lua paths are relative to the reference repository identified in the parity audit; brace lists name multiple files.

## Scope

- Named palette create/rename/copy/reorder/delete/cycle, global or job/subjob action storage, shared-subjob fallback and pet-aware overrides.
- A shared active palette for ordinary bars and an independent active crossbar palette, with supported per-bar command targeting.
- Macro editing/references and import of tHotBar/tCrossBar bindings using the Lua implementation as the behavior reference.

## Dependencies

P02 slot schema; P01 can supply pet identities, while palette storage must also work without a visible Pet HUD.

## Implementation

1. Create a versioned action-storage model keyed by character scope, job/subjob, palette ID, bank/bar and slot. Define pet override precedence and shared-subjob fallback as explicit lookup rules.
2. Build palette CRUD and cycling over that model. Keep at least one usable palette; copy actions when switching storage scope or require an explicit reviewed confirmation before clearing them.
3. Represent multi-line macros and references separately from slot display data. Route execution through P02 and retain native command timing/target semantics.
4. Inspect actual source import formats at implementation time. Parse supported literal data into a temporary model without executing Lua, preview conflicts/unsupported actions and import into a new palette or an explicitly chosen destination.
5. Add action payloads to portable profiles with schema/version limits. Preserve existing native share formats and the immutable XIUI2 dictionary; exclude transient bank/focus and local hardware settings.

## Input and controller behavior

Palette menus and macro editors use shared dialogs and controller navigation. Cancel preserves current assignments; deletion requires a fresh confirmation. Keyboard text entry is a valid first milestone.

## Completion criteria

- [x] Job/subjob/pet precedence is explicit and checked without overwriting other contexts.
- [x] Palette selection is separate for hotbars/crossbar; stable IDs survive renaming and reordering.
- [x] Import previews report unsupported records and reject executable expressions; source files remain untouched.
- [ ] Cody's live macro, job/pet and imported-layout acceptance.

## Focused validation

Focused precedence, CRUD, import-parser and profile round-trip fixtures; no automatic live macro execution.

Apply the [shared implementation contract](README.md#shared-implementation-contract), then update this plan and the audit when the feature is integrated.

