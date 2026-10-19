# Feature implementation plans

Read the audit and all plans together in the [combined HTML document](../feature-parity.html), with search, navigation and expandable sections.

These 27 implementation records accompany the [October 15 feature-parity audit](../feature-parity.md). P01–P25, P27 and P28 are implemented: 27 records and none pending. Physical-device/live action acceptance stays explicit. Treasure Pool supplies loot decisions while alerts stay in Notifications. Satchel includes browser, actions and cached inventories/slips. P28 adds a Native Minimap beyond the original Lua comparison. Record 26 was removed as requested; original numbering remains. P23 covers icons only. See [Action Bars](../actionbars.md), [Status Effects usage](../status-effects.md), [Satchel](../satchel.md) and [native mob-data findings](../native-mob-data-findings.md).

The audit's completed-work list also records Party screen-edge movement, auto-translated emote command routing and independent chat timestamp colors. These are completed fixes outside the original numbered backlog. The combined HTML uses green for implemented work and blue for pending/deferred work; unchecked gameplay acceptance stays explicit.

Lua source paths in each plan are relative to `C:/Users/Cody/Desktop/workspaces/ffxi/addons/XIUI`. Native links are relative to this repository. The audit pins both source baselines and counts current uncommitted Native code as present.

## Plan index

- [P01: Pet HUD](01-pet-hud.md) — Implemented; unavailable native lifetimes stay explicit.
- [P02: Action hotbars](02-action-hotbars.md) — Implemented; ten independent bars and shared action editor/executor.
- [P03: Controller crossbar](03-controller-crossbar.md) — Implemented; six trigger banks and native replacement; physical acceptance remains with Cody.
- [P04: Action palettes, macros and imports](04-action-palettes-and-imports.md) — Implemented; scoped/pet palettes, shared macros and previewed literal-data imports.
- [P05: Skillchain prediction and Magic Burst](05-skillchains-and-magic-burst.md) — Implemented; shared tracker, hotbar/crossbar predictions and an independently styled elemental burst HUD. Live combat acceptance remains with Cody.
- [P06: Mob information](06-mob-information.md) — Integrated into Target/child bars using the pinned HeadsUp Phoenix snapshot; no separate module.
- [P07: Treasure pool and lot/pass](07-treasure-pool.md) — Implemented; shared controller actions, estimated timers and existing Notifications alerts. Live loot acceptance remains with Cody.
- [P08: Satchel inventory browser](08-inventory-browser.md) — Implemented first pass; [usage](../satchel.md).
- [P09: Satchel item actions](09-inventory-item-actions.md) — Implemented first pass; live mutation acceptance remains with Cody.
- [P10: Offline inventories and storage slips](10-inventory-cache-and-storage-slips.md) — Implemented first pass; 28 slip catalogs and per-character local caches.
- [P11: Blue Magic learned alert](11-blue-magic-learned-alert.md) — Implemented under Extras.
- [P12: Phantom Roll tracker](12-phantom-roll-tracker.md) — Implemented under Extras; potency is labeled base-only.
- [P13: Native status data and observed effects](13-debuff-tracking-rules.md) — Implemented; duration formulas superseded.
- [P14: Status display controls and tooltips](14-status-controls-and-tooltips.md) — Implemented, including Party grouping and native Buffs/Debuffs modules.
- [P15: Enemy List columns and target layout](15-enemy-list-layout.md) — Implemented.
- [P16: Player resource display and state colors](16-player-resource-options.md) — Implemented.
- [P17: MP cost preview on resource bars](17-mp-cost-preview.md) — Implemented.
- [P18: Target display options and entity palettes](18-target-display-options.md) — Implemented.
- [P19: Independent Subtarget Bar](19-independent-subtarget-bar.md) — Implemented.
- [P20: Detached Target of Target window](20-detached-target-of-target.md) — Implemented.
- [P21: Shared contextual visibility](21-shared-visibility-rules.md) — Implemented.
- [P22: Notification groups and pinned invitations](22-notification-groups-and-pinning.md) — Implemented.
- [P23: Status and job icon themes](23-asset-theme-selection.md) — Implemented; background/cursor themes are out of scope.
- [P24: Per-character active profiles](24-per-character-profile-selection.md) — Implemented; live two-character acceptance remains with Cody.
- [P25: Damage and healing feedback](25-damage-and-healing-feedback.md) — Implemented.
- [P27: Ready Check sound selection](27-ready-check-sound-selection.md) — Implemented.
- [P28: Minimap](28-minimap.md) — Implemented; three shapes, Regular/Enhanced WebP art, native alignment, markers, per-zone hover zoom and independently positioned/styled Zoom text. Live acceptance remains with Cody.

## Shared implementation contract

- Read [AGENTS.md](../../AGENTS.md), [shared foundations](../foundations.md), the [controller support skill](../../.agents/skills/xiui-controller-support/SKILL.md) and [controller implementation status](../controller-implementation.md). Follow the current source when an older proposal differs from implemented behavior.
- Reuse module registration, config-section/option APIs, native DAT containers, shared meters, text profiles, icon caches and asynchronous settings writers. HUD text uses shared title/meter/caption roles with gradients and normal effects; state colors use TintTextProfile. Compact sizing retains effects. Configuration text has its own presentation.
- All interactive surfaces provide explicit enter, navigate, activate, details/adjust and cancel routes using the current normalized controller actions/focus graph. Pointer/keyboard/controller invoke the same validated operations. Display-only HUDs need no artificial focus stops.
- Honor native text/menu/subtarget/gameplay ownership and already-blocked input. Consume owned press/held/release consistently, never repeat destructive/send/activation actions from held Confirm, and release on lifecycle transitions. Keep controller callbacks outside the runtime mutex and use the current host mailbox.
- Preserve positions, styles, enable flags, drafts and custom settings. New fields use compatibility defaults or narrow versioned migrations; profile round trips include intended preferences. Keep character/device bindings, inventory caches and transient held/focus state out of portable keys. Preserve the immutable XIUI2 preset/dictionary.
- Distinguish observed values, estimates and unavailable values. Validate client identities and packet bounds; clear stale state on relevant lifecycle changes. Revalidate identity and action eligibility at activation.
- Cache resources outside rendering and bound transient collections/IO. Bundle approved artwork/data/audio and notices with the existing resource system. The release remains one dist/xiuinative.dll produced by scripts/package.ps1; optional user files must not become mandatory companion dependencies.
- Normal changes use the standalone build/install workflow. Runtime hot reload is available only in an explicitly selected isolated two-DLL development install; see [Development](../development.md). Host/shared-ABI changes require a coordinated pair in development; a runtime-only publish cannot establish compatibility with an older host.
- Add only focused checks that exercise meaningful state, ownership, identity, persistence or format boundaries for the implementation. Do not run full suites, repeated builds, asset sweeps or browser/gameplay checks for routine reversible work. Cody checks appearance/gameplay. A proposed validation checklist is not evidence it has run.
- Record completed scope and source references in the plan and [audit](../feature-parity.md). Keep partial work partial until the remaining criteria are met; do not mark future adapters, previews or synthetic-only input as proven native hardware/gameplay support.

## Dependency notes

- P02 supplies action execution/slots; P04 adds storage and imports; P03 uses both for crossbar interaction. P05 can deliver its standalone burst HUD before any hotbar.
- P08 supplies live item records/browser; P09 adds validated actions; P10 reuses the presentation for read-only cached characters/slips.
- P13 supplies native IDs/timers and confirmed untimed observations; P14 supplies shared layout/tooltips and grouping. P23 supplies optional icon artwork. Do not reintroduce fabricated durations or infer polarity from cancelability.
- P06 supplies optional mob levels to Target and its child views. P18/P19/P20 share the target rendering foundations.
- P21 supplies shared visibility for registered modules. New features should adopt it.


Native extension: [World Target Bar](../world-target-bar.md), with projected target HP and standard meter/text controls. This is outside the numbered Lua parity backlog.
