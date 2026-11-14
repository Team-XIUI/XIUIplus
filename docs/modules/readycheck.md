# Ready Check

Coordinates a bounded party/alliance readiness prompt using compatible party messages and a snapshot of the current roster.

**Start in:** [ready_check_module.cpp](../../src/runtime/modules/readycheck/ready_check_module.cpp), [ready_check_state.hpp](../../src/runtime/modules/readycheck/ready_check_state.hpp), [ready_check_sound.hpp](../../src/runtime/modules/readycheck/ready_check_sound.hpp).

**Keep intact:** Only current-roster replies count, duplicate answers do not increase totals, and completion/summary happens once. Cancellation, session changes and previews must not send a summary. Sound previews use the existing selector path.

**Focused check:** `./scripts/check.ps1 -Name new-features`; sound cases live in `tests/modules/ready_check_sound_checks.cpp`.

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
