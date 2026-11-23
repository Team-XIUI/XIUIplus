# Phantom Roll

Tracks observed rolls and Double-Up state, displaying configured rules, values and temporary feedback.

**Start in:** [phantom_roll_tracker.cpp](../../src/runtime/modules/phantomroll/phantom_roll_tracker.cpp), [phantom_roll_module.cpp](../../src/runtime/modules/phantomroll/phantom_roll_module.cpp), [phantom_roll_data.cpp](../../src/runtime/modules/phantomroll/phantom_roll_data.cpp).

**Keep intact:** Keep Retail/Horizon/Phoenix behavior in shared combat settings. Expire stale observations and deduplicate packets. Preview must not create gameplay state; estimates and unavailable potency modifiers remain explicit. A non-bust total adds the roll's job bonus while a main-party member in the zone has that main job (Retail from XIUI's Lua data, Phoenix from the pinned era sources, none on Horizon); preview samples never include it. A solo roller has no party and gets no job bonus. A subjob Corsair's value is scaled by subjob level over main level, rounded up, as in the pinned `applyRoll`. `/xiuiplus phantomroll status` prints each seat's potency line.

**Party rolls:** The two seats are the local player's own roll effects. Rolls that main-party members hold and the seats do not show are listed below them with a small die and a timer: a second Corsair's rolls, or ones the local Corsair rolled before moving away and replacing them. The server limits each recipient to two rolls per Corsair, so a replaced roll stays on members who were out of range. A local roll's party copy takes the seat's native deadline and keeps it after the seat is replaced; another Corsair's roll is estimated at five minutes from its action (`~`) unless the local player also holds it. Party status lists carry no timers but cover the whole zone, so they confirm who still holds a roll at any distance; a roll no member holds is removed. Members whose statuses are unavailable keep a timed roll until it runs out. Zoning clears totals and party timers.

**Focused check:** `./scripts/check.ps1 -Name new-features` (roll cases are in `tests/modules/phantom_roll_checks.cpp`).

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
