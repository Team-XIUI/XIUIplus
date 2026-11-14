# Blue Magic learned alert

Shows a temporary notification for newly learned Blue Magic. This is a passive display, not a spell selector.

**Start in:** [blue_magic_module.cpp](../../src/runtime/modules/bluemagic/blue_magic_module.cpp), [blue_magic_alert.hpp](../../src/runtime/modules/bluemagic/blue_magic_alert.hpp).

**Keep intact:** Retain duplicate/replay suppression and clear transient alerts on session changes. Edit previews must not fabricate learned spells or gameplay events.

**Focused check:** `./scripts/check.ps1 -Name new-features` (Blue Magic cases are in `tests/modules/blue_magic_checks.cpp`).

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
