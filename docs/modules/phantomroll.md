# Phantom Roll

Tracks observed rolls and Double-Up state, displaying configured rules, values and temporary feedback.

**Start in:** [phantom_roll_tracker.cpp](../../src/runtime/modules/phantomroll/phantom_roll_tracker.cpp), [phantom_roll_module.cpp](../../src/runtime/modules/phantomroll/phantom_roll_module.cpp), [phantom_roll_data.cpp](../../src/runtime/modules/phantomroll/phantom_roll_data.cpp).

**Keep intact:** Keep Retail/Horizon/Phoenix behavior in shared combat settings. Expire stale observations and deduplicate packets. Preview must not create gameplay state; estimates and unavailable potency modifiers remain explicit.

**Focused check:** `./scripts/check.ps1 -Name new-features` (roll cases are in `tests/phantom_roll_checks.cpp`).

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
