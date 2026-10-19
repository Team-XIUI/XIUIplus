# Status Effects

Renders independent local Buffs/Debuffs and shared status strips/tooltips for other HUD modules.

**Start in:** [status_effects_module.cpp](../../src/runtime/modules/status/status_effects_module.cpp), [status_strip.hpp](../../src/runtime/ui/status_strip.hpp), [status_durations.hpp](../../src/runtime/combat/status_durations.hpp).

**Keep intact:** Distinguish native timers from observed duration estimates. Do not fabricate arbitrary remote remaining time or unseen effects. Stable effect identities preserve inspection focus; hover/browsing never cancels an effect implicitly. Explicit local Confirm uses native cancellation flags and rechecks player/effect identity; remote inspection remains read-only.

**Focused check:** `./scripts/check.ps1 -Name status-snapshots`; display fixtures are in `tests/status_display_checks.cpp`.

Cancellation: `xiui_chat_smoke --status-cancel-only` checks native requests with a fake packet manager and never removes live effects.

[Detailed guide](../reference/status-effects.md) · [Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
