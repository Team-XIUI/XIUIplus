# Notifications

Typed, bounded notifications cover rewards, messages and invitations. Groups can move independently; pinned invitations expose details without answering them.

General → Enable Group 2 starts off. Disabled groups have no notifications, edit previews or movement handles; assignments fall back to Group 1 without discarding their preferred route. The old unused Group 2 default migrates off once, preserving existing Group 2 routes and later explicit choices.

**Start in:** [notification_center.cpp](../../src/runtime/notifications/notification_center.cpp), [notification_source.cpp](../../src/runtime/notifications/notification_source.cpp), [notification_module.cpp](../../src/runtime/modules/notifications/notification_module.cpp).

**Keep intact:** Use stable event identities, monotonic expiry, bounded coalescing and silent replay. Viewing or clearing an invitation must not accept/decline it. Groups retain normal text profiles and shared movement controls.

**Focused check:** `./scripts/check.ps1 -Name notification-feedback`; pure queue behavior uses `xiui_inventory_notifications_smoke`.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
