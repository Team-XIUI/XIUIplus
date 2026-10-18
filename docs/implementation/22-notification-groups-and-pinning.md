# P22: Notification groups and pinned invitations

Status: **Implemented** in the Native working tree, October 16, 2026. Appearance and gameplay acceptance remain with Cody.

[Parity audit](../feature-parity.md) · [Plan index](README.md)

## Usage

- **Notifications → General → Group count** enables 2–6 independent stacks. Existing events migrate to Group 1 with the original position, screen anchor, filters, colors and duration.
- **Events → Notification type / Assigned group** routes all nine kinds, including XP/limit points, skills and messages. Each active event has one destination. Inactive destinations temporarily use Group 1; re-enabling a group restores its preferred assignments.
- **Layout / Appearance / Animation / Text → Group** selects the stack to customize. Each retains its own position, width, stack/entry direction, anchor, row count/spacing, lifetime, rise, background, event colors and shared text profile. Dragging uses a manual position. Controller Movement uses the same registered adapters.
- **Animation → Pin invitations / Minify after** controls compact notices, initially after three seconds. Hover a notice for its sender, or use `/xiui focus notifications`; controller directions browse and Cancel leaves. Reading never accepts or declines an invitation.
- **Clear invitation notices** clears only the selected group's visual invitations.

## Lifetime and resolution

Party invitations keep the 90-second native window and clear when the SDK pending-invite flag ends, after the existing 250 ms admission grace. Trade clears on a matching native 0x0022 cancellation/completion/error response. Opening the exchange retains the notice until it ends. Trade also has a 30-second **presentation fallback**, not a claimed server expiry. Zoning, logout and character changes clear notices.

One source and one bounded 32-entry queue serve all groups. Stable event identities survive merging and routing changes. Party and trade each retain one slot, protected from reward overflow. A group can also show up to eight ordinary rows. Group 1 keeps original setting keys; additional settings use `notifications.group2`–`notifications.group6` and corresponding layout/text sections. `notifications.routing` stores kind assignments. Queues and inspection focus are transient.

## Source evidence

Lua reference: `XIUI/modules/notifications/{data,display}.lua`, `XIUI/handlers/notificationhandler.lua`, `XIUI/config/notifications.lua` and the group settings in `XIUI/core/settings/user.lua`, relative to the reference repository pinned in the audit.

Native integration: [notification center](../../src/runtime/notifications/notification_center.hpp), [renderer](../../src/runtime/notifications/notification_renderer.cpp), [event source](../../src/runtime/notifications/notification_source.cpp) and [module/group adapters](../../src/runtime/modules/notifications/notification_module.hpp).

## Completion criteria

- [x] Every event appears in its assigned group exactly once; routing changes preserve identity and age.
- [x] Minified party/trade slots are bounded and resolve through native observations or the documented fallback.
- [x] Legacy filters, colors, screen anchoring and positions survive migration.

## Focused validation

`xiui_config_input_smoke --notification-feedback-only` covers all nine kinds, routing without replay, inactive-group fallback/restoration, per-group expiry, queue overflow, native trade response bounds/identity, legacy settings/profile round trips, movement adapters, compact row sizing, shared pointer/controller details and focus release. Native gameplay appearance remains for Cody to check.
